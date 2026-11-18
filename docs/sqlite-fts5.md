# LemmaJet SQLite FTS5 Extension Guide

Zero-dependency, offline-built SQLite FTS5 tokenizer extensions for LemmaJet stemmers and lemmatizers across 6 Tier 1 languages (`pl`, `en`, `de`, `es`, `fr`, `cs`).

---

## 1. Overview

`lemmajet_fts5` wraps SQLite's built-in `unicode61` tokenizer as a lightweight decorator:

- **Zero external dependencies**: Pure C99 (`pkg/c/sqlite/lemmajet_fts5.c`) linked against LemmaJet's `static const` `.rodata` automaton tables (`0.0 ms` startup, OS demand-paged).
- **Zero-allocation hot path**: Uses a `char stack_buf[256]` buffer via `lemmajet_stem()` / `lemmajet_lemmatize()` with `snprintf`-style length return and a single heap retry only for tokens `>= 256` bytes.
- **Diacritic preservation (`remove_diacritics=0`)**: Forces `unicode61` with `remove_diacritics=0` so Polish (`ą, ć, ę, ł, ń, ó, ś, ź, ż`), Czech (`č, ď, ě, ň, ř, š, ť, ů, ž`), German (`ä, ö, ü, ß`), Spanish, and French diacritics reach LemmaJet intact.
- **Prefix query passthrough (`FTS5_TOKENIZE_PREFIX`)**: Tokens flagged with `FTS5_TOKENIZE_PREFIX` (e.g. `MATCH 'mężczyz*'`) bypass stemming/lemmatization so prefix queries match inflected surface prefixes as expected.
- **Per-model & combined extensions**: Build a single combined `lemmajet_fts5.(dylib|so)` (subsettable via `LANGS`, `PROFILES`, or `MODELS`), or build collision-free per-model extensions (`lemmajet_fts5_<lang>_<profile>.(dylib|so)`) that can be `.load`-ed side-by-side in the same SQLite connection.

---

## 2. Building SQLite 3.54.1+ from Source

The `pkg/c/sqlite/Makefile` is strictly offline and requires `sqlite3ext.h` and `fts5.h` (provided by a local SQLite source tree or include directory).

### Tested Reproducible Build (`sqlite-src-3540100.zip`)

```bash
curl -LO https://www.sqlite.org/2026/sqlite-src-3540100.zip
unzip sqlite-src-3540100.zip
cd sqlite-src-3540100
./configure --enable-json --enable-load-extension --enable-fts5
make -j
```

This produces:
- Headers: `sqlite3ext.h` and `fts5.h` in `/path/to/sqlite-src-3540100`
- CLI binary: `/path/to/sqlite-src-3540100/sqlite3` (with FTS5 and `.load` enabled)

> **macOS System Python Note:** macOS system Python's built-in `sqlite3` module is compiled with `enable_load_extension == False` and cannot load `.dylib` extensions via `conn.enable_load_extension(True)`. For automated Python scripts and benchmarks (`pkg/c/sqlite/bench_fts5.py`), invoke the `sqlite3` CLI built above (via `--sqlite-src` or `--sqlite-cli`) rather than `import sqlite3`.

---

## 3. Building `lemmajet_fts5`

### Core C Library (Zero SQLite Dependency)

Running `make` in `pkg/c/` builds only the core library (`liblemmajet.a` and `liblemmajet.(dylib|so)`), with zero SQLite or PostgreSQL dependencies:

```bash
make -C pkg/c
```

### Per-Model SQLite Extension

Build a single `(lang, profile)` extension with its own unique SQLite entry point (`sqlite3_lemmajetfts<lang><profile>_init`) and tokenizer name (`lemmajet_<lang>_<profile>`):

```bash
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 model LANG=pl PROFILE=sm_balanced
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 model LANG=en PROFILE=md_balanced
```

To batch-build per-model extensions for multiple languages and profiles:

```bash
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 models LANGS="pl en" PROFILES="sm_balanced md_balanced"
```

### Combined Subsettable Extension

Build a single `lemmajet_fts5.(dylib|so)` containing only the languages and profiles you specify:

```bash
# Subset by LANGS x PROFILES
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 combined LANGS="pl en" PROFILES="sm_balanced md_balanced"

# Or specify exact lang:profile pairs
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 combined MODELS="pl:sm_balanced en:md_balanced"
```

You can also delegate from `pkg/c/` via:

```bash
make -C pkg/c sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 LANGS="pl en" PROFILES="md_balanced"
```

> **Note:** If headers are installed in a custom include directory instead of a source tree, pass `SQLITE_INC=/path/to/include` (and optionally `SQLITE_CLI=/path/to/sqlite3` for `make test`).

### Verify Multi-Extension Build

```bash
make -C pkg/c/sqlite SQLITE_SRC=/path/to/sqlite-src-3540100 test
```

---

## 4. Loading & SQL `tokenize` Syntax

### Option A: Single Combined Extension

Load `lemmajet_fts5` and pass `<lang> [mode] [profile]` to `tokenize`:
- `lang`: ISO code or name (`pl`, `en`, `de`, `es`, `fr`, `cs`, case-insensitive)
- `mode`: `stem` (default) or `lemma`
- `profile`: `xs_balanced`, `sm_balanced`, `md_balanced` (default: `md_balanced`; `-` normalized to `_`)

```sql
.load pkg/c/sqlite/build/lemmajet_fts5

CREATE VIRTUAL TABLE pl_docs USING fts5(
    body,
    tokenize = 'lemmajet pl lemma'
);

CREATE VIRTUAL TABLE en_docs USING fts5(
    body,
    tokenize = 'lemmajet en stem xs_balanced'
);
```

### Option B: Multiple Per-Model Extensions Side-by-Side

Each per-model `.dylib` / `.so` exports a unique entry point (`sqlite3_lemmajetfts<lang><profile>_init`), registers `lemmajet_<lang>_<profile>`, and guards the generic `lemmajet` alias via `xFindTokenizer` so multiple extensions load cleanly in the same database connection without symbol or tokenizer collisions:

```sql
.load pkg/c/sqlite/build/lemmajet_fts5_pl_sm_balanced
.load pkg/c/sqlite/build/lemmajet_fts5_en_md_balanced

CREATE VIRTUAL TABLE pl_docs USING fts5(
    body,
    tokenize = 'lemmajet_pl_sm_balanced lemma'
);

CREATE VIRTUAL TABLE en_docs USING fts5(
    body,
    tokenize = 'lemmajet_en_md_balanced lemma'
);

INSERT INTO pl_docs VALUES ('Dwaj mężczyźni szli drogą.'), ('Widziałem mężczyznę w ogrodzie.');
INSERT INTO en_docs VALUES ('The mice were running quickly.'), ('A small mouse hid.');

-- Inflected & lemma queries work out of the box
SELECT * FROM pl_docs WHERE pl_docs MATCH 'mężczyzna';
-- Prefix queries bypass lemmatization (FTS5_TOKENIZE_PREFIX)
SELECT * FROM pl_docs WHERE pl_docs MATCH 'mężczyz*';
-- Irregular English plural/singular
SELECT * FROM en_docs WHERE en_docs MATCH 'mouse';
```

### How `remove_diacritics=0` & `FTS5_TOKENIZE_PREFIX` Work

1. **`remove_diacritics=0`**: SQLite's default `unicode61` strips diacritics (`ą` → `a`, `ł` → `l`, `ż` → `z`) before passing tokens downstream. LemmaJet's models are trained on full UTF-8 surface forms, so `lemmajet_fts5` forces `remove_diacritics=0` when instantiating the underlying `unicode61` sub-tokenizer.
2. **`FTS5_TOKENIZE_PREFIX`**: When SQLite evaluates a prefix query (`MATCH 'mężczyz*'`), it invokes `xTokenize` with `FTS5_TOKENIZE_PREFIX`. `lemmajet_fts5` passes those query tokens through without altering them, preventing a lemmatizer or stemmer from rewriting a partial prefix into a full word that wouldn't match the index prefix.

---

## 5. Profile Selection & 6-Language Benchmarks

### Choosing a Profile

| Profile | Best For | Trade-off |
| :--- | :--- | :--- |
| `xs_balanced` | Ultra-compact English or embedded targets | Smallest binary (`~246 KB` `.dylib` for `en`), `35/35` EN smoke pass |
| `sm_balanced` | Balanced size & inflection recall (recommended default for `pl`, `es`, `cs` when binary size matters) | `2–5x` smaller `.dylib` than `md_balanced` with near-identical throughput |
| `md_balanced` | Maximum inflection accuracy across all 6 languages | Highest smoke pass rate (`10/10` on `de`/`cs`, `27/28` on `pl`, `9/10` on `es`, `35/35` on `en`) |

### Measured 6-Language SQLite FTS5 Benchmark (`bench_fts5.py`)

Measured via `uv run python3 pkg/c/sqlite/bench_fts5.py --sqlite-src /path/to/sqlite-src-3540100` on 4,000 documents per configuration (Apple Silicon macOS, `clang -O3`, SQLite 3.54.1):

| Lang | Config / Profile | Smoke Pass | `fts5vocab` Terms | Vocab Reduction vs `unicode61` | Throughput (`docs/sec`) | Index (`ms`) | Query (`ms`) | `.dylib` Size | Model `.o` Size |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `pl` | `unicode61` | 15 / 28 | 66 | 0.0% | 346,322 | 11.5 | 4.5 | — | — |
| `pl` | `lemmajet_sm_balanced` | 26 / 28 | 26 | **+60.6%** | 189,361 | 21.1 | 14.6 | 6.44 MB | 3.14 MB |
| `pl` | `lemmajet_md_balanced` | **27 / 28** | 29 | +56.1% | 196,787 | 20.3 | 14.7 | 29.64 MB | 12.45 MB |
| `en` | `unicode61` | 3 / 35 | 55 | 0.0% | 405,611 | 9.9 | 3.7 | — | — |
| `en` | `porter` | 12 / 35 | 50 | +9.1% | 430,428 | 9.3 | 10.3 | — | — |
| `en` | `lemmajet_xs_balanced` | **35 / 35** | 44 | +20.0% | 242,739 | 16.5 | 16.6 | 245.6 KB | 77.9 KB |
| `en` | `lemmajet_sm_balanced` | **35 / 35** | 43 | **+21.8%** | 244,246 | 16.4 | 17.5 | 616.5 KB | 217.7 KB |
| `en` | `lemmajet_md_balanced` | **35 / 35** | 43 | **+21.8%** | 255,289 | 15.7 | 16.3 | 1.42 MB | 508.9 KB |
| `de` | `unicode61` | 0 / 10 | 50 | 0.0% | 491,831 | 8.1 | 3.5 | — | — |
| `de` | `lemmajet_sm_balanced` | 9 / 10 | 50 | 0.0% | 364,214 | 11.0 | 4.1 | 1.26 MB | 642.0 KB |
| `de` | `lemmajet_md_balanced` | **10 / 10** | 50 | 0.0% | 361,331 | 11.1 | 4.6 | 3.37 MB | 1.59 MB |
| `es` | `unicode61` | 0 / 10 | 47 | 0.0% | 501,696 | 8.0 | 3.6 | — | — |
| `es` | `lemmajet_sm_balanced` | 8 / 10 | 43 | +8.5% | 375,343 | 10.7 | 4.6 | 4.43 MB | 1.77 MB |
| `es` | `lemmajet_md_balanced` | **9 / 10** | 42 | **+10.6%** | 340,845 | 11.7 | 6.0 | 33.01 MB | 11.15 MB |
| `fr` | `unicode61` | 0 / 10 | 49 | 0.0% | 510,543 | 7.8 | 3.5 | — | — |
| `fr` | `lemmajet_sm_balanced` | 4 / 10 | 46 | +6.1% | 368,559 | 10.9 | 4.5 | 2.08 MB | 921.0 KB |
| `fr` | `lemmajet_md_balanced` | **5 / 10** | 45 | **+8.2%** | 352,179 | 11.4 | 4.7 | 5.36 MB | 2.10 MB |
| `cs` | `unicode61` | 0 / 10 | 46 | 0.0% | 464,291 | 8.6 | 4.8 | — | — |
| `cs` | `lemmajet_sm_balanced` | 9 / 10 | 46 | 0.0% | 365,595 | 10.9 | 4.6 | 5.14 MB | 2.41 MB |
| `cs` | `lemmajet_md_balanced` | **10 / 10** | 46 | 0.0% | 368,624 | 10.9 | 5.0 | 19.89 MB | 9.24 MB |

> **Re-running the benchmark:**
> ```bash
> uv run python3 pkg/c/sqlite/bench_fts5.py --sqlite-src /path/to/sqlite-src-3540100 --json-out /tmp/lemmajet_fts5_bench.json
> ```
