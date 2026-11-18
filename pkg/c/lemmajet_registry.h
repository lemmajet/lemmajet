/*
 * LemmaJet: ANSI C Language Registry
 * Maps algorithm names and ISO codes to default md_balanced stemmers and lemmatizers.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_REGISTRY_H
#define LEMMAJET_REGISTRY_H

#include "lemmajet.h"

#include "stemmer/cs/md_balanced.h"
#include "lemmatizer/cs/md_balanced.h"
#include "stemmer/de/md_balanced.h"
#include "lemmatizer/de/md_balanced.h"
#include "stemmer/en/md_balanced.h"
#include "lemmatizer/en/md_balanced.h"
#include "stemmer/es/md_balanced.h"
#include "lemmatizer/es/md_balanced.h"
#include "stemmer/fr/md_balanced.h"
#include "lemmatizer/fr/md_balanced.h"
#include "stemmer/pl/md_balanced.h"
#include "lemmatizer/pl/md_balanced.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    const lemmajet_model_t* stemmer;
    const lemmajet_model_t* lemmatizer;
} lemmajet_registry_entry_t;

static const lemmajet_registry_entry_t LJ_REGISTRY[] = {
    {"cs", &lemmajet_model_cs_general_md_balanced_stem, &lemmajet_model_cs_general_md_balanced_lemma},
    {"czech", &lemmajet_model_cs_general_md_balanced_stem, &lemmajet_model_cs_general_md_balanced_lemma},
    {"de", &lemmajet_model_de_general_md_balanced_stem, &lemmajet_model_de_general_md_balanced_lemma},
    {"german", &lemmajet_model_de_general_md_balanced_stem, &lemmajet_model_de_general_md_balanced_lemma},
    {"en", &lemmajet_model_en_general_md_balanced_stem, &lemmajet_model_en_general_md_balanced_lemma},
    {"english", &lemmajet_model_en_general_md_balanced_stem, &lemmajet_model_en_general_md_balanced_lemma},
    {"es", &lemmajet_model_es_general_md_balanced_stem, &lemmajet_model_es_general_md_balanced_lemma},
    {"spanish", &lemmajet_model_es_general_md_balanced_stem, &lemmajet_model_es_general_md_balanced_lemma},
    {"fr", &lemmajet_model_fr_general_md_balanced_stem, &lemmajet_model_fr_general_md_balanced_lemma},
    {"french", &lemmajet_model_fr_general_md_balanced_stem, &lemmajet_model_fr_general_md_balanced_lemma},
    {"pl", &lemmajet_model_pl_general_md_balanced_stem, &lemmajet_model_pl_general_md_balanced_lemma},
    {"polish", &lemmajet_model_pl_general_md_balanced_stem, &lemmajet_model_pl_general_md_balanced_lemma},
    {NULL, NULL, NULL}
};

static inline const lemmajet_model_t* lemmajet_lookup_model(const char* algorithm) {
    if (!algorithm) return NULL;
    int is_lemma_req = 0;
#ifdef LEMMAJET_SNOWBALL_DEFAULT_LEMMATIZER
    is_lemma_req = 1;
#endif

    char clean_name[64];
    size_t alen = strlen(algorithm);
    if (alen >= sizeof(clean_name)) return NULL;
    strcpy(clean_name, algorithm);

    /* Check for _lemma suffix */
    if (alen > 6 && strcmp(clean_name + alen - 6, "_lemma") == 0) {
        is_lemma_req = 1;
        clean_name[alen - 6] = '\0';
    }

    for (size_t i = 0; LJ_REGISTRY[i].name != NULL; i++) {
        if (lj_strcasecmp(clean_name, LJ_REGISTRY[i].name) == 0) {
            return is_lemma_req ? LJ_REGISTRY[i].lemmatizer : LJ_REGISTRY[i].stemmer;
        }
    }
    return NULL;
}

static inline void lemmajet_registry_init(void) {
    lemmajet_set_model_lookup(lemmajet_lookup_model);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void lj_auto_init_registry(void) {
    lemmajet_set_model_lookup(lemmajet_lookup_model);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_REGISTRY_H */
