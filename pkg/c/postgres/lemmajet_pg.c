/*
 * LemmaJet PostgreSQL TEXT SEARCH Dictionary Extension (PGXS)
 *
 * Single macro-parameterized C source compiled once per language into
 * lemmajet_<lang>.so (e.g. lemmajet_pl.so, lemmajet_en.so).
 *
 * Required compile-time definitions:
 *   LJ_PG_LANG    - Language code string literal (e.g. "pl")
 *   LJ_PG_DINIT   - Init function symbol (e.g. lemmajet_pl_dinit)
 *   LJ_PG_DLEXIZE - Lexize function symbol (e.g. lemmajet_pl_dlexize)
 */

#include "postgres.h"

#include "commands/defrem.h"
#include "fmgr.h"
#include "nodes/pg_list.h"
#include "tsearch/ts_public.h"
#include "utils/builtins.h"

#include "lemmajet_dict.h"

#include <string.h>

#ifndef LJ_PG_LANG
#error "LJ_PG_LANG must be defined (e.g. -DLJ_PG_LANG='\"pl\"')"
#endif
#ifndef LJ_PG_DINIT
#error "LJ_PG_DINIT must be defined (e.g. -DLJ_PG_DINIT=lemmajet_pl_dinit)"
#endif
#ifndef LJ_PG_DLEXIZE
#error "LJ_PG_DLEXIZE must be defined (e.g. -DLJ_PG_DLEXIZE=lemmajet_pl_dlexize)"
#endif

PG_MODULE_MAGIC;

/* Two-level indirection ensures LJ_PG_* macros expand before ## token pasting. */
#define LJ_PG_FUNCTION_INFO_V1_EXP(fn) PG_FUNCTION_INFO_V1(fn)
#define LJ_PG_FUNCTION_INFO_V1(fn)     LJ_PG_FUNCTION_INFO_V1_EXP(fn)

LJ_PG_FUNCTION_INFO_V1(LJ_PG_DINIT);
LJ_PG_FUNCTION_INFO_V1(LJ_PG_DLEXIZE);

typedef struct
{
    const lemmajet_dict_t *dict;
    bool        is_lemma;
    bool        is_filter;
} DictLemmajet;

Datum
LJ_PG_DINIT(PG_FUNCTION_ARGS)
{
    List       *dictoptions = (List *) PG_GETARG_POINTER(0);
    ListCell   *l;
    const char *lang = LJ_PG_LANG;
    const char *mode = "lemma";
    const char *profile = "md_balanced";
    bool        is_filter = false;
    bool        lang_set = false;
    bool        mode_set = false;
    bool        profile_set = false;
    bool        filter_set = false;

    foreach(l, dictoptions)
    {
        DefElem    *defel = (DefElem *) lfirst(l);

        if (strcmp(defel->defname, "language") == 0)
        {
            if (lang_set)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("multiple Language parameters")));
            lang = defGetString(defel);
            lang_set = true;
        }
        else if (strcmp(defel->defname, "mode") == 0)
        {
            if (mode_set)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("multiple Mode parameters")));
            mode = defGetString(defel);
            mode_set = true;
        }
        else if (strcmp(defel->defname, "profile") == 0)
        {
            if (profile_set)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("multiple Profile parameters")));
            profile = defGetString(defel);
            profile_set = true;
        }
        else if (strcmp(defel->defname, "filter") == 0)
        {
            if (filter_set)
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("multiple Filter parameters")));
            is_filter = defGetBoolean(defel);
            filter_set = true;
        }
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("unrecognized lemmajet parameter: \"%s\"",
                            defel->defname)));
        }
    }

    const lemmajet_dict_t *dict = lemmajet_get_dict(lang, mode, profile);

    if (dict == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("lemmajet_%s: model not found for language=\"%s\", mode=\"%s\", profile=\"%s\"",
                        LJ_PG_LANG, lang, mode, profile)));
    }

    DictLemmajet *d = (DictLemmajet *) palloc0(sizeof(DictLemmajet));

    d->dict = dict;
    d->is_lemma = (dict->is_lemmatizer != 0);
    d->is_filter = is_filter;

    PG_RETURN_POINTER(d);
}

Datum
LJ_PG_DLEXIZE(PG_FUNCTION_ARGS)
{
    DictLemmajet *d = (DictLemmajet *) PG_GETARG_POINTER(0);
    char       *in = (char *) PG_GETARG_POINTER(1);
    int32       len = PG_GETARG_INT32(2);
    char        stack_buf[256];
    char       *out_lexeme;
    TSLexeme   *res;
    int         n;

    if (in == NULL || len <= 0)
        PG_RETURN_POINTER(NULL);

    /*
     * Stage 0 in lemmajet.h already folds UTF-8 case inline via LJ_LOWER_MAP,
     * and strictly honors (size_t) len without requiring NUL termination.
     */
    n = d->is_lemma
        ? lemmajet_lemmatize(d->dict, in, (size_t) len, stack_buf, sizeof(stack_buf))
        : lemmajet_stem(d->dict, in, (size_t) len, stack_buf, sizeof(stack_buf));

    if (n >= 0 && (size_t) n < sizeof(stack_buf))
    {
        out_lexeme = (char *) palloc((size_t) n + 1);
        memcpy(out_lexeme, stack_buf, (size_t) n);
        out_lexeme[n] = '\0';
    }
    else if (n >= (int) sizeof(stack_buf))
    {
        int         n2;

        out_lexeme = (char *) palloc((size_t) n + 1);
        n2 = d->is_lemma
            ? lemmajet_lemmatize(d->dict, in, (size_t) len, out_lexeme, (size_t) n + 1)
            : lemmajet_stem(d->dict, in, (size_t) len, out_lexeme, (size_t) n + 1);
        if (n2 < 0 || n2 > n)
        {
            pfree(out_lexeme);
            PG_RETURN_POINTER(NULL);
        }
        out_lexeme[n2] = '\0';
    }
    else
    {
        PG_RETURN_POINTER(NULL);
    }

    res = (TSLexeme *) palloc0(sizeof(TSLexeme) * 2);
    res[0].lexeme = out_lexeme;
    if (d->is_filter)
        res[0].flags = TSL_FILTER;

    PG_RETURN_POINTER(res);
}
