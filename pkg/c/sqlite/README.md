# LemmaJet SQLite FTS5 Extension

Offline-built SQLite FTS5 tokenizer extensions (`unicode61` with `remove_diacritics=0` + `FTS5_TOKENIZE_PREFIX` passthrough + zero-allocation LemmaJet stemming/lemmatization).

For full SQLite source build instructions, multi-language usage, and 6-language benchmark results, see [`docs/sqlite-fts5.md`](../../../docs/sqlite-fts5.md).

---

## Prerequisites

Point `SQLITE_SRC` to an extracted & built SQLite source tree (containing `sqlite3ext.h`, `fts5.h`, and `sqlite3` CLI), or pass `SQLITE_INC=/path/to/include`:

```bash
# Tested with sqlite-src-3540100.zip:
#   ./configure --enable-json --enable-load-extension --enable-fts5 && make -j
```

---

## Makefile Targets

```bash
cd pkg/c/sqlite

# 1. Combined extension (build/lemmajet_fts5.dylib or .so), subsettable via LANGS/PROFILES or MODELS
make combined SQLITE_SRC=/path/to/sqlite-src
make combined SQLITE_SRC=/path/to/sqlite-src LANGS="pl en" PROFILES="sm_balanced md_balanced"
make combined SQLITE_SRC=/path/to/sqlite-src MODELS="pl:sm_balanced en:md_balanced"

# 2. Single per-model extension (build/lemmajet_fts5_<lang>_<profile>.dylib or .so)
make model SQLITE_SRC=/path/to/sqlite-src LANG=pl PROFILE=sm_balanced
make model SQLITE_SRC=/path/to/sqlite-src LANG=en PROFILE=md_balanced

# 3. Batch-build per-model extensions
make models SQLITE_SRC=/path/to/sqlite-src LANGS="pl en" PROFILES="sm_balanced md_balanced"

# 4. Multi-extension smoke test (:memory: load of pl/sm_balanced + en/md_balanced)
make test SQLITE_SRC=/path/to/sqlite-src

# 5. Clean build artifacts
make clean
```

> **Alternative header/CLI overrides:** If `sqlite3ext.h` / `fts5.h` are in a system or custom include path, pass `SQLITE_INC=/path/to/include` (and `SQLITE_CLI=/path/to/sqlite3` for `make test`).

---

## Docker (Optional, Zero-Setup)

If you do not have a local `SQLITE_SRC` build or C toolchain, you can build, test, and benchmark the Linux `.so` inside Docker:

```bash
cd pkg/c/sqlite

# Build image with combined lemmajet_fts5.so (optional LANGS/PROFILES/MODELS)
make docker-build
make docker-build LANGS="pl en" PROFILES="sm_balanced md_balanced"

# Run multi-extension FTS5 smoke tests inside the container
make docker-test

# Run bench_fts5.py inside the container
make docker-bench
make docker-bench BENCH_ARGS="--langs pl,en --docs 1000"
```

---

## SQL `tokenize` One-Liners

### Combined Extension

```sql
.load pkg/c/sqlite/build/lemmajet_fts5
CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet pl lemma');
CREATE VIRTUAL TABLE docs USING fts5(body, tokenize='lemmajet en stem xs_balanced');
```

### Per-Model Extensions (Side-by-Side)

```sql
.load pkg/c/sqlite/build/lemmajet_fts5_pl_sm_balanced
.load pkg/c/sqlite/build/lemmajet_fts5_en_md_balanced
CREATE VIRTUAL TABLE pl_docs USING fts5(body, tokenize='lemmajet_pl_sm_balanced lemma');
CREATE VIRTUAL TABLE en_docs USING fts5(body, tokenize='lemmajet_en_md_balanced lemma');
```

---

## Querying, `bm25()`, and FTS5 Full-Text Syntax

`lemmajet_fts5` preserves `unicode61` byte offsets (`iStart`/`iEnd`) and passes `FTS5_TOKENIZE_PREFIX` queries through, so all standard [SQLite FTS5 Full-text Query Syntax](https://sqlite.org/fts5.html#full_text_query_syntax) features (`*` prefix, `NEAR`, `+` / `"..."` phrases, `AND`/`OR`/`NOT`, `^` initial-token, column filters) and auxiliary functions (`bm25()`, `highlight()`, `snippet()`) work out of the box:

```sql
.load pkg/c/sqlite/build/lemmajet_fts5_en_md_balanced
CREATE VIRTUAL TABLE en_docs USING fts5(
  title,
  body,
  tokenize='lemmajet_en_md_balanced lemma'
);

INSERT INTO en_docs (title, body) VALUES
  ('Database Guide', 'The engineers were running fast queries on indexed tables'),
  ('Search Notes',   'She ran a full-text query and tuned the ranking'),
  ('Tuning Log',     'He runs nightly benchmarks on tokenizers'),
  ('Release Note',   'Added support for zero-allocation stemming');

-- Ranked lemma search with bm25() (more negative = better match) and snippet()
SELECT rowid,
       title,
       round(bm25(en_docs), 4) AS score,
       snippet(en_docs, 1, '[', ']', '...', 6) AS match_snippet
FROM en_docs
WHERE en_docs MATCH 'run'
ORDER BY bm25(en_docs);

-- Standard FTS5 query operators:
SELECT title FROM en_docs WHERE en_docs MATCH 'bench*';                    -- prefix (*)
SELECT title FROM en_docs WHERE en_docs MATCH 'run + fast';                -- phrase (+)
SELECT title FROM en_docs WHERE en_docs MATCH '"full-text query"';         -- quoted phrase
SELECT title FROM en_docs WHERE en_docs MATCH 'NEAR(run benchmark, 5)';    -- proximity (NEAR)
SELECT title FROM en_docs WHERE en_docs MATCH '(run OR stem) NOT nightly'; -- boolean
SELECT title FROM en_docs WHERE en_docs MATCH 'title:guide';               -- column filter
SELECT title FROM en_docs WHERE en_docs MATCH '^added';                    -- initial token (^)
```

> **Note on `*` prefix queries:** Because `FTS5_TOKENIZE_PREFIX` passes the query prefix through unlemmatized (while indexed tokens are lemmatized/stemmed), `prefix*` matches tokens whose **normalized lemma/stem** starts with `prefix`. See the [SQLite FTS5 documentation](https://sqlite.org/fts5.html) for full details on `bm25()` column weights, `highlight()`, `snippet()`, and query syntax.

---

## Quick Interactive Demo (`sqlite-demo.sh`)

Run the side-by-side 32-variant English tense/voice comparison (`unicode61` vs `porter` vs `lemmajet`), over-stemming checks, `bm25()` / `snippet()` / `*` / `NEAR` queries, and throughput benchmarks:

```bash
bash pkg/c/sqlite/sqlite-demo.sh --sqlite-src /path/to/sqlite-src
```

---

## Automated 6-Language Benchmark (`bench_fts5.py`)

Run smoke queries, `fts5vocab` term-reduction, throughput, and binary-size benchmarks across `pl`, `en`, `de`, `es`, `fr`, `cs`:

```bash
uv run python3 pkg/c/sqlite/bench_fts5.py --sqlite-src /path/to/sqlite-src --json-out /tmp/lemmajet_fts5_bench.json
```
