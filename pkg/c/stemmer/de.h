/*
 * LemmaJet: Ultra-fast ANSI C Language Root (DE)
 * Mode: stemmer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_STEMMER_DE_H
#define LEMMAJET_STEMMER_DE_H

#include "../lemmajet.h"
#include "de/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_de lemmajet_model_de_general_md_balanced_stem

static inline int lemmajet_stem_de(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_stem(&lemmajet_model_de, word, len, out, cap);
}

static inline int lemmajet_stem_de_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_stem_cstr(&lemmajet_model_de, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_STEMMER_DE_H */
