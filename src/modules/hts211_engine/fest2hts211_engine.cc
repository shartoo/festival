/* ----------------------------------------------------------------- */
/*           The HMM-Based Speech Synthesis System (HTS)             */
/*           festopt_hts_engine developed by HTS Working Group       */
/*           http://hts-engine.sourceforge.net/                      */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2001-2010  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/*                2001-2008  Tokyo Institute of Technology           */
/*                           Interdisciplinary Graduate School of    */
/*                           Science and Engineering                 */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the HTS working group nor the names of its  */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission.   */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

/* standard C libraries */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include "festival.h"
#include <sstream>

/* header of hts_engine API */
#ifdef __cplusplus
extern "C" {
#endif
#include "HTS211_engine.h"
#include "HTS211_hidden.h"
#ifdef __cplusplus
}
#endif
/* Getfp: wrapper for fopen */
static FILE *Getfp(const char *name, const char *opt) {
  if (name == NULL) {
    return NULL;
  }
   FILE *fp = fopen(name, opt);

   if (fp == NULL) {
      std::cerr << "Getfp: Cannot open " << name << std::endl;
      festival_error();
   }
   return (fp);
}

static HTS211_Engine *engine = NULL;
static const char *cached_voice = NULL;

/* HTS211_Engine_save_label_ostream: output label with time */
static void HTS211_Engine_save_label_ostream(HTS211_Engine * engine, std::ostream &os)
{
   int i, j;
   int frame, state, duration;

   HTS211_Label *label = &engine->label;
   HTS211_SStreamSet *sss = &engine->sss;
   const int nstate = HTS211_ModelSet_get_nstate(&engine->ms);
   const double rate =
       engine->global.fperiod * 1e+7 / engine->global.sampling_rate;

   for (i = 0, state = 0, frame = 0; i < HTS211_Label_get_size(label); i++) {
      for (j = 0, duration = 0; j < nstate; j++)
         duration += HTS211_SStreamSet_get_duration(sss, state++);
      /* in HTK & HTS format */
      os << (int) (frame * rate) << " "
         << (int) ((frame + duration) * rate) << " "
         << HTS211_Label_get_string(label, i) << std::endl;
      frame += duration;
   }
}

/* HTS211_Engine_steal_speech_pointer: Steal wave from GStream to reduce
 * memory copying */
static short int * HTS211_Engine_steal_speech_pointer(HTS211_Engine * engine)
{
   short int * rawwave = (&engine->gss)->gspeech;
   (&engine->gss)->gspeech = NULL;
   return rawwave;
}

/* HTS211_Synthesize_Utt: generate speech from utt by using hts_engine API */
LISP HTS211_Synthesize_Utt(LISP utt)
{
   EST_Utterance *u = get_c_utt(utt);
   EST_Item *item = 0;
   LISP hts_engine_params = NIL;
   LISP hts_output_params = NIL;

   char *fn_ms_lf0 = NULL, *fn_ms_mcp = NULL, *fn_ms_dur = NULL;
   char *fn_ts_lf0 = NULL, *fn_ts_mcp = NULL, *fn_ts_dur = NULL;
   char *fn_ws_lf0[3], *fn_ws_mcp[3];
   char *fn_ms_gvl = NULL, *fn_ms_gvm = NULL;
   char *fn_ts_gvl = NULL, *fn_ts_gvm = NULL;
   char *fn_gv_switch = NULL;

  LISP label_string_list = NIL;
  const char **label_string_array = NULL;
  size_t i, numlabels = 0;

   FILE *labfp = NULL;
  FILE *lf0fp = NULL, *mcpfp = NULL, *rawfp = NULL, *durfp = NULL;

   int sampling_rate;
   int fperiod;
   double alpha;
   int stage;
   double beta;
   double uv_threshold;

   const char* current_voice = NULL;
   
   /* get current voice name */
   current_voice = get_c_string(siod_get_lval("current-voice", NULL));
   
   
   /* get params */
   hts_engine_params =
       siod_get_lval("hts_engine_params",
                     "festopt_hts_engine: no parameters set for module");
   hts_output_params =
       siod_get_lval("hts_output_params",
                     "festopt_hts_engine: no output parameters set for module");

   /* get model file names */
   fn_ms_dur = (char *) get_param_str("-md", hts_engine_params, "hts/lf0.pdf");
   fn_ms_mcp = (char *) get_param_str("-mm", hts_engine_params, "hts/mgc.pdf");
   fn_ms_lf0 = (char *) get_param_str("-mf", hts_engine_params, "hts/dur.pdf");
   fn_ts_dur =
       (char *) get_param_str("-td", hts_engine_params, "hts/tree-lf0.inf");
   fn_ts_mcp =
       (char *) get_param_str("-tm", hts_engine_params, "hts/tree-mgc.inf");
   fn_ts_lf0 =
       (char *) get_param_str("-tf", hts_engine_params, "hts/tree-dur.inf");
   fn_ws_mcp[0] =
       (char *) get_param_str("-dm1", hts_engine_params, "hts/mgc.win1");
   fn_ws_mcp[1] =
       (char *) get_param_str("-dm2", hts_engine_params, "hts/mgc.win2");
   fn_ws_mcp[2] =
       (char *) get_param_str("-dm3", hts_engine_params, "hts/mgc.win3");
   fn_ws_lf0[0] =
       (char *) get_param_str("-df1", hts_engine_params, "hts/lf0.win1");
   fn_ws_lf0[1] =
       (char *) get_param_str("-df2", hts_engine_params, "hts/lf0.win2");
   fn_ws_lf0[2] =
       (char *) get_param_str("-df3", hts_engine_params, "hts/lf0.win3");
   fn_ms_gvm =
       (char *) get_param_str("-cm", hts_engine_params, "hts/gv-mgc.pdf");
   fn_ms_gvl =
       (char *) get_param_str("-cf", hts_engine_params, "hts/gv-lf0.pdf");
   fn_ts_gvm =
       (char *) get_param_str("-em", hts_engine_params, "hts/tree-gv-mgc.inf");
   fn_ts_gvl =
       (char *) get_param_str("-ef", hts_engine_params, "hts/tree-gv-lf0.inf");
   fn_gv_switch =
       (char *) get_param_str("-k", hts_engine_params, "hts/gv-switch.inf");

  /* open input file pointers */
   labfp =
       Getfp(get_param_str("-labelfile", hts_output_params, NULL), "r");
  /* get input label as string list */
  label_string_list = 
       (LISP) get_param_lisp("-labelstring", hts_output_params, NULL);
  if (label_string_list != NULL) {
    numlabels = siod_llength(label_string_list);
    label_string_array = new const char*[numlabels];
    LISP b;
    for (i=0, b=label_string_list; b != NIL; b=cdr(b),i++)
      label_string_array[i] = get_c_string(car(b));
  }
   /* open output file pointers */
   rawfp = Getfp(get_param_str("-or", hts_output_params, NULL), "wb");
   lf0fp = Getfp(get_param_str("-of", hts_output_params, NULL), "wb");
   mcpfp = Getfp(get_param_str("-om", hts_output_params, NULL), "wb");
   durfp = Getfp(get_param_str("-od", hts_output_params, NULL), "wb");

   /* get other params */
   sampling_rate = (int) get_param_float("-s", hts_engine_params, 16000.0);
   fperiod = (int) get_param_float("-p", hts_engine_params, 80.0);
   alpha = (double) get_param_float("-a", hts_engine_params, 0.42);
   stage = (int) get_param_float("-g", hts_engine_params, 0.0);
   beta = (double) get_param_float("-b", hts_engine_params, 0.0);
   uv_threshold = (double) get_param_float("-u", hts_engine_params, 0.5);

   std::stringstream labelstream(std::stringstream::in|std::stringstream::out);

   /* initialize */
   /* If voice name has not changed, keep cached parameters and models */
   if ( cached_voice != NULL && current_voice != NULL && \
        strcmp(cached_voice, current_voice)==0 ) {
      HTS211_Engine_refresh(engine);
   } else {
      if (cached_voice != NULL)
           HTS211_Engine_clear(engine);
      HTS211_Engine_initialize(engine, 2);
      HTS211_Engine_set_sampling_rate(engine, sampling_rate);
      HTS211_Engine_set_fperiod(engine, fperiod);
      HTS211_Engine_set_alpha(engine, alpha);
      HTS211_Engine_set_gamma(engine, stage);
      HTS211_Engine_set_beta(engine, beta);
      HTS211_Engine_set_msd_threshold(engine, 1, uv_threshold);
      HTS211_Engine_set_audio_buff_size(engine, 0);

   /* load models */
   HTS211_Engine_load_duration_from_fn(engine, &fn_ms_dur, &fn_ts_dur, 1);
   HTS211_Engine_load_parameter_from_fn(engine, &fn_ms_mcp, &fn_ts_mcp, fn_ws_mcp,
                                     0, FALSE, 3, 1);
   HTS211_Engine_load_parameter_from_fn(engine, &fn_ms_lf0, &fn_ts_lf0, fn_ws_lf0,
                                     1, TRUE, 3, 1);
   HTS211_Engine_load_gv_from_fn(engine, &fn_ms_gvm, &fn_ts_gvm, 0, 1);
   HTS211_Engine_load_gv_from_fn(engine, &fn_ms_gvl, &fn_ts_gvl, 1, 1);
   HTS211_Engine_load_gv_switch_from_fn(engine, fn_gv_switch);
      cached_voice = current_voice;
}

   /* generate speech */
   if (u->relation("Segment")->first()) {       /* only if there segments */
    /* Load label from file pointer or from string */
      if ( labfp != NULL ) {
         HTS211_Engine_load_label_from_fp(engine, labfp);
    } else if ( label_string_array != NULL ) {
      HTS211_Engine_load_label_from_string_list(engine, label_string_array, numlabels);
    } else {
      std::cerr << "No input label specified" << std::endl;
      HTS211_Engine_refresh(engine);
      return utt;
    }
      HTS211_Engine_create_sstream(engine);
      HTS211_Engine_create_pstream(engine);
      HTS211_Engine_create_gstream(engine);
      if (rawfp != NULL)
         HTS211_Engine_save_generated_speech(engine, rawfp);
      if (durfp != NULL)
         HTS211_Engine_save_label(engine, durfp);
      HTS211_Engine_save_label_ostream(engine, labelstream);
      if (lf0fp != NULL)
         HTS211_Engine_save_generated_parameter(engine, lf0fp, 1);
      if (mcpfp != NULL)
         HTS211_Engine_save_generated_parameter(engine, mcpfp, 1);
   }

   /* free (keep models in cache) */
   /* HTS211_Engine_clear(engine); */

   /* close output file pointers */
   if (rawfp != NULL)
      fclose(rawfp);
   if (durfp != NULL)
      fclose(durfp);
   if (lf0fp != NULL)
      fclose(lf0fp);
   if (mcpfp != NULL)
      fclose(mcpfp);

   /* close input file pointers */
   if (labfp != NULL)
      fclose(labfp);

   /* Load back in the waveform */
   const int numsamples = HTS211_GStreamSet_get_total_nsample(&engine->gss);
   short int * rawwave = HTS211_Engine_steal_speech_pointer(engine);
   EST_Wave *w;
   if (u->relation("Segment")->first()) /* only if there segments */
   {
      w = new EST_Wave(numsamples, 1 /* one channel */,
                       rawwave, 0, sampling_rate, 1 /* deallocate when destroyed */);
   } else {
      w = new EST_Wave;
      w->resample(sampling_rate);
   }

   item = u->create_relation("Wave")->append();
   item->set_val("wave", est_val(w));

   /* Load back in the segment times */
   EST_TokenStream ts_label;
   ts_label.open(labelstream);
   EST_Relation *r = new EST_Relation;
   EST_Item *s,*o;
   ts_label.seek(0);
   r->load("tmp.lab", ts_label, "htk");

   for(o = r->first(), s = u->relation("Segment")->first() ;
       (o != NULL) && (s != NULL) ; o = o->next(), s = s->next() )
      if (o->S("name").before("+").after("-").matches(s->S("name")))
         s->set("end",o->F("end")); 
      else
         std::cerr << "HTS211_Synthesize_Utt: Output segment mismatch" << std::endl;

   delete r;
   HTS211_Engine_refresh(engine);
   ts_label.close();
  delete[] label_string_array;
   return utt;
}

void festival_hts211_engine_init(void)
{
   char buf[1024];
   engine = new HTS211_Engine;
   HTS211_get_copyright(buf);
   proclaim_module("hts211_engine", buf);


   festival_def_utt_module("HTS211_Synthesize", HTS211_Synthesize_Utt,
                           "(HTS211_Synthesis UTT)\n  Synthesize a waveform using the hts_engine and the current models");
}
