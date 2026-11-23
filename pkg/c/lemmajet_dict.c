/*
 * LemmaJet Dictionary Lookup Registry
 *
 * Maps (lang, mode, profile) tuples to modular per-model accessors in liblemmajet.a.
 * Compile-time subsetting is controlled via LEMMAJET_ENABLE_<LANG>_<PROFILE> macros.
 */
#include "lemmajet_dict.h"

#include <stddef.h>
#include <string.h>

static int lj_dict_strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && *s2) {
        int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (*s1 + 32) : (unsigned char)*s1;
        int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (*s2 + 32) : (unsigned char)*s2;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/* Modular model accessors (linked from separate .o files in liblemmajet.a) */
#ifdef LEMMAJET_DISABLE_STEM
#define LJ_DECL_PAIR(lang, profile) \
    extern const lemmajet_model_t* lj_get_##lang##_lemma_##profile(void);
#else
#define LJ_DECL_PAIR(lang, profile) \
    extern const lemmajet_model_t* lj_get_##lang##_stem_##profile(void); \
    extern const lemmajet_model_t* lj_get_##lang##_lemma_##profile(void);
#endif

#ifdef LEMMAJET_ENABLE_PL_SM_BALANCED
LJ_DECL_PAIR(pl, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_PL_MD_BALANCED
LJ_DECL_PAIR(pl, md_balanced)
#endif

#ifdef LEMMAJET_ENABLE_EN_XS_BALANCED
LJ_DECL_PAIR(en, xs_balanced)
#endif
#ifdef LEMMAJET_ENABLE_EN_SM_BALANCED
LJ_DECL_PAIR(en, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_EN_MD_BALANCED
LJ_DECL_PAIR(en, md_balanced)
#endif

#ifdef LEMMAJET_ENABLE_DE_SM_BALANCED
LJ_DECL_PAIR(de, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_DE_MD_BALANCED
LJ_DECL_PAIR(de, md_balanced)
#endif

#ifdef LEMMAJET_ENABLE_ES_SM_BALANCED
LJ_DECL_PAIR(es, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_ES_MD_BALANCED
LJ_DECL_PAIR(es, md_balanced)
#endif

#ifdef LEMMAJET_ENABLE_FR_SM_BALANCED
LJ_DECL_PAIR(fr, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_FR_MD_BALANCED
LJ_DECL_PAIR(fr, md_balanced)
#endif

#ifdef LEMMAJET_ENABLE_CS_SM_BALANCED
LJ_DECL_PAIR(cs, sm_balanced)
#endif
#ifdef LEMMAJET_ENABLE_CS_MD_BALANCED
LJ_DECL_PAIR(cs, md_balanced)
#endif

typedef const lemmajet_model_t* (*lj_model_getter_fn)(void);

typedef struct {
    const char* iso;
    const char* name;
    const char* profile;
    lj_model_getter_fn stem_fn;
    lj_model_getter_fn lemma_fn;
} lj_dict_entry_t;

#ifdef LEMMAJET_DISABLE_STEM
#define LJ_ENTRY(iso, name, profile) \
    {#iso, name, #profile, NULL, lj_get_##iso##_lemma_##profile}
#else
#define LJ_ENTRY(iso, name, profile) \
    {#iso, name, #profile, lj_get_##iso##_stem_##profile, lj_get_##iso##_lemma_##profile}
#endif

static const lj_dict_entry_t LJ_DICT_TABLE[] = {
#ifdef LEMMAJET_ENABLE_PL_SM_BALANCED
    LJ_ENTRY(pl, "polish", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_PL_MD_BALANCED
    LJ_ENTRY(pl, "polish", md_balanced),
#endif
#ifdef LEMMAJET_ENABLE_EN_XS_BALANCED
    LJ_ENTRY(en, "english", xs_balanced),
#endif
#ifdef LEMMAJET_ENABLE_EN_SM_BALANCED
    LJ_ENTRY(en, "english", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_EN_MD_BALANCED
    LJ_ENTRY(en, "english", md_balanced),
#endif
#ifdef LEMMAJET_ENABLE_DE_SM_BALANCED
    LJ_ENTRY(de, "german", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_DE_MD_BALANCED
    LJ_ENTRY(de, "german", md_balanced),
#endif
#ifdef LEMMAJET_ENABLE_ES_SM_BALANCED
    LJ_ENTRY(es, "spanish", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_ES_MD_BALANCED
    LJ_ENTRY(es, "spanish", md_balanced),
#endif
#ifdef LEMMAJET_ENABLE_FR_SM_BALANCED
    LJ_ENTRY(fr, "french", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_FR_MD_BALANCED
    LJ_ENTRY(fr, "french", md_balanced),
#endif
#ifdef LEMMAJET_ENABLE_CS_SM_BALANCED
    LJ_ENTRY(cs, "czech", sm_balanced),
#endif
#ifdef LEMMAJET_ENABLE_CS_MD_BALANCED
    LJ_ENTRY(cs, "czech", md_balanced),
#endif
    {NULL, NULL, NULL, NULL, NULL}
};

const lemmajet_dict_t* lemmajet_get_dict(const char* lang, const char* mode, const char* profile) {
    if (!lang || !*lang) return NULL;

    int is_lemma = 0;
    if (mode && *mode) {
        if (lj_dict_strcasecmp(mode, "lemma") == 0 || lj_dict_strcasecmp(mode, "lemmatize") == 0) {
            is_lemma = 1;
        } else if (lj_dict_strcasecmp(mode, "stem") == 0 || lj_dict_strcasecmp(mode, "stemmer") == 0) {
            is_lemma = 0;
        } else {
            return NULL;
        }
    }

    const char* eff_profile = (profile && *profile) ? profile : "md_balanced";
    char norm_profile[64];
    size_t plen = strlen(eff_profile);
    if (plen >= sizeof(norm_profile)) return NULL;
    for (size_t i = 0; i <= plen; i++) {
        norm_profile[i] = (eff_profile[i] == '-') ? '_' : eff_profile[i];
    }

    for (size_t i = 0; LJ_DICT_TABLE[i].iso != NULL; i++) {
        if ((lj_dict_strcasecmp(lang, LJ_DICT_TABLE[i].iso) == 0 ||
             lj_dict_strcasecmp(lang, LJ_DICT_TABLE[i].name) == 0) &&
            lj_dict_strcasecmp(norm_profile, LJ_DICT_TABLE[i].profile) == 0) {
            lj_model_getter_fn fn = is_lemma ? LJ_DICT_TABLE[i].lemma_fn : LJ_DICT_TABLE[i].stem_fn;
            return fn ? fn() : NULL;
        }
    }
    return NULL;
}

static const lemmajet_model_t* lj_snowball_lookup(const char* algorithm) {
    if (!algorithm) return NULL;
    char clean[64];
    size_t alen = strlen(algorithm);
    if (alen >= sizeof(clean)) return NULL;
    memcpy(clean, algorithm, alen + 1);

    const char* mode = "stem";
    if (alen > 6 && strcmp(clean + alen - 6, "_lemma") == 0) {
        mode = "lemma";
        clean[alen - 6] = '\0';
    }
    return lemmajet_get_dict(clean, mode, NULL);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void lj_dict_auto_init(void) {
    lemmajet_set_model_lookup(lj_snowball_lookup);
}
#endif
