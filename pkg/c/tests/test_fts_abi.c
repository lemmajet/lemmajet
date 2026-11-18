/*
 * Layer 2 ASan/UBSan C FTS ABI & UTF-8 Edge-Case Test Suite
 */
#define LEMMAJET_ENABLE_SNOWBALL_COMPAT
#include "lemmajet_dict.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_non_null_terminated_slice(const lemmajet_dict_t* m, const char* word, const char* expected) {
    size_t wlen = strlen(word);
    /* Allocate exact wlen bytes with NO trailing '\0' so ASan catches any 1-byte over-read */
    char* slice = (char*)malloc(wlen);
    assert(slice != NULL);
    memcpy(slice, word, wlen);

    char stack_buf[256];
    int n = lemmajet_lemmatize(m, slice, wlen, stack_buf, sizeof(stack_buf));
    assert(n >= 0 && (size_t)n < sizeof(stack_buf));
    assert(strlen(stack_buf) == (size_t)n);

    char ref_buf[256];
    int ref_n = lemmajet_lemmatize(m, word, wlen, ref_buf, sizeof(ref_buf));
    assert(ref_n == n);
    assert(strcmp(stack_buf, ref_buf) == 0);

    if (expected != NULL) {
        assert(strcmp(stack_buf, expected) == 0);
        assert((size_t)n == strlen(expected));
    }
    free(slice);
}

static void check_snprintf_retry(const lemmajet_dict_t* m, const char* word) {
    size_t wlen = strlen(word);
    char expected[256];
    int exp_n = lemmajet_lemmatize(m, word, wlen, expected, sizeof(expected));
    assert(exp_n > 0 && (size_t)exp_n < sizeof(expected));

    /* 1. Query size with NULL, 0 */
    int needed = lemmajet_lemmatize(m, word, wlen, NULL, 0);
    assert(needed == exp_n);

    /* 2. Tiny 2-byte buffer -> must not overflow and must return needed */
    char tiny[2] = {'X', 'Y'};
    int r2 = lemmajet_lemmatize(m, word, wlen, tiny, sizeof(tiny));
    if (needed >= (int)sizeof(tiny)) {
        assert(r2 == needed);
        assert(tiny[0] == 'X' && tiny[1] == 'Y');
    }

    /* 3. Exact buffer -> succeeds */
    char* exact = (char*)malloc((size_t)needed + 1);
    assert(exact != NULL);
    int r3 = lemmajet_lemmatize(m, word, wlen, exact, (size_t)needed + 1);
    assert(r3 == needed);
    assert(strcmp(exact, expected) == 0);
    free(exact);
}

static void check_long_token(const lemmajet_dict_t* m, size_t len, char fill) {
    char* tok = (char*)malloc(len);
    assert(tok != NULL);
    memset(tok, fill, len);

    char stack_buf[256];
    int n = lemmajet_lemmatize(m, tok, len, stack_buf, sizeof(stack_buf));
    assert(n == (int)len);
    assert((size_t)n >= sizeof(stack_buf));

    char* heap_buf = (char*)malloc((size_t)n + 1);
    assert(heap_buf != NULL);
    int n2 = lemmajet_lemmatize(m, tok, len, heap_buf, (size_t)n + 1);
    assert(n2 == (int)len);
    assert(memcmp(heap_buf, tok, len) == 0);
    assert(heap_buf[len] == '\0');

    free(heap_buf);
    free(tok);
}

static void check_passthrough_and_utf8(const lemmajet_dict_t* m, const char* input) {
    size_t len = strlen(input);
    char* slice = (char*)malloc(len);
    assert(slice != NULL);
    memcpy(slice, input, len);

    char out[256];
    int n = lemmajet_lemmatize(m, slice, len, out, sizeof(out));
    assert(n >= 0 && (size_t)n < sizeof(out));
    assert(strlen(out) == (size_t)n);
    free(slice);
}

int main(void) {
    /* 1. Registry lookup across all 6 Tier 1 languages */
    const lemmajet_dict_t* pl_lemma = lemmajet_get_dict("pl", "lemma", NULL);
    const lemmajet_dict_t* pl_stem  = lemmajet_get_dict("polish", "stem", "md-balanced");
    const lemmajet_dict_t* en_lemma = lemmajet_get_dict("EN", "LEMMA", "md_balanced");
    const lemmajet_dict_t* en_stem  = lemmajet_get_dict("en", NULL, NULL);
    const lemmajet_dict_t* de_lemma = lemmajet_get_dict("de", "lemma", NULL);
    const lemmajet_dict_t* es_lemma = lemmajet_get_dict("es", "lemma", NULL);
    const lemmajet_dict_t* fr_lemma = lemmajet_get_dict("fr", "lemma", NULL);
    const lemmajet_dict_t* cs_lemma = lemmajet_get_dict("cs", "lemma", NULL);
    assert(pl_lemma != NULL);
    assert(pl_stem != NULL);
    assert(en_lemma != NULL);
    assert(en_stem != NULL);
    assert(de_lemma != NULL);
    assert(es_lemma != NULL);
    assert(fr_lemma != NULL);
    assert(cs_lemma != NULL);
    assert(lemmajet_get_dict("xx", "lemma", NULL) == NULL);
    assert(lemmajet_get_dict("pl", "invalid_mode", NULL) == NULL);
    assert(lemmajet_get_dict("pl", "lemma", "xl_nonexistent") == NULL);

    /* 2. Length-delimited non-null-terminated UTF-8 slices & case folding */
    char pl_fold_ref[256];
    int pl_fold_n = lemmajet_lemmatize(pl_lemma, "mężczyzną", strlen("mężczyzną"), pl_fold_ref, sizeof(pl_fold_ref));
    assert(pl_fold_n > 0);
    char en_fold_ref[256];
    int en_fold_n = lemmajet_lemmatize(en_lemma, "universities", strlen("universities"), en_fold_ref, sizeof(en_fold_ref));
    assert(en_fold_n > 0);

    check_non_null_terminated_slice(pl_lemma, "mężczyznami", NULL);
    check_non_null_terminated_slice(pl_lemma, "rękach", NULL);
    check_non_null_terminated_slice(pl_lemma, "MĘŻCZYZNĄ", pl_fold_ref);
    check_non_null_terminated_slice(en_lemma, "found", NULL);
    check_non_null_terminated_slice(en_lemma, "UNIVERSITIES", en_fold_ref);
    check_non_null_terminated_slice(de_lemma, "Häusern", NULL);
    check_non_null_terminated_slice(es_lemma, "ciudades", NULL);
    check_non_null_terminated_slice(fr_lemma, "chevaux", NULL);
    check_non_null_terminated_slice(cs_lemma, "městech", NULL);

    /* 3. snprintf-style size query & retry */
    check_snprintf_retry(pl_lemma, "mężczyznami");
    check_snprintf_retry(en_lemma, "building");

    /* 4. Empty string & invalid args */
    char small[4];
    assert(lemmajet_lemmatize(pl_lemma, "", 0, small, sizeof(small)) == 0);
    assert(small[0] == '\0');
    assert(lemmajet_lemmatize(pl_lemma, "", 0, NULL, 0) == 0);
    assert(lemmajet_lemmatize(NULL, "abc", 3, small, sizeof(small)) == -1);
    assert(lemmajet_lemmatize(pl_lemma, "abc", 3, NULL, 4) == -1);

    /* 5. Long tokens (>256B and >512B) triggering heap retry */
    check_long_token(en_lemma, 300, 'a');
    check_long_token(pl_lemma, 700, 'z');

    /* 6. OOV, numbers, emojis, truncated UTF-8 */
    check_passthrough_and_utf8(en_lemma, "v1.2.3");
    check_passthrough_and_utf8(pl_lemma, "🚀test🚀");
    check_passthrough_and_utf8(pl_lemma, "🦀");
    /* Truncated 3-byte UTF-8 sequence (2 bytes only, no \0) */
    char bad_utf8[2] = {(char)0xE2, (char)0x82};
    char out_bad[32];
    int nb = lemmajet_lemmatize(pl_lemma, bad_utf8, 2, out_bad, sizeof(out_bad));
    assert(nb >= 0 && (size_t)nb < sizeof(out_bad));

    /* 7. Snowball libstemmer drop-in compatibility */
    char psami_ref[256];
    int psami_n = lemmajet_lemmatize(pl_lemma, "psami", 5, psami_ref, sizeof(psami_ref));
    assert(psami_n > 0);
    struct sb_stemmer* sb_pl = sb_stemmer_new("pl_lemma", "UTF_8");
    assert(sb_pl != NULL);
    const sb_symbol* sb_res = sb_stemmer_stem(sb_pl, (const sb_symbol*)"psami", 5);
    assert(sb_res != NULL);
    assert(sb_stemmer_length(sb_pl) == psami_n);
    assert(memcmp(sb_res, psami_ref, (size_t)psami_n) == 0);
    sb_stemmer_delete(sb_pl);

    struct sb_stemmer* sb_en = sb_stemmer_new("english", "UTF-8");
    assert(sb_en != NULL);
    sb_stemmer_delete(sb_en);

    printf("ALL LAYER 2 C FTS ABI & EDGE-CASE TESTS PASSED (ASan + UBSan clean)!\n");
    return 0;
}
