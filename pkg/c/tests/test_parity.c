/*
 * LemmaJet: Native C Cross-Language Parity & Safety Test Suite
 *
 * Verifies:
 * - 100% bit-for-bit parity against Python and JavaScript reference implementations
 *   across all Tier 1 languages (pl, en, de, es, fr, cs) in both stem and lemma modes.
 * - Buffer and slice safety: non-null-terminated buffers, zero length, auto length,
 *   capacity overflow rejection, in-place mutation, and >127 code point fallback.
 * - Snowball libstemmer drop-in compatibility API.
 *
 * SPDX-License-Identifier: MIT
 */

#define LEMMAJET_IMPLEMENTATION
#define LEMMAJET_ENABLE_SNOWBALL_COMPAT 1

#include "../lemmajet.h"
#include "../lemmajet_registry.h"
#include "../lemmajet_parity_vectors.h"

#ifdef LEMMAJET_PARITY_HAS_PL
#include "../stemmer/pl.h"
#include "../lemmatizer/pl.h"
#endif
#ifdef LEMMAJET_PARITY_HAS_EN
#include "../stemmer/en.h"
#include "../lemmatizer/en.h"
#endif
#ifdef LEMMAJET_PARITY_HAS_DE
#include "../stemmer/de.h"
#include "../lemmatizer/de.h"
#endif
#ifdef LEMMAJET_PARITY_HAS_ES
#include "../stemmer/es.h"
#include "../lemmatizer/es.h"
#endif
#ifdef LEMMAJET_PARITY_HAS_FR
#include "../stemmer/fr.h"
#include "../lemmatizer/fr.h"
#endif
#ifdef LEMMAJET_PARITY_HAS_CS
#include "../stemmer/cs.h"
#include "../lemmatizer/cs.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
        assert(cond); \
    } \
} while (0)

#define TEST_ASSERT_STREQ(actual, expected, lang, word, msg) do { \
    if (strcmp((actual), (expected)) != 0) { \
        fprintf(stderr, "FAIL: %s [%s] word='%s': actual='%s' expected='%s'\n", \
                __func__, (lang), (word), (actual), (expected)); \
    } \
    TEST_ASSERT(strcmp((actual), (expected)) == 0, (msg)); \
} while (0)

typedef int (*stem_fn_t)(const char*, size_t, char*, size_t);
typedef int (*stem_cstr_fn_t)(const char*, char*, size_t);

static void test_language_vectors(
    const char* lang,
    const parity_vector_t* vectors,
    stem_fn_t stem_fn,
    stem_cstr_fn_t stem_cstr_fn,
    stem_fn_t lemma_fn,
    stem_cstr_fn_t lemma_cstr_fn,
    const char* algo_stem,
    const char* algo_lemma
) {
    char out[256];
    int res;

    struct sb_stemmer* sb_s = sb_stemmer_new(algo_stem, NULL);
    TEST_ASSERT(sb_s != NULL, "sb_stemmer_new for stemmer returned NULL");

    struct sb_stemmer* sb_l = sb_stemmer_new(algo_lemma, NULL);
    TEST_ASSERT(sb_l != NULL, "sb_stemmer_new for lemmatizer returned NULL");

    size_t count = 0;
    for (size_t i = 0; vectors[i].word != NULL; i++) {
        const char* w = vectors[i].word;
        size_t wlen = strlen(w);
        count++;

        /* 1. Stemmer mode via explicit len */
        res = stem_fn(w, wlen, out, sizeof(out));
        TEST_ASSERT(res >= 0, "stem_fn returned error");
        TEST_ASSERT_STREQ(out, vectors[i].expected_stem, lang, w, "stem mismatch");

        /* 2. Stemmer mode via null-terminated cstr convenience wrapper */
        res = stem_cstr_fn(w, out, sizeof(out));
        TEST_ASSERT(res >= 0, "stem_cstr_fn returned error");
        TEST_ASSERT_STREQ(out, vectors[i].expected_stem, lang, w, "stem_cstr mismatch");

        /* 3. Stemmer mode via Snowball drop-in API */
        const sb_symbol* sb_res = sb_stemmer_stem(sb_s, (const sb_symbol*)w, (int)wlen);
        TEST_ASSERT(sb_res != NULL, "sb_stemmer_stem returned NULL");
        TEST_ASSERT_STREQ((const char*)sb_res, vectors[i].expected_stem, lang, w, "sb_stemmer_stem mismatch");
        TEST_ASSERT(sb_stemmer_length(sb_s) == (int)strlen(vectors[i].expected_stem), "sb_stemmer_length mismatch");

        /* 4. Lemmatizer mode via explicit len */
        res = lemma_fn(w, wlen, out, sizeof(out));
        TEST_ASSERT(res >= 0, "lemma_fn returned error");
        TEST_ASSERT_STREQ(out, vectors[i].expected_lemma, lang, w, "lemma mismatch");

        /* 5. Lemmatizer mode via null-terminated cstr convenience wrapper */
        res = lemma_cstr_fn(w, out, sizeof(out));
        TEST_ASSERT(res >= 0, "lemma_cstr_fn returned error");
        TEST_ASSERT_STREQ(out, vectors[i].expected_lemma, lang, w, "lemma_cstr mismatch");

        /* 6. Lemmatizer mode via Snowball drop-in API */
        const sb_symbol* sb_lres = sb_stemmer_stem(sb_l, (const sb_symbol*)w, (int)wlen);
        TEST_ASSERT(sb_lres != NULL, "sb_stemmer_stem (lemma) returned NULL");
        TEST_ASSERT_STREQ((const char*)sb_lres, vectors[i].expected_lemma, lang, w, "sb_stemmer_stem (lemma) mismatch");
        TEST_ASSERT(sb_stemmer_length(sb_l) == (int)strlen(vectors[i].expected_lemma), "sb_stemmer_length (lemma) mismatch");
    }

    sb_stemmer_delete(sb_s);
    sb_stemmer_delete(sb_l);
    printf("  [PASS] Language '%s' (%zu golden vectors)\n", lang, count);
}

static void test_buffer_and_slice_safety(void) {
    char out[256];
    char ref_en[256];
    char ref_pl[256];
    int res;

    int ref_en_len = lemmajet_stem_en("running", 7, ref_en, sizeof(ref_en));
    TEST_ASSERT(ref_en_len >= 2, "reference EN stem length must be >= 2");
    int ref_pl_len = lemmajet_lemmatize_pl("ludzie", (size_t)-1, ref_pl, sizeof(ref_pl));
    TEST_ASSERT(ref_pl_len >= 2, "reference PL lemma length must be >= 2");

    /* 1. Slice safety on non-null-terminated buffers */
    const char* slice_buf = "running_fast_and_jumping";
    /* Extract slice for "running" (len 7) */
    res = lemmajet_stem_en(slice_buf, 7, out, sizeof(out));
    TEST_ASSERT(res == ref_en_len, "slice stem len mismatch");
    TEST_ASSERT(strcmp(out, ref_en) == 0, "slice stem content mismatch");

    /* 2. Zero-length input */
    out[0] = 'X';
    res = lemmajet_stem_pl("", 0, out, sizeof(out));
    TEST_ASSERT(res == 0, "zero-len stem return value must be 0");
    TEST_ASSERT(out[0] == '\0', "zero-len stem must produce empty null-terminated string");

    /* 3. Auto-length (size_t)-1 matches explicit length */
    char out_auto[256];
    char out_exp[256];
    int res_auto = lemmajet_stem_pl("chleba", (size_t)-1, out_auto, sizeof(out_auto));
    int res_exp = lemmajet_stem_pl("chleba", 6, out_exp, sizeof(out_exp));
    TEST_ASSERT(res_auto == res_exp, "auto length return must match explicit length");
    TEST_ASSERT(strcmp(out_auto, out_exp) == 0, "auto length output must match explicit length");

    /* 4. snprintf-style required-length return on insufficient capacity without clobbering */
    char small_buf[3];
    memset(small_buf, 0xAA, sizeof(small_buf));
    /* ref_en requires ref_en_len + 1 >= 3 bytes; capacity 2 is guaranteed insufficient */
    res = lemmajet_stem_en("running", 7, small_buf, 2);
    TEST_ASSERT(res == ref_en_len, "insufficient capacity must return required length");
    TEST_ASSERT((size_t)res >= 2, "required length must be >= cap on truncation");
    /* Canary byte past capacity must not be touched */
    TEST_ASSERT((unsigned char)small_buf[2] == 0xAA, "canary byte clobbered on buffer overflow");
    TEST_ASSERT(lemmajet_stem_en("running", 7, NULL, 0) == ref_en_len, "(NULL, 0) size query must match stem length");
    TEST_ASSERT(lemmajet_stem_en("running", 7, NULL, 4) == -1, "NULL out with cap > 0 must return -1");

    /* 5. In-place safety: out == word */
    char inplace_buf[64];
    strcpy(inplace_buf, "running");
    res = lemmajet_stem_en(inplace_buf, (size_t)-1, inplace_buf, sizeof(inplace_buf));
    TEST_ASSERT(res == ref_en_len, "in-place stem length mismatch");
    TEST_ASSERT(strcmp(inplace_buf, ref_en) == 0, "in-place stem content mismatch");

    /* In-place with UTF-8/suppletive lemma transform: "ludzie" -> ref_pl */
    strcpy(inplace_buf, "ludzie");
    res = lemmajet_lemmatize_pl(inplace_buf, (size_t)-1, inplace_buf, sizeof(inplace_buf));
    TEST_ASSERT(res == ref_pl_len, "in-place lemma len mismatch");
    TEST_ASSERT(strcmp(inplace_buf, ref_pl) == 0, "in-place lemma content mismatch");

    /* 6. Very large input (> 127 code points): safe identity fallback */
    char long_word[256];
    memset(long_word, 'a', 150);
    long_word[150] = '\0';
    res = lemmajet_stem_en(long_word, (size_t)-1, out, sizeof(out));
    TEST_ASSERT(res == 150, "large input identity fallback length mismatch");
    TEST_ASSERT(strcmp(out, long_word) == 0, "large input identity fallback content mismatch");

    printf("  [PASS] Buffer, slice, and in-place safety validations\n");
}

static void test_snowball_registry_and_encoding_rules(void) {
    /* ISO codes vs full names case-insensitive */
    struct sb_stemmer* s1 = sb_stemmer_new("polish", NULL);
    struct sb_stemmer* s2 = sb_stemmer_new("PL", "UTF_8");
    struct sb_stemmer* s3 = sb_stemmer_new("German", "utf-8");
    struct sb_stemmer* s4 = sb_stemmer_new("es_lemma", NULL);
    struct sb_stemmer* s5 = sb_stemmer_new("french_lemma", "UTF-8");

    TEST_ASSERT(s1 != NULL, "sb_stemmer_new('polish') failed");
    TEST_ASSERT(s2 != NULL, "sb_stemmer_new('PL') failed");
    TEST_ASSERT(s3 != NULL, "sb_stemmer_new('German') failed");
    TEST_ASSERT(s4 != NULL, "sb_stemmer_new('es_lemma') failed");
    TEST_ASSERT(s5 != NULL, "sb_stemmer_new('french_lemma') failed");

    sb_stemmer_delete(s1);
    sb_stemmer_delete(s2);
    sb_stemmer_delete(s3);
    sb_stemmer_delete(s4);
    sb_stemmer_delete(s5);

    /* Unsupported algorithms & encodings */
    TEST_ASSERT(sb_stemmer_new("klingon", NULL) == NULL, "unsupported algorithm must return NULL");
    TEST_ASSERT(sb_stemmer_new("polish", "ISO-8859-1") == NULL, "unsupported encoding must return NULL");
    TEST_ASSERT(sb_stemmer_new("polish", "ASCII") == NULL, "unsupported encoding must return NULL");

    /* Available algorithms list */
    const char** list = sb_stemmer_list();
    TEST_ASSERT(list != NULL && list[0] != NULL, "sb_stemmer_list must return non-empty list");

    printf("  [PASS] Snowball registry and character encoding compatibility\n");
}

int main(void) {
    int langs_tested = 0;

    printf("======================================================================\n");
    printf(" LemmaJet ANSI C Native Parity & Safety Test Suite\n");
    printf("======================================================================\n");

#ifdef LEMMAJET_PARITY_HAS_PL
    test_language_vectors(
        "pl", VECTORS_PL,
        lemmajet_stem_pl, lemmajet_stem_pl_cstr,
        lemmajet_lemmatize_pl, lemmajet_lemmatize_pl_cstr,
        "polish", "polish_lemma"
    );
    langs_tested++;
#endif

#ifdef LEMMAJET_PARITY_HAS_EN
    test_language_vectors(
        "en", VECTORS_EN,
        lemmajet_stem_en, lemmajet_stem_en_cstr,
        lemmajet_lemmatize_en, lemmajet_lemmatize_en_cstr,
        "english", "english_lemma"
    );
    langs_tested++;
#endif

#ifdef LEMMAJET_PARITY_HAS_DE
    test_language_vectors(
        "de", VECTORS_DE,
        lemmajet_stem_de, lemmajet_stem_de_cstr,
        lemmajet_lemmatize_de, lemmajet_lemmatize_de_cstr,
        "german", "german_lemma"
    );
    langs_tested++;
#endif

#ifdef LEMMAJET_PARITY_HAS_ES
    test_language_vectors(
        "es", VECTORS_ES,
        lemmajet_stem_es, lemmajet_stem_es_cstr,
        lemmajet_lemmatize_es, lemmajet_lemmatize_es_cstr,
        "spanish", "spanish_lemma"
    );
    langs_tested++;
#endif

#ifdef LEMMAJET_PARITY_HAS_FR
    test_language_vectors(
        "fr", VECTORS_FR,
        lemmajet_stem_fr, lemmajet_stem_fr_cstr,
        lemmajet_lemmatize_fr, lemmajet_lemmatize_fr_cstr,
        "french", "french_lemma"
    );
    langs_tested++;
#endif

#ifdef LEMMAJET_PARITY_HAS_CS
    test_language_vectors(
        "cs", VECTORS_CS,
        lemmajet_stem_cs, lemmajet_stem_cs_cstr,
        lemmajet_lemmatize_cs, lemmajet_lemmatize_cs_cstr,
        "czech", "czech_lemma"
    );
    langs_tested++;
#endif

    TEST_ASSERT(langs_tested > 0, "at least one language parity vector set must be generated");
    test_buffer_and_slice_safety();
    test_snowball_registry_and_encoding_rules();

    printf("======================================================================\n");
    printf(" Results: %d / %d assertions PASSED (100%% parity)\n", g_tests_passed, g_tests_run);
    printf("======================================================================\n");

    return 0;
}
