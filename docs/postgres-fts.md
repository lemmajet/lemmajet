# LemmaJet PostgreSQL `TEXT SEARCH` Extension (`PGXS`) Guide

Zero-dependency, per-language PostgreSQL `TEXT SEARCH TEMPLATE` extensions (`lemmajet_pl`, `lemmajet_en`, `lemmajet_de`, `lemmajet_es`, `lemmajet_fr`, `lemmajet_cs`) built via `PGXS` and multi-stage `postgres:17` Docker images.

---

## 1. Overview

Each `lemmajet_<lang>` extension wraps LemmaJet's ANSI C runtime (`pkg/c/lemmajet.h` and `pkg/c/lemmajet_dict.h`) in a native PostgreSQL `TEXT SEARCH TEMPLATE` (`pkg/c/postgres/lemmajet_pg.c`):

- **Fork-shared `static const` `.rodata` tables**: PostgreSQL spawns a separate OS process per client connection via `fork()`. Because LemmaJet compiles its suffix tries and exception maps into read-only `static const` `.rodata` sections, the OS kernel shares those pages across all backend workers with near-zero per-worker `Private_Dirty` RAM (`+104 KB` for `lemmajet_pl` vs `+28,224 KB` for `hunspell-pl`).
- **Zero-allocation hot path**: `dlexize` uses a `char stack_buf[256]` fast path with `snprintf`-style length return and falls back to `palloc` only for tokens `>= 256` bytes.
- **Stage 0 inline UTF-8 case folding**: `lemmajet_stem()` / `lemmajet_lemmatize()` fold uppercase UTF-8 code points inline via `LJ_LOWER_MAP[1424]` while preserving all Polish, Czech, German, Spanish, and French diacritics.
- **`TSL_FILTER` pipeline chaining**: Passing `Filter = true` to `CREATE TEXT SEARCH DICTIONARY` sets `TSL_FILTER` on the returned `TSLexeme`, allowing `lemmajet_<lang>` to lemmatize with full diacritics intact *before* passing the token downstream to `unaccent`.
- **`RTLD_GLOBAL` symbol isolation**: Each per-language `.so` compiles `lemmajet_dict_<lang>.o` with `-fvisibility=hidden` and links with `-Wl,-Bsymbolic` on Linux, so all 6 extensions load simultaneously in one backend with zero symbol collisions.

---

## 2. Quick Start with Docker (`postgres:17`)

The multi-stage `pkg/c/postgres/Dockerfile` supports `PG_MAJOR` (`15`, `16`, `17`), `LANGS`, and `PROFILES` build arguments:

- **`builder` stage**: Installs `build-essential` and `postgresql-server-dev-${PG_MAJOR}`, compiles `liblemmajet` objects with `-fPIC`, and builds/installs each `lemmajet_<lang>.so`.
- **`runtime` stage**: Copies only `lemmajet_*.so`, `lemmajet_*.control`, and `lemmajet_*--1.0.sql` into a clean `postgres:${PG_MAJOR}` image (zero compiler or `-dev` header bloat).
- **`bench` stage**: Extends `runtime` with `hunspell-pl` (UTF-8 converted via `iconv` for PostgreSQL `ispell` compatibility) for Polish benchmarking.

### Build & Run Smoke Suite

```bash
# Build lean runtime image with all 6 languages and run the 6-language PGXS smoke suite
make -C pkg/c/postgres pg-test

# Or build a minimal single-language runtime image (e.g., Polish only)
docker build -f pkg/c/postgres/Dockerfile --target runtime \
  --build-arg PG_MAJOR=17 \
  --build-arg LANGS="pl" \
  --build-arg PROFILES="sm_balanced md_balanced" \
  -t lemmajet-pg-pl:17 pkg/c
```

### Local Development with Docker Compose

```bash
docker compose -f pkg/c/postgres/docker-compose.yml up -d
docker compose -f pkg/c/postgres/docker-compose.yml exec postgres psql -U postgres -d lemmajet
```

Override `PG_MAJOR`, `LANGS`, `PROFILES`, or `PG_PORT` via environment variables as needed.

---

## 3. Bare-Metal Linux `PGXS` Build

### Prerequisites (Debian / Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential postgresql-server-dev-17
```

### Build & Install

The core `make -C pkg/c` target remains 100% free of PostgreSQL or SQLite dependencies. PostgreSQL extensions are built either from `pkg/c/postgres/` or via the `make -C pkg/c postgres` delegation target:

```bash
# Build and install Polish and English extensions into PostgreSQL's libdir/extension
sudo make -C pkg/c/postgres install LANGS="pl en" PROFILES="xs_balanced sm_balanced md_balanced"

# Or build via pkg/c delegation (without installing)
make -C pkg/c postgres LANGS="pl en"
```

> **LLVM / Bitcode Note (`with_llvm=no`):** Debian's `postgresql-server-dev-17` ships `PGXS` configured with LLVM bitcode generation enabled, which fails if `clang-19` is not installed. `pkg/c/postgres/Makefile` passes `with_llvm=no` automatically in recursive `PGXS` invocations so standard `gcc` builds succeed out of the box.

Run the 6-language smoke suite against a local PostgreSQL instance:

```bash
make -C pkg/c/postgres pg-test
```

---

## 4. Installing & Configuring Extensions (`CREATE EXTENSION lemmajet_<lang>`)

Each `CREATE EXTENSION lemmajet_<lang>;` installs three ready-to-use objects:

1. **`TEXT SEARCH TEMPLATE lemmajet_<lang>_template`**: Open template accepting custom dictionary options.
2. **`TEXT SEARCH DICTIONARY lemmajet_<lang>_dict`**: Default dictionary configured with `Mode = 'lemma'`, `Profile = 'md_balanced'`.
3. **`TEXT SEARCH CONFIGURATION lemmajet_<lang>`**: Copies `pg_catalog.simple` and maps `asciiword, asciihword, hword_asciipart, word, hword, hword_part` to `lemmajet_<lang>_dict`.

### Immediate Usage with `tsvector` + `GIN`

```sql
CREATE EXTENSION lemmajet_pl;
CREATE EXTENSION lemmajet_en;

CREATE TABLE documents (
    id bigserial PRIMARY KEY,
    body_pl text NOT NULL,
    tsv_pl tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_pl', body_pl)) STORED
);

CREATE INDEX documents_tsv_pl_gin ON documents USING gin (tsv_pl);

INSERT INTO documents (body_pl) VALUES
    ('Młodzi mężczyźni szli wąskimi drogami.'),
    ('Spotkaliśmy młodego mężczyznę w mieście.');

SELECT id, body_pl
FROM documents
WHERE tsv_pl @@ to_tsquery('lemmajet_pl', 'mężczyzna');
```

### Custom Dictionary Options

Create a custom dictionary from `lemmajet_<lang>_template` with any combination of:

| Parameter | Values | Default | Description |
| :--- | :--- | :--- | :--- |
| `Language` | `'pl'`, `'en'`, `'de'`, `'es'`, `'fr'`, `'cs'` | Extension's `<lang>` | Optional; must match the installed per-language extension if specified |
| `Mode` | `'lemma'`, `'stem'` *(opt-in)* | `'lemma'` | Full dictionary lemma (default) or equivalence-class stem (requires `WITH_STEM=1` at build time) |
| `Profile` | `'xs_balanced'` (`en` only), `'sm_balanced'`, `'md_balanced'` | `'md_balanced'` | Precompiled model size/accuracy profile |
| `Filter` | `true`, `false` | `false` | When `true`, returns `TSL_FILTER` so downstream dictionaries process the lemmatized token |

> **Default single-profile `lemma`-only build (`PROFILES="md_balanced"`, `WITH_STEM=0`):** By default, `pkg/c/postgres/Makefile` and `Dockerfile` compile only `md_balanced` in `lemma` mode into `lemmajet_<lang>.so` for the smallest shared-library footprint with the highest recall and lexeme reduction. If you need additional profiles (`xs_balanced`, `sm_balanced`) or `Mode = 'stem'`, opt in at build time:
>
> ```bash
> make -C pkg/c/postgres install PROFILES="xs_balanced sm_balanced md_balanced" WITH_STEM=1
> # or Docker:
> make -C pkg/c/postgres docker-build PROFILES="xs_balanced sm_balanced md_balanced" WITH_STEM=1
> ```

```sql
CREATE TEXT SEARCH DICTIONARY en_stem_xs (
    TEMPLATE = lemmajet_en_template,
    Mode = 'stem',
    Profile = 'xs_balanced'
);

CREATE TEXT SEARCH CONFIGURATION en_stem_cfg (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION en_stem_cfg
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH en_stem_xs;
```

---

## 5. Pipeline Chaining (`Filter = true` + `unaccent` & Stopwords)

### Accent-Insensitive Lemmatized Search (`Filter = true` + `unaccent`)

Stripping diacritics *before* lemmatization destroys morphological distinctions (e.g., Polish `ą`/`a`, `ę`/`e`, `ł`/`l`, `ż`/`z`). By setting `Filter = true` (`TSL_FILTER`), `lemmajet_<lang>` lemmatizes the surface form with diacritics intact (`mężczyznami` → `mężczyzna`) and then passes the lemma forward to PostgreSQL's `unaccent` dictionary (`mężczyzna` → `mezczyzna`):

```sql
CREATE EXTENSION IF NOT EXISTS lemmajet_pl;
CREATE EXTENSION IF NOT EXISTS unaccent;

CREATE TEXT SEARCH DICTIONARY lemmajet_pl_filter (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = 'md_balanced',
    Filter = true
);

CREATE TEXT SEARCH CONFIGURATION lemmajet_pl_unaccent (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION lemmajet_pl_unaccent
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH lemmajet_pl_filter, unaccent, simple;

-- Both accented and ASCII queries match all 14 Polish grammatical cases:
SELECT to_tsvector('lemmajet_pl_unaccent', 'Rozmawialiśmy z młodymi mężczyznami')
    @@ to_tsquery('lemmajet_pl_unaccent', 'mezczyzna');  -- returns true
```

### Stopword Chaining with `simple`

`lemmajet_<lang>_template` intentionally omits a built-in `StopWords` parameter. To filter stopwords, place PostgreSQL's built-in `simple` dictionary configured with `StopWords` ahead of `lemmajet_<lang>_dict` in the mapping chain:

```sql
CREATE TEXT SEARCH DICTIONARY en_stop_simple (
    TEMPLATE = pg_catalog.simple,
    StopWords = english
);

CREATE TEXT SEARCH CONFIGURATION en_lemma_stop (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION en_lemma_stop
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH en_stop_simple, lemmajet_en_dict;
```

---

## 6. Interactive Demo (`pg-demo.sh`)

Run a self-contained Docker demonstration against PostgreSQL 17 with a single command (builds `lemmajet-pg:17` if needed and removes the ephemeral container on exit):

```bash
bash pkg/c/postgres/pg-demo.sh
```

Skip the Docker image build if `lemmajet-pg:17` already exists:

```bash
bash pkg/c/postgres/pg-demo.sh --no-build
```

The script walks through:
1. **32-Variant English Tense & Voice Matrix**: Side-by-side recall (`simple` `0/32`, `english` Snowball `11/32`, `lemmajet_en` `32/32`).
2. **Over-Stemming Guard**: `universe`/`university`, `police`/`policy`, `organ`/`organization` (`english` Snowball produces false positives; `lemmajet_en` separates them cleanly).
3. **Polish 14-Case Declension + `Filter = true` + `unaccent`**: Both `mężczyzna` and ASCII `mezczyzna` match all 14 inflected cases.
4. **Native PostgreSQL FTS Features**: `ts_rank_cd()`, `ts_headline()`, `websearch_to_tsquery()`, and `EXPLAIN` confirming `GIN` index scan.
5. **GIN Indexing Throughput**: 16,000 English documents indexed with `simple`, `english` Snowball, and `lemmajet_en`.

---

## 7. 6-Language & `hunspell-pl` Benchmarks (`bench_pg.py`)

Measured with `uv run python3 pkg/c/postgres/bench_pg.py --no-build --json-out /tmp/lemmajet_pg_bench.json` inside PostgreSQL `17.12 (Debian 17.12-1.pgdg13+1)` (`--reps 500`: `7,000` Polish docs, `19,000` English docs, `5,000` docs each for `de`/`es`/`fr`/`cs`).

### Smoke Recall, `ts_stat()` Lexeme Reduction, GIN Throughput & `ts_rank_cd()` Latency

| Lang | Config | Smoke Hits | Overstem FP | `ts_stat()` Lexemes | Reduction vs `simple` | GIN Build (`ms`) | Throughput (`docs/s`) | GIN Size | `ts_rank_cd()` Mean (`ms`) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `pl` | `simple` | 1 / 14 | — | 68 | 0.0% | 34.02 | 205,761 | 144.0 KB | 0.201 |
| `pl` | `hunspell_pl` | 14 / 14 | — | 55 | 19.12% | 613.09 | 11,418 | 184.0 KB | 15.309 |
| `pl` | `lemmajet_pl_sm_balanced_lemma` | 14 / 14 | — | 30 | 55.88% | 28.51 | 245,511 | 120.0 KB | 5.243 |
| `pl` | `lemmajet_pl_md_balanced_lemma` | **14 / 14** *(14/14 unaccent)* | — | **27** | **60.29%** | 27.97 | 250,286 | 120.0 KB | 7.084 |
| `pl` | `lemmajet_pl_md_balanced_stem` | 14 / 14 | — | 31 | 54.41% | **27.25** | **256,908** | **112.0 KB** | 7.094 |
| `en` | `simple` | 0 / 32 | 0 / 3 | 55 | 0.0% | 97.25 | 195,377 | 456.0 KB | 0.217 |
| `en` | `english` (Snowball) | 11 / 32 | 2 / 3 | 35 | 36.36% | 114.01 | 166,659 | 320.0 KB | 9.935 |
| `en` | `lemmajet_en_xs_balanced_lemma` | 0 / 32 | 0 / 3 | 43 | 21.82% | **84.64** | **224,478** | 448.0 KB | 0.236 |
| `en` | `lemmajet_en_sm_balanced_lemma` | **32 / 32** | **0 / 3** | 43 | 21.82% | 85.07 | 223,345 | 448.0 KB | 27.220 |
| `en` | `lemmajet_en_md_balanced_lemma` | **32 / 32** | **0 / 3** | 43 | 21.82% | 86.72 | 219,107 | 448.0 KB | 27.516 |
| `en` | `lemmajet_en_md_balanced_stem` | **32 / 32** | **0 / 3** | 44 | 20.00% | 85.89 | 221,216 | 448.0 KB | 27.980 |
| `de` | `simple` | 0 / 10 | — | 50 | 0.0% | 18.61 | 268,742 | 72.0 KB | 0.531 |
| `de` | `german` (Snowball) | 7 / 10 | — | 37 | 26.00% | 21.89 | 228,403 | 56.0 KB | 0.232 |
| `de` | `lemmajet_de_sm_balanced_lemma` | 9 / 10 | — | 50 | 0.0% | 16.16 | 309,464 | 72.0 KB | 0.226 |
| `de` | `lemmajet_de_md_balanced_lemma` | **10 / 10** | — | 49 | 2.00% | 16.00 | 312,405 | 72.0 KB | 0.218 |
| `de` | `lemmajet_de_md_balanced_stem` | 7 / 10 | — | 50 | 0.0% | **15.30** | **326,817** | 72.0 KB | 0.223 |
| `es` | `simple` | 0 / 10 | — | 47 | 0.0% | 18.45 | 271,018 | 72.0 KB | 0.183 |
| `es` | `spanish` (Snowball) | 10 / 10 | — | 35 | 25.53% | 20.98 | 238,334 | 48.0 KB | 0.259 |
| `es` | `lemmajet_es_sm_balanced_lemma` | **10 / 10** | — | 42 | 10.64% | 14.75 | 339,005 | 72.0 KB | 0.211 |
| `es` | `lemmajet_es_md_balanced_lemma` | **10 / 10** | — | 42 | 10.64% | **14.28** | **350,115** | 72.0 KB | 0.211 |
| `es` | `lemmajet_es_md_balanced_stem` | 9 / 10 | — | 44 | 6.38% | 15.49 | 322,876 | 72.0 KB | 0.182 |
| `fr` | `simple` | 0 / 10 | — | 49 | 0.0% | 17.11 | 292,262 | 80.0 KB | 0.211 |
| `fr` | `french` (Snowball) | 9 / 10 | — | 37 | 24.49% | 24.46 | 204,450 | 56.0 KB | 0.396 |
| `fr` | `lemmajet_fr_sm_balanced_lemma` | **9 / 10** | — | 45 | 8.16% | 24.97 | 200,264 | 80.0 KB | 0.224 |
| `fr` | `lemmajet_fr_md_balanced_lemma` | **9 / 10** | — | 45 | 8.16% | 18.67 | 267,782 | 80.0 KB | 0.216 |
| `fr` | `lemmajet_fr_md_balanced_stem` | **9 / 10** | — | 46 | 6.12% | **16.08** | **310,982** | 80.0 KB | 0.207 |
| `cs` | `simple` | 0 / 10 | — | 46 | 0.0% | 17.12 | 292,021 | 64.0 KB | 0.199 |
| `cs` | `lemmajet_cs_sm_balanced_lemma` | 9 / 10 | — | 46 | 0.0% | 17.85 | 280,176 | 64.0 KB | 0.195 |
| `cs` | `lemmajet_cs_md_balanced_lemma` | **10 / 10** | — | 46 | 0.0% | 14.35 | 348,406 | 64.0 KB | 0.198 |
| `cs` | `lemmajet_cs_md_balanced_stem` | **10 / 10** | — | 45 | 2.17% | **13.95** | **358,318** | 72.0 KB | 0.208 |

### Compiled `.so` Sizes & Polish Per-Backend Worker RAM (`/proc/<pid>/smaps_rollup`)

Compiled `.so` sizes (all 3 profiles bundled per language for `en`; `sm_balanced` + `md_balanced` for `pl`/`de`/`es`/`fr`/`cs`):

| Extension | `.so` Size |
| :--- | :---: |
| `lemmajet_en.so` | 633,032 B (618.2 KB) |
| `lemmajet_fr.so` | 1,286,368 B (1.23 MB) |
| `lemmajet_de.so` | 1,745,120 B (1.66 MB) |
| `lemmajet_pl.so` | 2,860,136 B (2.73 MB) |
| `lemmajet_cs.so` | 2,990,304 B (2.85 MB) |
| `lemmajet_es.so` | 3,121,376 B (2.98 MB) |

Per-backend worker memory & cold-start latency (`lemmajet_pl` vs `hunspell_pl` on Polish):

| Config | Cold Start (`ms`) | `Rss` (`KB`) | `Pss` (`KB`) | `Shared_Clean` (`KB`) | `Private_Dirty` (`KB`) | Delta `Private_Dirty` vs `simple` |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `simple` | 0.41 | 16,724 | 8,365 | 6,748 | 3,480 | 0 KB |
| `lemmajet_pl` | **0.47** | 20,192 | 10,333 | 9,388 | 3,584 | **+104 KB** |
| `hunspell_pl` | 154.82 | 45,004 | 36,618 | 6,812 | 31,704 | +28,224 KB |

### Reproducing the Benchmark

```bash
uv run python3 pkg/c/postgres/bench_pg.py --json-out /tmp/lemmajet_pg_bench.json
```
