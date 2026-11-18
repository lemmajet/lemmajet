# LemmaJet (ANSI C)

Ultra-fast, zero-dependency ANSI C (C99/C23) and C++ micro-stemmers and micro-lemmatizers across 6 languages: Polish (`pl`), English (`en`), German (`de`), Spanish (`es`), French (`fr`), and Czech (`cs`).

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](lemmajet.h)
[![Website: lemmajet.org](https://img.shields.io/badge/Website-lemmajet.org-green.svg)](https://lemmajet.org)

---

## Key Features

- **Zero External Dependencies & Zero Heap Allocation**: Pure C99 single-header runtime ([`lemmajet.h`](lemmajet.h)) writing directly into caller-provided buffers (`out`, `cap`) with no `malloc`/`free` calls in the hot path.
- **STB-Style Single-Header or Compiled Library**: Include `lemmajet.h` directly in your project (defining `LEMMAJET_IMPLEMENTATION` in one translation unit), or build static (`liblemmajet.a`) and shared (`liblemmajet.so` / `liblemmajet.dylib`) libraries via `make` or `CMake`.
- **High-Throughput Runtime**: Single-pass UTF-8 Stage 0 with an inline pure-ASCII fast path, compile-time open-addressed FNV-1a exception hash tables (`len_mask` + byte blob), and byte-level UTF-8 prefix/suffix tries (`byte_cid[256]`).
- **Dual Operating Modes**:
  - **Stemmers** (`stemmer/<lang>.h`): Equivalence class reductions for high-recall BM25 / search indexing.
  - **Lemmatizers** (`lemmatizer/<lang>.h`): Canonical dictionary headwords for NLP, NER, and entity normalization.
- **Compact Double-Array Trie (`--compact`) Option**: Optionally pack suffix tries into 12-byte `lj_dat_node_t` Double-Array Trie tables, reducing `.rodata` footprint by **~17×** (`80.5 MB -> 4.68 MB` across all 12 default models).
- **Snowball `libstemmer` Drop-In Compatibility**: Opt-in `#define LEMMAJET_ENABLE_SNOWBALL_COMPAT 1` provides `sb_stemmer_new`, `sb_stemmer_stem`, `sb_stemmer_length`, `sb_stemmer_delete`, and `sb_stemmer_list`.

---

## Quickstart

### 1. Single-Header Usage (Default `md_balanced` Profile)

Define `LEMMAJET_IMPLEMENTATION` in **exactly one** `.c`/`.cpp` file before including `lemmajet.h`:

```c
#define LEMMAJET_IMPLEMENTATION
#include "lemmajet.h"
#include "stemmer/pl.h"
#include "lemmatizer/en.h"

#include <stdio.h>

int main(void) {
    char out[128];

    /* Stemmer (length-delimited or null-terminated) */
    int n1 = lemmajet_stem_pl_cstr("programistami", out, sizeof(out));
    if (n1 >= 0) printf("pl stem: %s (%d bytes)\n", out, n1);

    /* Lemmatizer */
    int n2 = lemmajet_lemmatize_en("went", 4, out, sizeof(out));
    if (n2 >= 0) printf("en lemma: %s (%d bytes)\n", out, n2);

    return 0;
}
```

### 2. Specific Pareto Profile

Include any specific profile header directly (`xxs`, `xs`, `sm`, `md`, `lg`, `xl` × `precision`, `balanced`, `recall`):

```c
#define LEMMAJET_IMPLEMENTATION
#include "lemmajet.h"
#include "stemmer/en/xl_recall.h"
#include "lemmatizer/de/lg_precision.h"

void example(void) {
    char out[128];
    lemmajet_stem(&lemmajet_model_en_general_xl_recall_stem, "relational", (size_t)-1, out, sizeof(out));
    lemmajet_lemmatize(&lemmajet_model_de_general_lg_precision_lemma, "Häuser", (size_t)-1, out, sizeof(out));
}
```

### 3. Snowball `libstemmer` Drop-In Compatibility

Define `LEMMAJET_ENABLE_SNOWBALL_COMPAT` (and include `lemmajet_registry.h` for automatic algorithm lookup by name or ISO code):

```c
#define LEMMAJET_IMPLEMENTATION
#define LEMMAJET_ENABLE_SNOWBALL_COMPAT 1
#include "lemmajet.h"
#include "lemmajet_registry.h"

#include <stdio.h>

int main(void) {
    lemmajet_registry_init(); /* Automatic via GCC/Clang constructor; call explicitly on MSVC */

    struct sb_stemmer* stm = sb_stemmer_new("polish", "UTF_8");
    const sb_symbol* res = sb_stemmer_stem(stm, (const sb_symbol*)"książkach", 11);
    printf("%.*s\n", sb_stemmer_length(stm), (const char*)res);
    sb_stemmer_delete(stm);

    /* Append "_lemma" (or compile with -DLEMMAJET_SNOWBALL_DEFAULT_LEMMATIZER) for lemmatizer mode */
    struct sb_stemmer* lem = sb_stemmer_new("en_lemma", "UTF_8");
    const sb_symbol* lres = sb_stemmer_stem(lem, (const sb_symbol*)"mice", 4);
    printf("%.*s\n", sb_stemmer_length(lem), (const char*)lres);
    sb_stemmer_delete(lem);
    return 0;
}
```

> [!NOTE]
> When linking against real Snowball `libstemmer.h` in the same binary, keep `LEMMAJET_ENABLE_SNOWBALL_COMPAT` **undefined** in translation units that `#include <libstemmer.h>` so `sb_symbol` and `sb_stemmer_*` symbols do not collide.

---

## Native API Reference

### Core FTS C ABI (`lemmajet.h` & `lemmajet_dict.h`)

- **Length-delimited UTF-8**: `lemmajet_stem` and `lemmajet_lemmatize` read strictly within `len` bytes (`word` need not be null-terminated unless `len == (size_t)-1`).
- **`snprintf`-style length return & `(NULL, 0)` size query**: Both functions return the required output byte length (excluding `'\0'`). If `ret >= (int)cap`, output is not written past `cap`—callers can use a stack buffer (`char out[256]`) and retry with a single heap allocation of `ret + 1` bytes, or pass `(NULL, 0)` to query the exact output size. Returns `-1` only on invalid arguments (`!m || !word || (cap > 0 && !out)`).
- **Dictionary Registry (`lemmajet_dict.h`)**: `lemmajet_get_dict(lang, mode, profile)` looks up compiled `static const lemmajet_dict_t*` models in `.rodata` (`lang` case-insensitive, `mode` = `"stem"`/`"lemma"`, `profile` defaulting to `"md_balanced"` with `'-'` normalized to `'_'`).

| Function | Description |
| :--- | :--- |
| `int lemmajet_stem(m, word, len, out, cap)` | Stems `len` bytes of `word` (`(size_t)-1` for `strlen`) into `out`. Returns required byte length (excluding `'\0'`), or `-1` on invalid arguments. |
| `int lemmajet_lemmatize(m, word, len, out, cap)` | Lemmatizes `word` into `out` with identical `snprintf`-style return semantics. |
| `int lemmajet_stem_cstr(m, word, out, cap)` | Convenience wrapper passing `len = (size_t)-1`. |
| `int lemmajet_lemmatize_cstr(m, word, out, cap)` | Convenience wrapper passing `len = (size_t)-1`. |
| `const lemmajet_dict_t* lemmajet_get_dict(lang, mode, profile)` | Looks up a compiled `static const` model by `(lang, mode, profile)` in [`lemmajet_dict.h`](lemmajet_dict.h). |
| `const lemmajet_model_t* lemmajet_lookup_model(alg)` | Looks up `"pl"`/`"polish"`, `"en_lemma"`, etc. in `lemmajet_registry.h`. |
| `void lemmajet_registry_init(void)` | Registers `lemmajet_lookup_model` for `sb_stemmer_new`. |

---

## Building, Testing & Benchmarking

All C sources compile cleanly under strict C99 flags (`-std=c99 -Wall -Wextra -Werror -pedantic -O3`).

### Using Make

```bash
cd pkg/c

# Build static (build/liblemmajet.a, -fPIC), shared (build/liblemmajet.dylib / .so), and CLI binary (build/lemmajet)
# Note: builds only the core C library + CLI with zero SQLite/PostgreSQL dependencies
make all

# Or build only the CLI demo binary
make cli

# Subset compiled languages/profiles in liblemmajet.a (reduces binary size)
make all LANGS="pl en" PROFILES="sm_balanced md_balanced"

# Build and run the native C parity test suite (tests/test_parity.c)
make test

# Build and run the Layer 2 ASan/UBSan FTS C ABI & UTF-8 edge-case test suite (tests/test_fts_abi.c)
make test-san

# Build and run the single-thread C microbenchmark (benchmarks/bench_c.c)
make bench

# Delegate to pkg/c/sqlite/Makefile to build the SQLite FTS5 extension (see pkg/c/sqlite/README.md)
make sqlite SQLITE_SRC=/path/to/sqlite-src LANGS="pl en" PROFILES="md_balanced"

# Remove build artifacts
make clean
```

### Using CMake

```bash
cd pkg/c
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/bench_c
```

Targets exported by [`CMakeLists.txt`](CMakeLists.txt):
- `lemmajet` (`INTERFACE` header-only target)
- `lemmajet_static` (`STATIC` library)
- `lemmajet_shared` (`SHARED` library)
- `test_parity` and `bench_c` executables

### Regenerating Model Headers (Dense vs `--compact` DAT)

From the repository root, rebuild generated C headers with [`scripts/build_pkg.py`](../../scripts/build_pkg.py):

```bash
# Default dense byte-trie headers (fastest single-model throughput)
uv run python scripts/build_pkg.py --target c --profile md_balanced

# Compact Double-Array Trie (DAT) headers (~17x smaller .rodata, ideal for multi-language binaries)
uv run python scripts/build_pkg.py --target c --profile md_balanced --compact
```

To run the Python-side C emitter parity and compiler test suite:

```bash
uv run pytest tests/test_c_emitter.py tests/test_build_pkg.py
```

### 4. Standalone CLI Utility (`build/lemmajet`)

`make all` (or `make cli`) also builds [`examples/lemmajet_cli.c`](examples/lemmajet_cli.c) into `build/lemmajet` — a zero-dependency stdin/stdout line filter and word tool linked against `liblemmajet.a`:

```bash
# Stream one word per line from stdin -> stdout
printf "ludzie\npsami\nchleba\n" | ./build/lemmajet -l pl -m lemma
# człowiek
# pies
# chleb

# Positional shorthand: <lang> [stem|lemma] [profile] [word ...]
./build/lemmajet pl lemma ludzie psami
printf "running\nbetter\nwent\n" | ./build/lemmajet en lemma

# Print tab-separated input<TAB>output and list compiled models
printf "häuser\ngroße\n" | ./build/lemmajet -l de -m lemma -v
./build/lemmajet --list
```

- Flags: `-l/--lang <lang>`, `-m/--mode <stem|lemma>` (default: `stem`), `-p/--profile <profile>` (default: `md_balanced`), `-v/--verbose`, `--list`, `-h/--help`.
- Build with additional profiles in `liblemmajet.a` using `make cli PROFILES="xs_balanced sm_balanced md_balanced"`.

---

## Supported Languages & Profiles

| Language Code | Language | Default Profile |
| :--- | :--- | :--- |
| `pl` | Polish | `md_balanced` |
| `en` | English | `md_balanced` |
| `de` | German | `md_balanced` |
| `es` | Spanish | `md_balanced` |
| `fr` | French | `md_balanced` |
| `cs` | Czech | `md_balanced` |

- **Sizes**: `xxs`, `xs`, `sm`, `md`, `lg`, `xl`
- **Variants**: `precision` ($F_{0.5}$), `balanced` ($F_{1.0}$), `recall` ($F_{2.0}$)

---

## Architecture & Optimization Internals

LemmaJet C models execute the universal 4-stage inference pipeline directly on UTF-8 bytes:
1. **Stage 0**: Single-pass UTF-8 lowercasing (`LJ_LOWER_MAP` covering `U+0000..U+058F`), inline FNV-1a hash accumulation, and `is_ascii` fast-path tracking.
2. **Stage 1 / 2.5 / 3.5**: $O(1)$ open-addressed FNV-1a exception & stem-bridge lookup with 64-bit `len_mask` early rejection.
3. **Stage 2 & Stage 3**: Byte-level UTF-8 prefix & reversed suffix tries (`byte_cid[256]`) — or optional `lj_dat_node_t` Double-Array Trie (`--compact`) — writing the stem directly into `out`.
4. **Stage 4**: Precomputed tail normalizer substitutions using arithmetic code-point length tracking.

See the bundled architecture guides for full design details and cross-regime benchmarks:
- [`Model Inference & Stem-Bridge Architecture`](docs/model-inference-and-bridge-architecture.md)
- [`ANSI C Emitter & Runtime Optimizations`](docs/c-emitter-optimizations.md)

---

## License

MIT License. Copyright (c) 2026 Michal Šrajer and LemmaJet Contributors. See [`lemmajet.h`](lemmajet.h) and [`lemmajet_registry.h`](lemmajet_registry.h).
