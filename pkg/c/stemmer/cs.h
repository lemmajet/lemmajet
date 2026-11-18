/*
 * LemmaJet: Ultra-fast ANSI C Language Root (CS)
 * Mode: stemmer
 * Default Profile: md_balanced
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_STEMMER_CS_H
#define LEMMAJET_STEMMER_CS_H

#include "../lemmajet.h"
#include "cs/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

#define lemmajet_model_cs lemmajet_model_cs_general_md_balanced_stem

static inline int lemmajet_stem_cs(const char* word, size_t len, char* out, size_t cap) {
    return lemmajet_stem(&lemmajet_model_cs, word, len, out, cap);
}

static inline int lemmajet_stem_cs_cstr(const char* word, char* out, size_t cap) {
    return lemmajet_stem_cstr(&lemmajet_model_cs, word, out, cap);
}

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_STEMMER_CS_H */
