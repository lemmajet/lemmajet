/*
 * LemmaJet SQLite FTS5 Tokenizer Extension
 *
 * Wraps SQLite's built-in unicode61 tokenizer with remove_diacritics=0,
 * passes through FTS5_TOKENIZE_PREFIX queries unmodified, and applies
 * LemmaJet zero-allocation stemming/lemmatization on each token.
 *
 * Supports both:
 *   1. Combined build (default):
 *        .load pkg/c/sqlite/build/lemmajet_fts5
 *        CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet pl lemma');
 *        CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet pl lemma sm_balanced');
 *   2. Per-model build (LJ_FTS5_ENTRY_NAME, LJ_FTS5_TOK_NAME, LJ_FTS5_MODEL_LANG, LJ_FTS5_MODEL_PROFILE):
 *        .load pkg/c/sqlite/build/lemmajet_fts5_pl_sm_balanced
 *        CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet_pl_sm_balanced lemma');
 *        CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet_pl_sm_balanced');
 */
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#include "fts5.h"
#include "lemmajet_dict.h"

#include <stdlib.h>
#include <string.h>

#ifndef LJ_FTS5_ENTRY_NAME
#define LJ_FTS5_ENTRY_NAME sqlite3_lemmajetfts_init
#endif

typedef struct {
    fts5_api *pFts5;
    void *pUnicodeData;
    fts5_tokenizer unicodeTok;
} LemmajetGlobal;

typedef struct {
    const lemmajet_dict_t *dict;
    int is_lemma;
    fts5_tokenizer unicodeTok;
    Fts5Tokenizer *pUnicodeSub;
} LemmajetTokenizer;

typedef struct {
    void *pCtx;
    int (*xToken)(void *, int, const char *, int, int, int);
    const lemmajet_dict_t *dict;
    int is_lemma;
    int is_prefix;
} LemmajetTokenCtx;

#if defined(LJ_FTS5_MODEL_LANG) && defined(LJ_FTS5_MODEL_PROFILE)
static int lj_fts5_strcasecmp(const char *s1, const char *s2) {
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

static int lj_fts5_is_mode(const char *arg) {
    if (!arg) return 0;
    return lj_fts5_strcasecmp(arg, "stem") == 0 ||
           lj_fts5_strcasecmp(arg, "stemmer") == 0 ||
           lj_fts5_strcasecmp(arg, "lemma") == 0 ||
           lj_fts5_strcasecmp(arg, "lemmatize") == 0;
}
#endif

static int lemmajet_token_cb(
    void *pCtx,
    int tflags,
    const char *pToken,
    int nToken,
    int iStart,
    int iEnd
) {
    LemmajetTokenCtx *ctx = (LemmajetTokenCtx *)pCtx;
    if (ctx->is_prefix || nToken <= 0) {
        return ctx->xToken(ctx->pCtx, tflags, pToken, nToken, iStart, iEnd);
    }

    char stack_buf[256];
    int n = ctx->is_lemma
        ? lemmajet_lemmatize(ctx->dict, pToken, (size_t)nToken, stack_buf, sizeof(stack_buf))
        : lemmajet_stem(ctx->dict, pToken, (size_t)nToken, stack_buf, sizeof(stack_buf));

    if (n >= 0 && (size_t)n < sizeof(stack_buf)) {
        return ctx->xToken(ctx->pCtx, tflags, stack_buf, n, iStart, iEnd);
    }

    if (n >= (int)sizeof(stack_buf)) {
        char *heap_buf = (char *)malloc((size_t)n + 1);
        if (!heap_buf) return SQLITE_NOMEM;
        int n2 = ctx->is_lemma
            ? lemmajet_lemmatize(ctx->dict, pToken, (size_t)nToken, heap_buf, (size_t)n + 1)
            : lemmajet_stem(ctx->dict, pToken, (size_t)nToken, heap_buf, (size_t)n + 1);
        int rc = (n2 >= 0 && n2 <= n)
            ? ctx->xToken(ctx->pCtx, tflags, heap_buf, n2, iStart, iEnd)
            : ctx->xToken(ctx->pCtx, tflags, pToken, nToken, iStart, iEnd);
        free(heap_buf);
        return rc;
    }

    return ctx->xToken(ctx->pCtx, tflags, pToken, nToken, iStart, iEnd);
}

static int lemmajet_fts5_create(
    void *pUserData,
    const char **azArg,
    int nArg,
    Fts5Tokenizer **ppOut
) {
    LemmajetGlobal *g = (LemmajetGlobal *)pUserData;
    if (!g) return SQLITE_ERROR;

    const char *lang = NULL;
    const char *mode = "stem";
    const char *profile = NULL;

#if defined(LJ_FTS5_MODEL_LANG) && defined(LJ_FTS5_MODEL_PROFILE)
    if (nArg == 0) {
        lang = LJ_FTS5_MODEL_LANG;
        mode = "stem";
        profile = LJ_FTS5_MODEL_PROFILE;
    } else if (nArg == 1 && lj_fts5_is_mode(azArg[0])) {
        lang = LJ_FTS5_MODEL_LANG;
        mode = azArg[0];
        profile = LJ_FTS5_MODEL_PROFILE;
    } else {
        lang = azArg[0];
        mode = (nArg >= 2) ? azArg[1] : "stem";
        profile = (nArg >= 3) ? azArg[2] : LJ_FTS5_MODEL_PROFILE;
    }
#else
    if (nArg < 1) return SQLITE_ERROR;
    lang = azArg[0];
    mode = (nArg >= 2) ? azArg[1] : "stem";
    profile = (nArg >= 3) ? azArg[2] : NULL;
#endif

    const lemmajet_dict_t *dict = lemmajet_get_dict(lang, mode, profile);
    if (!dict) return SQLITE_ERROR;

    const char *uni_args[2] = {"remove_diacritics", "0"};
    Fts5Tokenizer *pSub = NULL;
    int rc = g->unicodeTok.xCreate(g->pUnicodeData, uni_args, 2, &pSub);
    if (rc != SQLITE_OK) return rc;

    LemmajetTokenizer *tok = (LemmajetTokenizer *)sqlite3_malloc(sizeof(LemmajetTokenizer));
    if (!tok) {
        g->unicodeTok.xDelete(pSub);
        return SQLITE_NOMEM;
    }

    tok->dict = dict;
    tok->is_lemma = dict->is_lemmatizer ? 1 : 0;
    tok->unicodeTok = g->unicodeTok;
    tok->pUnicodeSub = pSub;
    *ppOut = (Fts5Tokenizer *)tok;
    return SQLITE_OK;
}

static void lemmajet_fts5_delete(Fts5Tokenizer *pTok) {
    LemmajetTokenizer *tok = (LemmajetTokenizer *)pTok;
    if (tok) {
        if (tok->pUnicodeSub && tok->unicodeTok.xDelete) {
            tok->unicodeTok.xDelete(tok->pUnicodeSub);
        }
        sqlite3_free(tok);
    }
}

static int lemmajet_fts5_tokenize(
    Fts5Tokenizer *pTok,
    void *pCtx,
    int flags,
    const char *pText,
    int nText,
    int (*xToken)(void *, int, const char *, int, int, int)
) {
    LemmajetTokenizer *tok = (LemmajetTokenizer *)pTok;
    LemmajetTokenCtx wrap;
    wrap.pCtx = pCtx;
    wrap.xToken = xToken;
    wrap.dict = tok->dict;
    wrap.is_lemma = tok->is_lemma;
    wrap.is_prefix = (flags & FTS5_TOKENIZE_PREFIX) ? 1 : 0;

    return tok->unicodeTok.xTokenize(
        tok->pUnicodeSub,
        (void *)&wrap,
        flags,
        pText,
        nText,
        lemmajet_token_cb
    );
}

static void lemmajet_global_destroy(void *pUserData) {
    sqlite3_free(pUserData);
}

static fts5_api *fts5_api_from_db(sqlite3 *db) {
    fts5_api *pRet = NULL;
    sqlite3_stmt *pStmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &pStmt, 0) == SQLITE_OK) {
        sqlite3_bind_pointer(pStmt, 1, (void *)&pRet, "fts5_api_ptr", 0);
        sqlite3_step(pStmt);
    }
    sqlite3_finalize(pStmt);
    return pRet;
}

static int lj_register_tokenizer(
    fts5_api *pFts5,
    const char *tok_name,
    void *pUnicodeData,
    fts5_tokenizer unicodeTok
) {
    LemmajetGlobal *g = (LemmajetGlobal *)sqlite3_malloc(sizeof(LemmajetGlobal));
    if (!g) return SQLITE_NOMEM;
    memset(g, 0, sizeof(*g));
    g->pFts5 = pFts5;
    g->pUnicodeData = pUnicodeData;
    g->unicodeTok = unicodeTok;

    fts5_tokenizer tok;
    tok.xCreate = lemmajet_fts5_create;
    tok.xDelete = lemmajet_fts5_delete;
    tok.xTokenize = lemmajet_fts5_tokenize;

    int rc = pFts5->xCreateTokenizer(pFts5, tok_name, (void *)g, &tok, lemmajet_global_destroy);
    if (rc != SQLITE_OK) {
        sqlite3_free(g);
    }
    return rc;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int LJ_FTS5_ENTRY_NAME(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
) {
    SQLITE_EXTENSION_INIT2(pApi);

    fts5_api *pFts5 = fts5_api_from_db(db);
    if (!pFts5) {
        if (pzErrMsg) *pzErrMsg = sqlite3_mprintf("lemmajet_fts5: FTS5 extension not available");
        return SQLITE_ERROR;
    }

    void *pUnicodeData = NULL;
    fts5_tokenizer unicodeTok;
    int rc = pFts5->xFindTokenizer(pFts5, "unicode61", &pUnicodeData, &unicodeTok);
    if (rc != SQLITE_OK) {
        if (pzErrMsg) *pzErrMsg = sqlite3_mprintf("lemmajet_fts5: unicode61 tokenizer not found");
        return rc;
    }

#ifdef LJ_FTS5_TOK_NAME
    rc = lj_register_tokenizer(pFts5, LJ_FTS5_TOK_NAME, pUnicodeData, unicodeTok);
    if (rc != SQLITE_OK) return rc;
#endif

    void *pDummyData = NULL;
    fts5_tokenizer dummyTok;
    if (pFts5->xFindTokenizer(pFts5, "lemmajet", &pDummyData, &dummyTok) != SQLITE_OK) {
        rc = lj_register_tokenizer(pFts5, "lemmajet", pUnicodeData, unicodeTok);
        if (rc != SQLITE_OK) return rc;
    }

    return SQLITE_OK;
}
