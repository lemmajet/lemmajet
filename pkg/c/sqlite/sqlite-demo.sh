#!/usr/bin/env bash
# ============================================================================
# LemmaJet SQLite FTS5 Interactive Comparison & Benchmark Demo
#
# Demonstrates:
#   1. 32-variant English tense & voice matrix (unicode61 vs porter vs lemmajet)
#   2. Over-stemming guard (university/universe, police/policy, organ/organization)
#   3. Native SQLite FTS5 features (bm25(), snippet(), * prefix, NEAR)
#   4. End-to-end SQLite FTS5 indexing throughput (docs/sec & words/sec)
#   5. Raw ANSI C engine throughput (make bench)
#
# Usage:
#   bash pkg/c/sqlite/sqlite-demo.sh --sqlite-src /path/to/sqlite-src
#   SQLITE_SRC=/path/to/sqlite-src bash pkg/c/sqlite/sqlite-demo.sh
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
C_PKG_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

SQLITE_SRC="${SQLITE_SRC:-}"
SQLITE_INC="${SQLITE_INC:-}"
SQLITE_CLI="${SQLITE_CLI:-}"
PROFILE="${PROFILE:-md_balanced}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sqlite-src)
      SQLITE_SRC="$2"
      shift 2
      ;;
    --sqlite-inc)
      SQLITE_INC="$2"
      shift 2
      ;;
    --sqlite-cli)
      SQLITE_CLI="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    -h|--help)
      cat <<EOF
Usage: bash pkg/c/sqlite/sqlite-demo.sh [options]

Options:
  --sqlite-src <dir>   Path to built SQLite source tree (with sqlite3ext.h, fts5.h, sqlite3)
  --sqlite-inc <dir>   Path to directory containing sqlite3ext.h and fts5.h
  --sqlite-cli <bin>   Path to sqlite3 CLI binary with .load extension support
  --profile <name>     English profile to benchmark (default: md_balanced)
  -h, --help           Show this help message
EOF
      exit 0
      ;;
    *)
      echo "Error: Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# Resolve sqlite3 CLI with .load support
if [ -z "$SQLITE_CLI" ]; then
  if [ -n "$SQLITE_SRC" ] && [ -x "${SQLITE_SRC}/sqlite3" ]; then
    SQLITE_CLI="${SQLITE_SRC}/sqlite3"
  elif command -v sqlite3 >/dev/null 2>&1; then
    SQLITE_CLI="$(command -v sqlite3)"
  fi
fi

if [ -z "$SQLITE_CLI" ] || [ ! -x "$SQLITE_CLI" ]; then
  echo "ERROR: sqlite3 CLI not found. Pass --sqlite-src /path/to/sqlite-src or --sqlite-cli /path/to/sqlite3." >&2
  exit 1
fi

if ! "$SQLITE_CLI" :memory: ".help load" 2>/dev/null | grep -q "\.load"; then
  echo "ERROR: '$SQLITE_CLI' does not support .load (macOS system sqlite3 disables extensions)." >&2
  echo "Build SQLite from source per pkg/c/sqlite/README.md and pass --sqlite-src /path/to/sqlite-src." >&2
  exit 1
fi

# Build the English per-model extension
MAKE_ARGS=(model "LANG=en" "PROFILE=${PROFILE}")
if [ -n "$SQLITE_SRC" ]; then
  MAKE_ARGS+=("SQLITE_SRC=${SQLITE_SRC}")
elif [ -n "$SQLITE_INC" ]; then
  MAKE_ARGS+=("SQLITE_INC=${SQLITE_INC}")
fi

EXT_PATH="${SCRIPT_DIR}/build/lemmajet_fts5_en_${PROFILE}"

echo "==> Building LemmaJet ANSI C + SQLite FTS5 extension (en/${PROFILE})..."
make -C "${SCRIPT_DIR}" "${MAKE_ARGS[@]}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo ""
echo "==> 1. 32-Variant English Tense & Voice Matrix (unicode61 vs porter vs lemmajet)"
cat > "${TMP_DIR}/demo_32.sql" <<SQL
.load ${EXT_PATH}
.headers on
.mode column

CREATE TABLE sentences (
    id INTEGER PRIMARY KEY,
    variant TEXT NOT NULL,
    body TEXT NOT NULL
);

INSERT INTO sentences (id, variant, body) VALUES
  (1,  'Active  · Present Simple',            'Smart engines find exact lemmas and build fast indexes.'),
  (2,  'Active  · Present Continuous',        'Smart engines are finding exact lemmas and building fast indexes.'),
  (3,  'Active  · Present Perfect',           'Smart engines have found exact lemmas and built fast indexes.'),
  (4,  'Active  · Present Perfect Cont.',     'Smart engines have been finding exact lemmas and building fast indexes.'),
  (5,  'Active  · Past Simple',               'Smart engines found exact lemmas and built fast indexes.'),
  (6,  'Active  · Past Continuous',           'Smart engines were finding exact lemmas and building fast indexes.'),
  (7,  'Active  · Past Perfect',              'Smart engines had found exact lemmas and built fast indexes.'),
  (8,  'Active  · Past Perfect Cont.',        'Smart engines had been finding exact lemmas and building fast indexes.'),
  (9,  'Active  · Future Simple',             'Smart engines will find exact lemmas and build fast indexes.'),
  (10, 'Active  · Future Continuous',         'Smart engines will be finding exact lemmas and building fast indexes.'),
  (11, 'Active  · Future Perfect',            'Smart engines will have found exact lemmas and built fast indexes.'),
  (12, 'Active  · Future Perfect Cont.',      'Smart engines will have been finding exact lemmas and building fast indexes.'),
  (13, 'Active  · Conditional Simple',        'Smart engines would find exact lemmas and build fast indexes.'),
  (14, 'Active  · Conditional Continuous',    'Smart engines would be finding exact lemmas and building fast indexes.'),
  (15, 'Active  · Conditional Perfect',       'Smart engines would have found exact lemmas and built fast indexes.'),
  (16, 'Active  · Conditional Perfect Cont.', 'Smart engines would have been finding exact lemmas and building fast indexes.'),
  (17, 'Passive · Present Simple',            'Exact lemmas are found and fast indexes are built by smart engines.'),
  (18, 'Passive · Present Continuous',        'Exact lemmas are being found and fast indexes are being built by smart engines.'),
  (19, 'Passive · Present Perfect',           'Exact lemmas have been found and fast indexes have been built by smart engines.'),
  (20, 'Passive · Present Perfect Cont.',     'Exact lemmas have been being found and fast indexes have been being built by smart engines.'),
  (21, 'Passive · Past Simple',               'Exact lemmas were found and fast indexes were built by smart engines.'),
  (22, 'Passive · Past Continuous',           'Exact lemmas were being found and fast indexes were being built by smart engines.'),
  (23, 'Passive · Past Perfect',              'Exact lemmas had been found and fast indexes had been built by smart engines.'),
  (24, 'Passive · Past Perfect Cont.',        'Exact lemmas had been being found and fast indexes had been being built by smart engines.'),
  (25, 'Passive · Future Simple',             'Exact lemmas will be found and fast indexes will be built by smart engines.'),
  (26, 'Passive · Future Continuous',         'Exact lemmas will be being found and fast indexes will be being built by smart engines.'),
  (27, 'Passive · Future Perfect',            'Exact lemmas will have been found and fast indexes will have been built by smart engines.'),
  (28, 'Passive · Future Perfect Cont.',      'Exact lemmas will have been being found and fast indexes will have been being built by smart engines.'),
  (29, 'Passive · Conditional Simple',        'Exact lemmas would be found and fast indexes would be built by smart engines.'),
  (30, 'Passive · Conditional Continuous',    'Exact lemmas would be being found and fast indexes would be being built by smart engines.'),
  (31, 'Passive · Conditional Perfect',       'Exact lemmas would have been found and fast indexes would have been built by smart engines.'),
  (32, 'Passive · Conditional Perfect Cont.', 'Exact lemmas would have been being found and fast indexes would have been being built by smart engines.');

CREATE VIRTUAL TABLE fts_raw      USING fts5(body, content='sentences', content_rowid='id', tokenize='unicode61');
CREATE VIRTUAL TABLE fts_porter   USING fts5(body, content='sentences', content_rowid='id', tokenize='porter');
CREATE VIRTUAL TABLE fts_lemmajet USING fts5(body, content='sentences', content_rowid='id', tokenize='lemmajet_en_${PROFILE} lemma');

INSERT INTO fts_raw(fts_raw) VALUES('rebuild');
INSERT INTO fts_porter(fts_porter) VALUES('rebuild');
INSERT INTO fts_lemmajet(fts_lemmajet) VALUES('rebuild');

SELECT
  s.id,
  s.variant,
  CASE WHEN EXISTS (SELECT 1 FROM fts_raw r WHERE r.rowid = s.id AND fts_raw MATCH 'smart engine find exact lemma build fast index') THEN 'YES' ELSE 'no' END AS raw_unicode61,
  CASE WHEN EXISTS (SELECT 1 FROM fts_porter p WHERE p.rowid = s.id AND fts_porter MATCH 'smart engine find exact lemma build fast index') THEN 'YES' ELSE 'no' END AS fts5_porter,
  CASE WHEN EXISTS (SELECT 1 FROM fts_lemmajet l WHERE l.rowid = s.id AND fts_lemmajet MATCH 'smart engine find exact lemma build fast index') THEN 'YES' ELSE 'no' END AS fts5_lemmajet
FROM sentences s
ORDER BY s.id;

SELECT
  'Query: smart engine find exact lemma build fast index' AS query,
  (SELECT count(*) FROM fts_raw      WHERE fts_raw      MATCH 'smart engine find exact lemma build fast index') AS raw_hits,
  (SELECT count(*) FROM fts_porter   WHERE fts_porter   MATCH 'smart engine find exact lemma build fast index') AS porter_hits,
  (SELECT count(*) FROM fts_lemmajet WHERE fts_lemmajet MATCH 'smart engine find exact lemma build fast index') AS lemmajet_hits;

SELECT
  'Query: find build' AS query,
  (SELECT count(*) FROM fts_raw      WHERE fts_raw      MATCH 'find build') AS raw_hits,
  (SELECT count(*) FROM fts_porter   WHERE fts_porter   MATCH 'find build') AS porter_hits,
  (SELECT count(*) FROM fts_lemmajet WHERE fts_lemmajet MATCH 'find build') AS lemmajet_hits;
SQL
"$SQLITE_CLI" :memory: < "${TMP_DIR}/demo_32.sql"

echo ""
echo "==> 2. Over-Stemming Guard (porter false positives vs lemmajet)"
cat > "${TMP_DIR}/overstem.sql" <<SQL
.load ${EXT_PATH}
.headers on
.mode column

CREATE TABLE docs (id INTEGER PRIMARY KEY, body TEXT NOT NULL);
INSERT INTO docs (id, body) VALUES
  (1, 'The university changed its admission rules.'),
  (2, 'The universe is expanding faster than expected.'),
  (3, 'The police arrested the suspect.'),
  (4, 'The insurance policy covers fire damage.'),
  (5, 'The organization published an annual report.'),
  (6, 'A human organ requires constant blood flow.');

CREATE VIRTUAL TABLE o_porter   USING fts5(body, content='docs', content_rowid='id', tokenize='porter');
CREATE VIRTUAL TABLE o_lemmajet USING fts5(body, content='docs', content_rowid='id', tokenize='lemmajet_en_${PROFILE} lemma');
INSERT INTO o_porter(o_porter) VALUES('rebuild');
INSERT INTO o_lemmajet(o_lemmajet) VALUES('rebuild');

SELECT 'universe' AS query,
  (SELECT group_concat(rowid) FROM o_porter   WHERE o_porter   MATCH 'universe') AS porter_rowids,
  (SELECT group_concat(rowid) FROM o_lemmajet WHERE o_lemmajet MATCH 'universe') AS lemmajet_rowids
UNION ALL
SELECT 'police',
  (SELECT group_concat(rowid) FROM o_porter   WHERE o_porter   MATCH 'police'),
  (SELECT group_concat(rowid) FROM o_lemmajet WHERE o_lemmajet MATCH 'police')
UNION ALL
SELECT 'organ',
  (SELECT group_concat(rowid) FROM o_porter   WHERE o_porter   MATCH 'organ'),
  (SELECT group_concat(rowid) FROM o_lemmajet WHERE o_lemmajet MATCH 'organ');
SQL
"$SQLITE_CLI" :memory: < "${TMP_DIR}/overstem.sql"

echo ""
echo "==> 3. Native SQLite FTS5 Features (bm25, snippet, * prefix, NEAR)"
cat > "${TMP_DIR}/features.sql" <<SQL
.load ${EXT_PATH}
.headers on
.mode column

CREATE VIRTUAL TABLE articles USING fts5(title, body, tokenize='lemmajet_en_${PROFILE} lemma');
INSERT INTO articles (title, body) VALUES
  ('Irregular Verbs in Search', 'Smart engines found exact lemmas and built fast indexes; finding the right root builds better search.'),
  ('Continuous Aspect',         'Developers are building fast indexes while engines are finding lemmas.'),
  ('Database Migrations',       'A database migration script updated three configuration tables.'),
  ('Query Planning',            'Cost-based optimizers choose index scans over sequential table scans.'),
  ('Storage Engines',           'Write-ahead logging guarantees durability across unexpected crashes.'),
  ('Full-Text Internals',       'Inverted postings lists store document identifiers and term offsets.');

SELECT title, round(bm25(articles), 4) AS score, snippet(articles, 1, '[', ']', '...', 14) AS hit_snippet
FROM articles
WHERE articles MATCH 'find build'
ORDER BY bm25(articles);

SELECT count(*) AS prefix_hits FROM articles WHERE articles MATCH 'eng* lem*';
SELECT count(*) AS near_hits   FROM articles WHERE articles MATCH 'NEAR(find lemma, 5)';
SQL
"$SQLITE_CLI" :memory: < "${TMP_DIR}/features.sql"

if command -v python3 >/dev/null 2>&1; then
  echo ""
  echo "==> 4. SQLite FTS5 Indexing Throughput Benchmark (32,000 docs / 408,000 words)"
  cat > "${TMP_DIR}/bench_sql.py" <<'PY'
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sqlite_cli, ext_path, profile = sys.argv[1], sys.argv[2], sys.argv[3]

DOCS = [
    "Smart engines find exact lemmas and build fast indexes.",
    "Smart engines are finding exact lemmas and building fast indexes.",
    "Smart engines have found exact lemmas and built fast indexes.",
    "Smart engines have been finding exact lemmas and building fast indexes.",
    "Smart engines found exact lemmas and built fast indexes.",
    "Smart engines were finding exact lemmas and building fast indexes.",
    "Smart engines had found exact lemmas and built fast indexes.",
    "Smart engines had been finding exact lemmas and building fast indexes.",
    "Smart engines will find exact lemmas and build fast indexes.",
    "Smart engines will be finding exact lemmas and building fast indexes.",
    "Smart engines will have found exact lemmas and built fast indexes.",
    "Smart engines will have been finding exact lemmas and building fast indexes.",
    "Smart engines would find exact lemmas and build fast indexes.",
    "Smart engines would be finding exact lemmas and building fast indexes.",
    "Smart engines would have found exact lemmas and built fast indexes.",
    "Smart engines would have been finding exact lemmas and building fast indexes.",
    "Exact lemmas are found and fast indexes are built by smart engines.",
    "Exact lemmas are being found and fast indexes are being built by smart engines.",
    "Exact lemmas have been found and fast indexes have been built by smart engines.",
    "Exact lemmas have been being found and fast indexes have been being built by smart engines.",
    "Exact lemmas were found and fast indexes were built by smart engines.",
    "Exact lemmas were being found and fast indexes were being built by smart engines.",
    "Exact lemmas had been found and fast indexes had been built by smart engines.",
    "Exact lemmas had been being found and fast indexes had been being built by smart engines.",
    "Exact lemmas will be found and fast indexes will be built by smart engines.",
    "Exact lemmas will be being found and fast indexes will be being built by smart engines.",
    "Exact lemmas will have been found and fast indexes will have been built by smart engines.",
    "Exact lemmas will have been being found and fast indexes will have been being built by smart engines.",
    "Exact lemmas would be found and fast indexes would be built by smart engines.",
    "Exact lemmas would be being found and fast indexes would be being built by smart engines.",
    "Exact lemmas would have been found and fast indexes would have been built by smart engines.",
    "Exact lemmas would have been being found and fast indexes would have been being built by smart engines.",
]

N_REPS = 1000
total_docs = len(DOCS) * N_REPS
total_words = sum(len(d.split()) for d in DOCS) * N_REPS

configs = [
    ("unicode61 (raw FTS5)", None, "unicode61"),
    ("porter (built-in)",    None, "porter"),
    (f"lemmajet ({profile})", ext_path, f"lemmajet_en_{profile} lemma"),
]

with tempfile.TemporaryDirectory() as td:
    td_path = Path(td)
    for label, ext, tok in configs:
        db = td_path / f"{tok.replace(' ', '_')}.db"
        sql = td_path / f"{tok.replace(' ', '_')}.sql"
        lines = []
        if ext:
            lines.append(f".load {ext}")
        lines.append("PRAGMA journal_mode = OFF;")
        lines.append("PRAGMA synchronous = OFF;")
        lines.append(f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tok}');")
        lines.append("BEGIN;")
        batch = ", ".join(f"('{d}')" for d in DOCS)
        for _ in range(N_REPS):
            lines.append(f"INSERT INTO t(body) VALUES {batch};")
        lines.append("COMMIT;")
        sql.write_text("\n".join(lines) + "\n", encoding="utf-8")

        t0 = time.perf_counter()
        with sql.open("rb") as f:
            subprocess.run([sqlite_cli, str(db)], stdin=f, check=True, stdout=subprocess.DEVNULL)
        dt = time.perf_counter() - t0
        print(
            f"{label:24s} | {total_docs:,} docs | {total_words:,} words | "
            f"{dt*1000:6.1f} ms | {total_docs/dt:9,.0f} docs/s | {total_words/dt:10,.0f} words/s"
        )
PY
  python3 "${TMP_DIR}/bench_sql.py" "${SQLITE_CLI}" "${EXT_PATH}" "${PROFILE}"
fi

echo ""
echo "==> 5. Raw C Engine Throughput (make -C pkg/c bench)"
make -C "${C_PKG_DIR}" bench
