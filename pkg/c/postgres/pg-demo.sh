#!/usr/bin/env bash
# ============================================================================
# LemmaJet PostgreSQL 17 TEXT SEARCH Interactive Comparison & Demo
#
# Demonstrates (via ephemeral Docker container):
#   1. 32-variant English tense & voice matrix (simple vs english Snowball vs lemmajet_en)
#   2. Over-stemming guard (university/universe, police/policy, organ/organization)
#   3. Polish 14-case declension & Filter = true + unaccent chaining
#   4. Native PostgreSQL FTS features (ts_rank_cd, ts_headline, websearch_to_tsquery, GIN EXPLAIN)
#   5. GIN indexing throughput & index size comparison (16,000 docs)
#
# Usage:
#   bash pkg/c/postgres/pg-demo.sh
#   bash pkg/c/postgres/pg-demo.sh --no-build
#   bash pkg/c/postgres/pg-demo.sh --profile sm_balanced --pg-major 17
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
C_PKG_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

PG_MAJOR="${PG_MAJOR:-17}"
PROFILE="${PROFILE:-md_balanced}"
IMAGE="${IMAGE:-}"
NO_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --image)
      IMAGE="$2"
      shift 2
      ;;
    --no-build)
      NO_BUILD=1
      shift
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --pg-major)
      PG_MAJOR="$2"
      shift 2
      ;;
    -h|--help)
      cat <<EOF
Usage: bash pkg/c/postgres/pg-demo.sh [options]

Options:
  --image <tag>      Docker image tag (default: lemmajet-pg:<pg-major>)
  --no-build         Skip docker build and reuse existing image
  --profile <name>   LemmaJet profile to demo (default: md_balanced)
  --pg-major <ver>   PostgreSQL major version (default: 17)
  -h, --help         Show this help message
EOF
      exit 0
      ;;
    *)
      echo "Error: Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! "$PROFILE" =~ ^[a-z0-9_]+$ ]]; then
  echo "Error: Invalid profile name: $PROFILE" >&2
  exit 1
fi

if [[ ! "$PG_MAJOR" =~ ^[0-9]+$ ]]; then
  echo "Error: Invalid pg-major version: $PG_MAJOR" >&2
  exit 1
fi

if [ -z "$IMAGE" ]; then
  IMAGE="lemmajet-pg:${PG_MAJOR}"
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker is required to run pkg/c/postgres/pg-demo.sh." >&2
  exit 1
fi

if [ "$NO_BUILD" -eq 0 ]; then
  BUILD_PROFILES="xs_balanced sm_balanced md_balanced"
  if [[ " ${BUILD_PROFILES} " != *" ${PROFILE} "* ]]; then
    BUILD_PROFILES="${BUILD_PROFILES} ${PROFILE}"
  fi
  echo "==> Building ${IMAGE} (PG_MAJOR=${PG_MAJOR}, LANGS='pl en', PROFILES='${BUILD_PROFILES}')..."
  docker build -f "${SCRIPT_DIR}/Dockerfile" --target runtime \
    --build-arg PG_MAJOR="${PG_MAJOR}" \
    --build-arg LANGS="pl en" \
    --build-arg PROFILES="${BUILD_PROFILES}" \
    -t "${IMAGE}" "${C_PKG_DIR}"
else
  if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    echo "ERROR: Docker image '${IMAGE}' not found. Run without --no-build or run 'make -C pkg/c/postgres docker-build'." >&2
    exit 1
  fi
fi

CID=""
cleanup() {
  if [ -n "$CID" ]; then
    docker rm -f "$CID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

echo "==> Starting ephemeral PostgreSQL ${PG_MAJOR} container (${IMAGE})..."
CID="$(docker run -d -e POSTGRES_PASSWORD=postgres "${IMAGE}")"

for _ in $(seq 1 60); do
  if docker exec "$CID" pg_isready -U postgres >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

if ! docker exec "$CID" pg_isready -U postgres >/dev/null 2>&1; then
  echo "ERROR: PostgreSQL container failed to become ready." >&2
  exit 1
fi

run_psql() {
  docker exec -i "$CID" psql -U postgres -v ON_ERROR_STOP=1 -P pager=off "$@"
}

echo ""
echo "==> Setup: Installing extensions (lemmajet_en, lemmajet_pl, unaccent) [profile=${PROFILE}]"
run_psql -q <<SQL
CREATE EXTENSION IF NOT EXISTS unaccent;
CREATE EXTENSION IF NOT EXISTS lemmajet_en;
CREATE EXTENSION IF NOT EXISTS lemmajet_pl;

CREATE TEXT SEARCH DICTIONARY lemmajet_en_demo_dict (
    TEMPLATE = lemmajet_en_template,
    Mode = 'lemma',
    Profile = '${PROFILE}'
);
CREATE TEXT SEARCH CONFIGURATION lemmajet_en_demo (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION lemmajet_en_demo
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH lemmajet_en_demo_dict;

CREATE TEXT SEARCH DICTIONARY lemmajet_pl_demo_dict (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = '${PROFILE}'
);
CREATE TEXT SEARCH CONFIGURATION lemmajet_pl_demo (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION lemmajet_pl_demo
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH lemmajet_pl_demo_dict;
SQL

echo ""
echo "==> 1. 32-Variant English Tense & Voice Matrix (simple vs english Snowball vs lemmajet_en)"
run_psql <<'SQL'
CREATE TABLE demo_sentences (
    id int PRIMARY KEY,
    variant text NOT NULL,
    body text NOT NULL,
    tsv_simple tsvector GENERATED ALWAYS AS (to_tsvector('simple', body)) STORED,
    tsv_english tsvector GENERATED ALWAYS AS (to_tsvector('english', body)) STORED,
    tsv_lemmajet tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_en_demo', body)) STORED
);

INSERT INTO demo_sentences (id, variant, body) VALUES
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

CREATE INDEX demo_sentences_simple_gin   ON demo_sentences USING gin (tsv_simple);
CREATE INDEX demo_sentences_english_gin  ON demo_sentences USING gin (tsv_english);
CREATE INDEX demo_sentences_lemmajet_gin ON demo_sentences USING gin (tsv_lemmajet);

SELECT
    id,
    variant,
    CASE WHEN tsv_simple   @@ plainto_tsquery('simple', 'smart engine find exact lemma build fast index')          THEN 'YES' ELSE 'no' END AS pg_simple,
    CASE WHEN tsv_english  @@ plainto_tsquery('english', 'smart engine find exact lemma build fast index')         THEN 'YES' ELSE 'no' END AS pg_english,
    CASE WHEN tsv_lemmajet @@ plainto_tsquery('lemmajet_en_demo', 'smart engine find exact lemma build fast index') THEN 'YES' ELSE 'no' END AS lemmajet_en
FROM demo_sentences
ORDER BY id;

SELECT
    'smart engine find exact lemma build fast index' AS query,
    count(*) FILTER (WHERE tsv_simple   @@ plainto_tsquery('simple', 'smart engine find exact lemma build fast index'))          AS simple_hits,
    count(*) FILTER (WHERE tsv_english  @@ plainto_tsquery('english', 'smart engine find exact lemma build fast index'))         AS english_hits,
    count(*) FILTER (WHERE tsv_lemmajet @@ plainto_tsquery('lemmajet_en_demo', 'smart engine find exact lemma build fast index')) AS lemmajet_hits
FROM demo_sentences
UNION ALL
SELECT
    'find build',
    count(*) FILTER (WHERE tsv_simple   @@ plainto_tsquery('simple', 'find build')),
    count(*) FILTER (WHERE tsv_english  @@ plainto_tsquery('english', 'find build')),
    count(*) FILTER (WHERE tsv_lemmajet @@ plainto_tsquery('lemmajet_en_demo', 'find build'))
FROM demo_sentences;
SQL

echo ""
echo "==> 2. Over-Stemming Guard (english Snowball false positives vs lemmajet_en)"
run_psql <<'SQL'
CREATE TABLE demo_overstem (
    id int PRIMARY KEY,
    body text NOT NULL,
    tsv_english tsvector GENERATED ALWAYS AS (to_tsvector('english', body)) STORED,
    tsv_lemmajet tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_en_demo', body)) STORED
);

INSERT INTO demo_overstem (id, body) VALUES
  (1, 'The university changed its admission rules.'),
  (2, 'The universe is expanding faster than expected.'),
  (3, 'The police arrested the suspect.'),
  (4, 'The insurance policy covers fire damage.'),
  (5, 'The organization published an annual report.'),
  (6, 'A human organ requires constant blood flow.');

SELECT
    q.term AS query,
    (SELECT string_agg(id::text, ',' ORDER BY id) FROM demo_overstem WHERE tsv_english  @@ to_tsquery('english', q.term))         AS english_doc_ids,
    (SELECT string_agg(id::text, ',' ORDER BY id) FROM demo_overstem WHERE tsv_lemmajet @@ to_tsquery('lemmajet_en_demo', q.term)) AS lemmajet_doc_ids
FROM (VALUES ('universe'), ('police'), ('organ')) AS q(term);
SQL

echo ""
echo "==> 3. Polish 14-Case Declension & Filter = true + unaccent Chaining"
run_psql <<SQL
CREATE TEXT SEARCH DICTIONARY lemmajet_pl_filter (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = '${PROFILE}',
    Filter = true
);
CREATE TEXT SEARCH CONFIGURATION lemmajet_pl_unaccent (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION lemmajet_pl_unaccent
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH lemmajet_pl_filter, unaccent, simple;

CREATE TABLE demo_pl_14 (
    id int PRIMARY KEY,
    grammatical_case text NOT NULL,
    body text NOT NULL,
    tsv_simple tsvector GENERATED ALWAYS AS (to_tsvector('simple', body)) STORED,
    tsv_pl tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_pl_demo', body)) STORED,
    tsv_pl_unacc tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_pl_unaccent', body)) STORED
);

INSERT INTO demo_pl_14 (id, grammatical_case, body) VALUES
  (1,  'Sg. Nom', 'Młody mężczyzna szedł wąską drogą z psem w ręku.'),
  (2,  'Sg. Gen', 'Nie było tam młodego mężczyzny, wąskiej drogi, psa ani ręki.'),
  (3,  'Sg. Dat', 'Przyglądaliśmy się młodemu mężczyźnie, wąskiej drodze, psu i ręce.'),
  (4,  'Sg. Acc', 'Spotkaliśmy młodego mężczyznę, wąską drogę, psa i rękę.'),
  (5,  'Sg. Ins', 'Rozmawialiśmy z młodym mężczyzną nad drogą o psie z ręką.'),
  (6,  'Sg. Loc', 'Opowiadali o młodym mężczyźnie, wąskiej drodze, psie i ręce.'),
  (7,  'Sg. Voc', 'O młody mężczyzno, wąska drogo, psie i ręko!'),
  (8,  'Pl. Nom', 'Młodzi mężczyźni szli wąskimi drogami z psami w rękach.'),
  (9,  'Pl. Gen', 'Nie było tam młodych mężczyzn, wąskich dróg, psów ani rąk.'),
  (10, 'Pl. Dat', 'Przyglądaliśmy się młodym mężczyznom, wąskim drogom, psom i rękom.'),
  (11, 'Pl. Acc', 'Spotkaliśmy młodych mężczyzn, wąskie drogi, psy i ręce.'),
  (12, 'Pl. Ins', 'Rozmawialiśmy z młodymi mężczyznami nad drogami o psach z rękami.'),
  (13, 'Pl. Loc', 'Opowiadali o młodym mężczyźnie, wąskich drogach, psach i rękach.'),
  (14, 'Pl. Voc', 'O młodzi mężczyźni, wąskie drogi, psy i ręce!');

SELECT
    count(*) FILTER (WHERE tsv_simple   @@ to_tsquery('simple', 'mężczyzna'))              AS simple_diacritic,
    count(*) FILTER (WHERE tsv_pl       @@ to_tsquery('lemmajet_pl_demo', 'mężczyzna'))    AS lemmajet_pl_diacritic,
    count(*) FILTER (WHERE tsv_pl_unacc @@ to_tsquery('lemmajet_pl_unaccent', 'mezczyzna')) AS filter_unaccent_ascii
FROM demo_pl_14;
SQL

echo ""
echo "==> 4. Native PostgreSQL FTS Features (ts_rank_cd, ts_headline, websearch_to_tsquery, GIN EXPLAIN)"
run_psql <<'SQL'
CREATE TABLE demo_articles (
    id serial PRIMARY KEY,
    title text NOT NULL,
    body text NOT NULL,
    tsv tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_en_demo', title || ' ' || body)) STORED
);
CREATE INDEX demo_articles_gin ON demo_articles USING gin (tsv);

INSERT INTO demo_articles (title, body) VALUES
  ('Irregular Verbs in Search', 'Smart engines found exact lemmas and built fast indexes; finding the right root builds better search.'),
  ('Continuous Aspect',         'Developers are building fast indexes while engines are finding lemmas.'),
  ('Nightly Builds',            'Automated nightly builds run integration tests and publish container images.'),
  ('Query Planning',            'Cost-based optimizers choose index scans over sequential table scans.'),
  ('Storage Engines',           'Write-ahead logging guarantees durability across unexpected crashes.'),
  ('Full-Text Internals',       'Inverted postings lists store document identifiers and term offsets.');

SELECT
    title,
    round(ts_rank_cd(tsv, q)::numeric, 4) AS rank,
    ts_headline('lemmajet_en_demo', body, q, 'StartSel=[, StopSel=], MaxWords=16, MinWords=6') AS headline
FROM demo_articles,
     websearch_to_tsquery('lemmajet_en_demo', 'find -nightly OR build') AS q
WHERE tsv @@ q
ORDER BY rank DESC;

SET enable_seqscan = off;
EXPLAIN (COSTS OFF)
SELECT id, title
FROM demo_articles
WHERE tsv @@ websearch_to_tsquery('lemmajet_en_demo', 'find -nightly OR build');
SQL

echo ""
echo "==> 5. GIN Indexing Throughput Comparison (16,000 English docs = 32 sentences x 500)"
run_psql <<'SQL'
CREATE TEMP TABLE bench_raw (body text NOT NULL);
INSERT INTO bench_raw (body)
SELECT s.body
FROM generate_series(1, 500) AS g(n)
CROSS JOIN demo_sentences AS s;

CREATE OR REPLACE FUNCTION pg_temp.bench_gin(cfg regconfig)
RETURNS TABLE (
    config text,
    docs bigint,
    total_ms numeric,
    docs_per_sec numeric,
    gin_size text
)
LANGUAGE plpgsql
AS $$
DECLARE
    t0 timestamptz;
    t1 timestamptz;
    t2 timestamptz;
    elapsed_ms numeric;
    n_docs bigint;
BEGIN
    DROP TABLE IF EXISTS pg_temp.bench_work;
    CREATE TEMP TABLE bench_work (id serial PRIMARY KEY, body text NOT NULL, tsv tsvector);
    INSERT INTO pg_temp.bench_work (body) SELECT body FROM bench_raw;

    t0 := clock_timestamp();
    UPDATE pg_temp.bench_work SET tsv = to_tsvector(cfg, body);
    t1 := clock_timestamp();
    CREATE INDEX bench_work_gin ON pg_temp.bench_work USING gin (tsv);
    t2 := clock_timestamp();

    SELECT count(*) INTO n_docs FROM pg_temp.bench_work;
    elapsed_ms := round(EXTRACT(EPOCH FROM (t2 - t0)) * 1000.0, 2);

    RETURN QUERY SELECT
        cfg::text,
        n_docs,
        elapsed_ms,
        round(n_docs / NULLIF(EXTRACT(EPOCH FROM (t2 - t0)), 0), 0),
        pg_size_pretty(pg_relation_size('pg_temp.bench_work_gin'));
END;
$$;

SELECT * FROM pg_temp.bench_gin('simple'::regconfig)
UNION ALL
SELECT * FROM pg_temp.bench_gin('english'::regconfig)
UNION ALL
SELECT * FROM pg_temp.bench_gin('lemmajet_en_demo'::regconfig);
SQL

echo ""
echo "==> Done! Ephemeral container will be removed automatically."
