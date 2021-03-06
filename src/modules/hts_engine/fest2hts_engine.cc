/* ----------------------------------------------------------------- */
/*           The HMM-Based Speech Synthesis Module for Festival      */
/*           "festopt_hts_engine" developed by HTS Working Group     */
/*           http://hts-engine.sourceforge.net/                      */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2001-2013  Nagoya Institute of Technology          */
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
/* Author: Sergio Oller <sergioller@gmail.com>                       */

/* standard C libraries */
#include <cstring>
#include "festival.h"

LISP HTS21_Synthesize_Utt(LISP utt);
LISP HTS211_Synthesize_Utt(LISP utt);
LISP HTS22_Synthesize_Utt(LISP utt);

/* HTS_Synthesize_Utt: generate speech from utt by using hts_engine API */
static LISP HTS_Synthesize_Utt(LISP utt) {
  LISP hts_engine_params = NIL;
  char *parameter = NULL;
  char *htsversion = NULL;   
  /* get params */
  hts_engine_params =
      siod_get_lval("hts_engine_params",
                    "festopt_hts_engine: no parameters set for module");
  

  /* Typical parameter from HTS-2.2 */
  htsversion = (char *) get_param_str("-htsversion",
                                       hts_engine_params,
                                       "");
  if (strcmp(htsversion,"2.2") == 0) {
    return HTS22_Synthesize_Utt(utt);
  } else if (strcmp(htsversion,"2.1.1") == 0) {
    return HTS211_Synthesize_Utt(utt);
  } else if (strcmp(htsversion, "2.1") == 0) {
    return HTS21_Synthesize_Utt(utt);
  }
  
  /* No htsversion, use heuristics */
  
  /* Typical parameter from HTS-2.2 */
  parameter = (char *) get_param_str("-m",
                                       hts_engine_params,
                                       NULL);
  if (parameter != NULL) {
    /* HTS-2.2 */
    return HTS22_Synthesize_Utt(utt);
  }
  
  /* Typical parameter from HTS-2.1.1 */
  parameter = (char *) get_param_str("-dm3",
                                       hts_engine_params,
                                       NULL);
  if (parameter != NULL) {
    /* HTS-2.1.1 */
    return HTS211_Synthesize_Utt(utt);
  }
  
  /* Typical parameter from HTS-2.1.1 */
  parameter = (char *) get_param_str("-dm3",
                                       hts_engine_params,
                                       NULL);
  if (parameter != NULL) {
    /* HTS-2.1.1 */
    return HTS211_Synthesize_Utt(utt);
  }

  /* HTS-2.1 */
  return HTS21_Synthesize_Utt(utt);
}
  

void festival_hts_engine_init(void) {
  proclaim_module("hts_engine");

  festival_def_utt_module("HTS_Synthesize", HTS_Synthesize_Utt,
                          "(HTS_Synthesis UTT)\n  Chooses the right HTS_Synthesis method and synthesizes a waveform using given models\n");
}
