/*
 * LemmaJet: Ultra-fast ANSI C Language Root (ES)
 * Mode: stemmer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_STEMMER_ES_H
#define LEMMAJET_STEMMER_ES_H

#include "../lemmajet.h"
#include "es/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_es lemmajet_model_es_general_md_balanced_stem

static inline int lemmajet_stem_es(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_stem(&lemmajet_model_es, word, len, out, cap);
}

static inline int lemmajet_stem_es_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_stem_cstr(&lemmajet_model_es, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_STEMMER_ES_H */
