/*
 * LemmaJet: Ultra-fast ANSI C Language Root (EN)
 * Mode: stemmer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_STEMMER_EN_H
#define LEMMAJET_STEMMER_EN_H

#include "../lemmajet.h"
#include "en/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_en lemmajet_model_en_general_md_balanced_stem

static inline int lemmajet_stem_en(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_stem(&lemmajet_model_en, word, len, out, cap);
}

static inline int lemmajet_stem_en_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_stem_cstr(&lemmajet_model_en, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_STEMMER_EN_H */
