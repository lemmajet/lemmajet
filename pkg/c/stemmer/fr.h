/*
 * LemmaJet: Ultra-fast ANSI C Language Root (FR)
 * Mode: stemmer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_STEMMER_FR_H
#define LEMMAJET_STEMMER_FR_H

#include "../lemmajet.h"
#include "fr/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_fr lemmajet_model_fr_general_md_balanced_stem

static inline int lemmajet_stem_fr(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_stem(&lemmajet_model_fr, word, len, out, cap);
}

static inline int lemmajet_stem_fr_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_stem_cstr(&lemmajet_model_fr, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_STEMMER_FR_H */
