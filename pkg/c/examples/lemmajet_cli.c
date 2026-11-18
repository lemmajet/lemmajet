/*
 * LemmaJet: Zero-Dependency ANSI C99 Stdin/Stdout CLI Demo
 *
 * Interactive & pipeline utility showcasing liblemmajet.a + lemmajet_dict.h.
 * Reads one word per line from stdin (or words from argv) and writes the
 * stemmed or lemmatized output to stdout.
 *
 * SPDX-License-Identifier: MIT
 */

#include "../lemmajet_dict.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* prog) {
    printf(
        "Usage:\n"
        "  %s -l <lang> [-m stem|lemma] [-p <profile>] [-v] [word ...]\n"
        "  %s <lang> [stem|lemma] [profile]\n"
        "  %s --list\n"
        "\n"
        "Options:\n"
        "  -l, --lang <lang>       Language code or name (pl, en, de, es, fr, cs)\n"
        "  -m, --mode <mode>       Operating mode: stem (default) or lemma\n"
        "  -p, --profile <prof>    Compiled profile (default: md_balanced)\n"
        "  -v, --verbose           Print tab-separated '<word>\\t<output>'\n"
        "      --list              List models compiled into this binary\n"
        "  -h, --help              Show this help message\n"
        "\n"
        "Reads words line-by-line from stdin when no positional words are given.\n",
        prog,
        prog,
        prog
    );
}

static void list_compiled_models(void) {
    static const char* const langs[] = {"pl", "en", "de", "es", "fr", "cs", NULL};
    static const char* const profiles[] = {
        "xxs_precision", "xxs_balanced", "xxs_recall",
        "xs_precision",  "xs_balanced",  "xs_recall",
        "sm_precision",  "sm_balanced",  "sm_recall",
        "md_precision",  "md_balanced",  "md_recall",
        "lg_precision",  "lg_balanced",  "lg_recall",
        "xl_precision",  "xl_balanced",  "xl_recall",
        NULL
    };
    static const char* const modes[] = {"stem", "lemma", NULL};

    size_t count = 0;
    for (size_t li = 0; langs[li] != NULL; li++) {
        for (size_t pi = 0; profiles[pi] != NULL; pi++) {
            for (size_t mi = 0; modes[mi] != NULL; mi++) {
                if (lemmajet_get_dict(langs[li], modes[mi], profiles[pi]) != NULL) {
                    printf("%s\t%s\t%s\n", langs[li], modes[mi], profiles[pi]);
                    count++;
                }
            }
        }
    }
    if (count == 0) {
        fprintf(stderr, "No models compiled into this binary.\n");
    }
}

static int process_word(
    const lemmajet_dict_t* dict,
    int is_lemma,
    int verbose,
    const char* word,
    size_t len
) {
    char stack_out[256];
    const char* out_ptr = stack_out;
    char* heap_out = NULL;

    int needed = is_lemma
        ? lemmajet_lemmatize(dict, word, len, stack_out, sizeof(stack_out))
        : lemmajet_stem(dict, word, len, stack_out, sizeof(stack_out));
    if (needed < 0) {
        return -1;
    }

    if ((size_t)needed >= sizeof(stack_out)) {
        heap_out = (char*)malloc((size_t)needed + 1);
        if (!heap_out) {
            return -1;
        }
        int retry = is_lemma
            ? lemmajet_lemmatize(dict, word, len, heap_out, (size_t)needed + 1)
            : lemmajet_stem(dict, word, len, heap_out, (size_t)needed + 1);
        if (retry < 0) {
            free(heap_out);
            return -1;
        }
        out_ptr = heap_out;
    }

    if (verbose) {
        fwrite(word, 1, len, stdout);
        putchar('\t');
    }
    fwrite(out_ptr, 1, (size_t)needed, stdout);
    putchar('\n');
    free(heap_out);
    return 0;
}

static int read_dynamic_line(FILE* fp, char** buf, size_t* cap, size_t* out_len) {
    if (!*buf) {
        *cap = 512;
        *buf = (char*)malloc(*cap);
        if (!*buf) return -1;
    }

    size_t len = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            break;
        }
        if (len + 1 >= *cap) {
            size_t next_cap = (*cap) * 2;
            char* grown = (char*)realloc(*buf, next_cap);
            if (!grown) return -1;
            *buf = grown;
            *cap = next_cap;
        }
        (*buf)[len++] = (char)ch;
    }

    if (ch == EOF && len == 0) {
        return 0; /* Clean EOF */
    }
    if (len > 0 && (*buf)[len - 1] == '\r') {
        len--;
    }
    (*buf)[len] = '\0';
    *out_len = len;
    return 1;
}

static int ci_eq(const char* a, const char* b) {
    while (*a && *b) {
        int c1 = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : (unsigned char)*a;
        int c2 = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : (unsigned char)*b;
        if (c1 != c2) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int is_mode_token(const char* s) {
    return ci_eq(s, "stem") || ci_eq(s, "stemmer") ||
           ci_eq(s, "lemma") || ci_eq(s, "lemmatize");
}

static int is_profile_token(const char* s) {
    static const char* const suffixes[] = {"precision", "balanced", "recall", NULL};
    size_t slen = strlen(s);
    for (size_t i = 0; suffixes[i] != NULL; i++) {
        size_t xlen = strlen(suffixes[i]);
        if (slen > xlen + 1 &&
            (s[slen - xlen - 1] == '_' || s[slen - xlen - 1] == '-') &&
            ci_eq(s + slen - xlen, suffixes[i])) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* lang = NULL;
    const char* mode = "stem";
    const char* profile = "md_balanced";
    int verbose = 0;
    int list_only = 0;
    int used_flags = 0;
    int word_start = argc;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "--list") == 0) {
            list_only = 1;
            used_flags = 1;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose = 1;
            used_flags = 1;
        } else if ((strcmp(arg, "-l") == 0 || strcmp(arg, "--lang") == 0) && i + 1 < argc) {
            lang = argv[++i];
            used_flags = 1;
        } else if ((strcmp(arg, "-m") == 0 || strcmp(arg, "--mode") == 0) && i + 1 < argc) {
            mode = argv[++i];
            used_flags = 1;
        } else if ((strcmp(arg, "-p") == 0 || strcmp(arg, "--profile") == 0) && i + 1 < argc) {
            profile = argv[++i];
            used_flags = 1;
        } else if (strcmp(arg, "--") == 0) {
            word_start = i + 1;
            break;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "Unknown or incomplete option: %s\n", arg);
            print_usage(argv[0]);
            return 2;
        } else {
            if (!used_flags && !lang) {
                /* Positional form: lemmajet <lang> [stem|lemma] [profile] [word ...] */
                lang = argv[i++];
                if (i < argc && is_mode_token(argv[i])) {
                    mode = argv[i++];
                }
                if (i < argc && is_profile_token(argv[i])) {
                    profile = argv[i++];
                }
                if (i < argc) {
                    word_start = i;
                }
                break;
            } else {
                word_start = i;
                break;
            }
        }
    }

    if (list_only) {
        list_compiled_models();
        return 0;
    }

    if (!lang || !*lang) {
        print_usage(argv[0]);
        return 2;
    }

    const lemmajet_dict_t* dict = lemmajet_get_dict(lang, mode, profile);
    if (!dict) {
        fprintf(
            stderr,
            "Error: model not compiled/found for lang='%s' mode='%s' profile='%s'.\n"
            "Run '%s --list' to inspect available models.\n",
            lang,
            mode,
            profile,
            argv[0]
        );
        return 1;
    }

    int is_lemma = (ci_eq(mode, "lemma") || ci_eq(mode, "lemmatize"));

    if (word_start < argc) {
        for (int i = word_start; i < argc; i++) {
            if (process_word(dict, is_lemma, verbose, argv[i], strlen(argv[i])) != 0) {
                fprintf(stderr, "Error processing word '%s'\n", argv[i]);
                return 1;
            }
        }
        return 0;
    }

    char* line = NULL;
    size_t cap = 0;
    size_t len = 0;
    int status;
    while ((status = read_dynamic_line(stdin, &line, &cap, &len)) > 0) {
        if (process_word(dict, is_lemma, verbose, line, len) != 0) {
            fprintf(stderr, "Error processing line\n");
            free(line);
            return 1;
        }
        fflush(stdout);
    }
    free(line);
    if (status < 0) {
        fprintf(stderr, "Error reading from stdin\n");
        return 1;
    }
    return 0;
}
