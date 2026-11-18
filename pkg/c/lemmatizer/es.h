/*
 * LemmaJet: Ultra-fast ANSI C Language Root (ES)
 * Mode: lemmatizer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_LEMMATIZER_ES_H
#define LEMMAJET_LEMMATIZER_ES_H

#include "../lemmajet.h"
#include "es/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_es_lemma lemmajet_model_es_general_md_balanced_lemma

static inline int lemmajet_lemmatize_es(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_lemmatize(&lemmajet_model_es_lemma, word, len, out, cap);
}

static inline int lemmajet_lemmatize_es_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_lemmatize_cstr(&lemmajet_model_es_lemma, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_LEMMATIZER_ES_H */
