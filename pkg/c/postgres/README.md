# LemmaJet PostgreSQL `TEXT SEARCH` Extensions (`PGXS`)

Zero-dependency, per-language PostgreSQL `TEXT SEARCH TEMPLATE` extensions (`lemmajet_pl`, `lemmajet_en`, `lemmajet_de`, `lemmajet_es`, `lemmajet_fr`, `lemmajet_cs`) with fork-shared `static const` `.rodata` tables, `TSL_FILTER` + `unaccent` chaining, and multi-stage `postgres:17` Docker packaging.

For full installation instructions, pipeline chaining patterns, and empirical 6-language + `hunspell-pl` benchmark results, see [`docs/postgres-fts.md`](../../../docs/postgres-fts.md).

---

## Docker Quick Start

```bash
# Build lean runtime image (lemmajet-pg:17) and run 6-language PGXS smoke suite
make -C pkg/c/postgres pg-test

# Build runtime or benchmark image explicitly
make -C pkg/c/postgres docker-build LANGS="pl en" PROFILES="sm_balanced md_balanced"
make -C pkg/c/postgres docker-build-bench

# Start local PostgreSQL 17 with Docker Compose
docker compose -f pkg/c/postgres/docker-compose.yml up -d
```

---

## Bare-Metal Linux `PGXS` Build

Requires `postgresql-server-dev-<major>` and `build-essential` (`with_llvm=no` is passed automatically):

```bash
cd pkg/c/postgres

# Build all 6 per-language extensions
make all

# Install subset into PostgreSQL libdir/extension (defaults to lemma-only, md_balanced profile)
sudo make install LANGS="pl en" PROFILES="md_balanced"
sudo make install LANGS="pl en" PROFILES="xs_balanced sm_balanced md_balanced" WITH_STEM=1

# Docker build with multi-profile + stem mode enabled
make docker-build PROFILES="xs_balanced sm_balanced md_balanced" WITH_STEM=1

# Run smoke suite against local psql
make pg-test

# Clean build artifacts
make clean
```

You can also build via the top-level `pkg/c` delegation target (`make -C pkg/c` stays PostgreSQL-free by default):

```bash
make -C pkg/c postgres LANGS="pl en"
```

---

## SQL One-Liners

```sql
-- 1. Load per-language extension (auto-wires template, default dict, and config)
CREATE EXTENSION IF NOT EXISTS lemmajet_pl;
SELECT to_tsvector('lemmajet_pl', 'Młodzi mężczyźni szli drogami') @@ to_tsquery('lemmajet_pl', 'mężczyzna');

-- 2. Custom dictionary (Profile defaults to 'md_balanced'; other profiles & Mode = 'stem' are opt-in at build time)
CREATE TEXT SEARCH DICTIONARY pl_lemma_md (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = 'md_balanced'
);

-- 3. Accent-insensitive lemmatized search (Filter = true / TSL_FILTER ahead of unaccent)
CREATE EXTENSION IF NOT EXISTS unaccent;
CREATE TEXT SEARCH DICTIONARY pl_flt (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = 'md_balanced',
    Filter = true
);
CREATE TEXT SEARCH CONFIGURATION pl_unacc (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION pl_unacc
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH pl_flt, unaccent, simple;
SELECT to_tsvector('pl_unacc', 'Rozmawialiśmy z młodymi mężczyznami') @@ to_tsquery('pl_unacc', 'mezczyzna');

-- 4. Native PostgreSQL FTS ranking, snippets, and websearch queries
CREATE EXTENSION IF NOT EXISTS lemmajet_en;
SELECT ts_rank_cd(tsv, q), ts_headline('lemmajet_en', body, q, 'StartSel=[, StopSel=]')
FROM articles, websearch_to_tsquery('lemmajet_en', 'find -nightly OR build') AS q
WHERE tsv @@ q;
```

---

## Interactive Demo (`pg-demo.sh`)

Run the one-command Docker demo showcasing the 32-variant English matrix, over-stemming guard, Polish 14-case `Filter = true` + `unaccent` chaining, `ts_rank_cd()` / `ts_headline()` / `websearch_to_tsquery()`, and GIN throughput:

```bash
bash pkg/c/postgres/pg-demo.sh
```

---

## 6-Language & `hunspell-pl` Benchmark (`bench_pg.py`)

Run smoke recall, `ts_stat()` lexeme reduction, `GIN` throughput/size, `ts_rank_cd()` latency, `.so` sizes, and `/proc/<pid>/smaps_rollup` worker RAM benchmarks across `pl`, `en`, `de`, `es`, `fr`, `cs`:

```bash
uv run python3 pkg/c/postgres/bench_pg.py --json-out /tmp/lemmajet_pg_bench.json
```
