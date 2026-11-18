/*
 * LemmaJet Dictionary Registry API for FTS5 / PostgreSQL / C integrations.
 */
#ifndef LEMMAJET_DICT_H
#define LEMMAJET_DICT_H

#include "lemmajet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef lemmajet_model_t lemmajet_dict_t;

/**
 * Look up a compiled LemmaJet dictionary/model.
 *
 * @param lang    Language code or name (e.g. "pl", "polish", "en", "english"). Case-insensitive.
 * @param mode    "stem", "stemmer", "lemma", or "lemmatize" (NULL or "" defaults to "stem"). Case-insensitive.
 * @param profile Model profile (e.g. "md_balanced", "sm_balanced", "xs_balanced";
 *                NULL or "" defaults to "md_balanced"; '-' normalized to '_').
 * @return        Pointer to static const model in .rodata, or NULL if not compiled in / unknown.
 */
const lemmajet_dict_t* lemmajet_get_dict(const char* lang, const char* mode, const char* profile);

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_DICT_H */
