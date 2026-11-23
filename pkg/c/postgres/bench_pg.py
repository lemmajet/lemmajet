#!/usr/bin/env python3
"""Zero-dependency 6-language PostgreSQL FTS smoke test and GIN benchmark suite."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import statistics
import subprocess
import sys
import time
import uuid
from pathlib import Path

POSTGRES_DIR = Path(__file__).resolve().parent

DEFAULT_LANGS = ["pl", "en", "de", "es", "fr", "cs"]
DEFAULT_PROFILES = ["sm_balanced", "md_balanced"]
ALLOWED_LANGS = frozenset(DEFAULT_LANGS)
ALLOWED_PROFILES = frozenset(["xs_balanced", "sm_balanced", "md_balanced"])
IDENT_RE = re.compile(r"^[a-z0-9_]+$")

PG_SNOWBALL_CONFIG: dict[str, str] = {
    "en": "english",
    "de": "german",
    "es": "spanish",
    "fr": "french",
}

EN_32_DOCS = [
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

EN_OVERSTEM_DOCS = [
    "The university changed its admission rules.",
    "The universe is expanding faster than expected.",
    "The police arrested the suspect.",
    "The insurance policy covers fire damage.",
    "The organization published an annual report.",
    "A human organ requires constant blood flow.",
]

PL_14_DOCS = [
    "Młody mężczyzna szedł wąską drogą z psem w ręku.",
    "Nie było tam młodego mężczyzny, wąskiej drogi, psa ani ręki.",
    "Przyglądaliśmy się młodemu mężczyźnie, wąskiej drodze, psu i ręce.",
    "Spotkaliśmy młodego mężczyznę, wąską drogę, psa i rękę.",
    "Rozmawialiśmy z młodym mężczyzną nad drogą o psie z ręką.",
    "Opowiadali o młodym mężczyźnie, wąskiej drodze, psie i ręce.",
    "O młody mężczyzno, wąska drogo, psie i ręko!",
    "Młodzi mężczyźni szli wąskimi drogami z psami w rękach.",
    "Nie było tam młodych mężczyzn, wąskich dróg, psów ani rąk.",
    "Przyglądaliśmy się młodym mężczyznom, wąskim drogom, psom i rękom.",
    "Spotkaliśmy młodych mężczyzn, wąskie drogi, psy i ręce.",
    "Rozmawialiśmy z młodymi mężczyznami nad drogami o psach z rękami.",
    "Opowiadali o młodym mężczyźnie, wąskich drogach, psach i rękach.",
    "O młodzi mężczyźni, wąskie drogi, psy i ręce!",
]

# Curated (doc, query) inflection pairs for DE, ES, FR, CS (10 pairs each)
LANG_SMOKE_PAIRS: dict[str, list[tuple[str, str]]] = {
    "de": [
        ("Die alten Häuser stehen am Fluss.", "haus"),
        ("Hohe Bäume wachsen im Park.", "baum"),
        ("Kleine Katzen schlafen auf dem Sofa.", "katze"),
        ("Er ging gestern schnell nach Hause.", "gehen"),
        ("Das große Schiff fährt über das Meer.", "groß"),
        ("Viele Bücher liegen auf dem Tisch.", "buch"),
        ("Die Kinder spielen fröhlich im Garten.", "kind"),
        ("Tiefe Wälder bedecken die Berge.", "wald"),
        ("Sie sprachen lange über das Projekt.", "sprechen"),
        ("Die Hunde liefen durch den Schnee.", "laufen"),
    ],
    "es": [
        ("Las casas blancas brillan al sol.", "casa"),
        ("Los perros corren por el parque.", "perro"),
        ("Estamos comiendo fruta fresca ahora.", "comer"),
        ("Los buenos amigos siempre ayudan.", "bueno"),
        ("Ellos estuvieron hablando toda la tarde.", "hablar"),
        ("Los niños juegan en la plaza.", "niño"),
        ("Muchas flores crecen en primavera.", "flor"),
        ("Seguimos corriendo por la montaña.", "correr"),
        ("Las grandes ciudades tienen museos.", "grande"),
        ("Leímos varios libros interesantes ayer.", "libro"),
    ],
    "fr": [
        ("Les grandes maisons bordent la rivière.", "maison"),
        ("Ils partent en mangeant des pommes.", "manger"),
        ("Les chevaux galopent dans la prairie.", "cheval"),
        ("De belles fleurs poussent dans le jardin.", "fleur"),
        ("Elle continuait en parlant doucement.", "parler"),
        ("Ces vieux livres racontent une histoire.", "livre"),
        ("Les journaux publient de nouvelles études.", "journal"),
        ("Les travaux avancent rapidement ici.", "travail"),
        ("Les chiens courent dans la forêt.", "chien"),
        ("Nous avons fini nos projets hier.", "finir"),
    ],
    "cs": [
        ("Procházeli jsme starými městy.", "město"),
        ("Nové domy stojí na kopci.", "dům"),
        ("Mnozí lidé přišli na koncert.", "člověk"),
        ("Malé kočky spí na okně.", "kočka"),
        ("Viděli jsme krásné hory v dálce.", "krásný"),
        ("Silní muži nesli těžké kameny.", "muž"),
        ("Mladé ženy zpívaly lidové písně.", "žena"),
        ("Vysoké stromy rostou podél cesty.", "strom"),
        ("Četli jsme zajímavé knihy doma.", "kniha"),
        ("Velké lodě plují po řece.", "velký"),
    ],
}

BENCH_QUERIES: dict[str, str] = {
    "en": "smart engine find exact lemma",
    "pl": "mężczyzna droga ręka",
    "de": "haus baum katze",
    "es": "casa perro comer",
    "fr": "maison manger cheval",
    "cs": "město dům člověk",
}


class PgRunner:
    """Executes psql and host/container inspection commands."""

    def __init__(self, container: str | None, use_local_psql: bool) -> None:
        self.container = container
        self.use_local_psql = use_local_psql

    def psql_cmd(self) -> list[str]:
        base = ["psql", "-q", "-U", "postgres", "-v", "ON_ERROR_STOP=1", "-At", "-F", "|"]
        if self.use_local_psql:
            return base
        assert self.container is not None
        return ["docker", "exec", "-i", self.container, *base]

    def run_psql(self, sql: str) -> str:
        try:
            proc = subprocess.run(
                self.psql_cmd(),
                input=sql,
                text=True,
                capture_output=True,
                check=True,
            )
        except subprocess.CalledProcessError as exc:
            if exc.stderr:
                print(exc.stderr, file=sys.stderr, end="")
            raise
        return proc.stdout.strip()

    def run_shell(self, cmd: list[str], user: str | None = None) -> str:
        if self.use_local_psql:
            full_cmd = cmd
        else:
            user_args = ["-u", user] if user else []
            full_cmd = ["docker", "exec", *user_args, self.container or "", *cmd]
        try:
            proc = subprocess.run(full_cmd, text=True, capture_output=True, check=True)
        except subprocess.CalledProcessError as exc:
            if exc.stderr:
                print(exc.stderr, file=sys.stderr, end="")
            raise
        return proc.stdout.strip()


def sql_escape(text: str) -> str:
    return text.replace("'", "''")


def validate_ident(value: str, allowed: frozenset[str], label: str) -> str:
    if not IDENT_RE.match(value) or value not in allowed:
        raise ValueError(f"Invalid {label} {value!r}; allowed: {sorted(allowed)}")
    return value


def parse_list_arg(
    raw: str | None, default: list[str], allowed: frozenset[str], label: str
) -> list[str]:
    if not raw:
        return list(default)
    items = [item.strip() for item in raw.replace(",", " ").split() if item.strip()]
    return [validate_ident(item, allowed, label) for item in items]


def lang_profiles_for(
    lang: str, requested_profiles: list[str], explicit_profiles: bool
) -> list[str]:
    profiles = list(requested_profiles)
    if lang == "en" and not explicit_profiles and "xs_balanced" not in profiles:
        profiles = ["xs_balanced", *profiles]
    if lang != "en":
        profiles = [p for p in profiles if p != "xs_balanced"]
    return profiles


def detect_hunspell_pl(runner: PgRunner) -> bool:
    sharedir = runner.run_shell(["pg_config", "--sharedir"])
    dict_path = f"{sharedir}/tsearch_data/pl_pl.dict"
    aff_path = f"{sharedir}/tsearch_data/pl_pl.affix"
    try:
        runner.run_shell(["test", "-f", dict_path])
        runner.run_shell(["test", "-f", aff_path])
        return True
    except subprocess.CalledProcessError:
        return False


def detect_snowball_config(runner: PgRunner, cfgname: str) -> bool:
    out = runner.run_psql(
        f"SELECT count(*) FROM pg_catalog.pg_ts_config WHERE cfgname = '{sql_escape(cfgname)}';"
    )
    return out.strip() == "1"


def detect_stem_support(runner: PgRunner, lang: str) -> bool:
    probe_sql = (
        f"CREATE EXTENSION IF NOT EXISTS lemmajet_{lang};\n"
        "DO $$ BEGIN\n"
        f"  CREATE TEXT SEARCH DICTIONARY _lj_stem_probe (TEMPLATE = lemmajet_{lang}_template, Mode = 'stem', Profile = 'md_balanced');\n"
        "  DROP TEXT SEARCH DICTIONARY _lj_stem_probe;\n"
        "  RAISE NOTICE 'stem_ok';\n"
        "EXCEPTION WHEN invalid_parameter_value THEN\n"
        "  NULL;\n"
        "END $$;"
    )
    proc = subprocess.run(
        [
            "docker",
            "exec",
            "-i",
            runner.container,
            "psql",
            "-U",
            "postgres",
            "-v",
            "ON_ERROR_STOP=1",
        ]
        if not runner.use_local_psql and runner.container
        else ["psql", "-v", "ON_ERROR_STOP=1"],
        input=probe_sql,
        text=True,
        capture_output=True,
        check=True,
    )
    return "stem_ok" in proc.stderr


def detect_available_profiles(runner: PgRunner, lang: str, candidates: list[str]) -> list[str]:
    checks = "\n".join(
        [
            f"CREATE EXTENSION IF NOT EXISTS lemmajet_{lang};",
            *(
                f"DO $$ BEGIN\n"
                f"  CREATE TEXT SEARCH DICTIONARY _lj_prof_{p} (TEMPLATE = lemmajet_{lang}_template, Mode = 'lemma', Profile = '{p}');\n"
                f"  DROP TEXT SEARCH DICTIONARY _lj_prof_{p};\n"
                f"  RAISE NOTICE 'prof_ok:{p}';\n"
                f"EXCEPTION WHEN invalid_parameter_value THEN NULL;\n"
                f"END $$;"
                for p in candidates
            ),
        ]
    )
    proc = subprocess.run(
        [
            "docker",
            "exec",
            "-i",
            runner.container,
            "psql",
            "-U",
            "postgres",
            "-v",
            "ON_ERROR_STOP=1",
        ]
        if not runner.use_local_psql and runner.container
        else ["psql", "-v", "ON_ERROR_STOP=1"],
        input=checks,
        text=True,
        capture_output=True,
        check=True,
    )
    available = {p for p in candidates if f"prof_ok:{p}" in proc.stderr}
    return [p for p in candidates if p in available]


def setup_extensions(
    runner: PgRunner,
    langs: list[str],
    requested_profiles: list[str],
    explicit_profiles: bool,
    has_hunspell_pl: bool,
) -> dict[str, list[tuple[str, str]]]:
    """Create extensions, dictionaries, and text search configurations; return configs per lang."""
    sql_lines: list[str] = ["CREATE EXTENSION IF NOT EXISTS unaccent;"]
    configs_by_lang: dict[str, list[tuple[str, str]]] = {}
    has_polish_snowball = "pl" in langs and detect_snowball_config(runner, "polish")
    has_stem = detect_stem_support(runner, langs[0])

    for lang in langs:
        sql_lines.append(f"CREATE EXTENSION IF NOT EXISTS lemmajet_{lang};")
        lang_configs: list[tuple[str, str]] = [("simple", "pg_catalog.simple")]

        if lang in PG_SNOWBALL_CONFIG:
            sb = PG_SNOWBALL_CONFIG[lang]
            lang_configs.append((sb, f"pg_catalog.{sb}"))
        elif lang == "pl" and has_polish_snowball:
            lang_configs.append(("polish", "pg_catalog.polish"))

        if lang == "pl" and has_hunspell_pl:
            sql_lines.extend(
                [
                    "DROP TEXT SEARCH CONFIGURATION IF EXISTS hunspell_pl CASCADE;",
                    "DROP TEXT SEARCH DICTIONARY IF EXISTS polish_hunspell CASCADE;",
                    "CREATE TEXT SEARCH DICTIONARY polish_hunspell (TEMPLATE = ispell, DictFile = pl_pl, AffFile = pl_pl);",
                    "CREATE TEXT SEARCH CONFIGURATION hunspell_pl (COPY = pg_catalog.simple);",
                    (
                        "ALTER TEXT SEARCH CONFIGURATION hunspell_pl "
                        "ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part "
                        "WITH polish_hunspell, simple;"
                    ),
                ]
            )
            lang_configs.append(("hunspell_pl", "hunspell_pl"))

        profiles = detect_available_profiles(
            runner, lang, lang_profiles_for(lang, requested_profiles, explicit_profiles)
        )
        for profile in profiles:
            cfg = f"lemmajet_{lang}_{profile}_lemma"
            dct = f"{cfg}_dict"
            sql_lines.extend(
                [
                    f"DROP TEXT SEARCH CONFIGURATION IF EXISTS {cfg} CASCADE;",
                    f"DROP TEXT SEARCH DICTIONARY IF EXISTS {dct} CASCADE;",
                    (
                        f"CREATE TEXT SEARCH DICTIONARY {dct} "
                        f"(TEMPLATE = lemmajet_{lang}_template, Mode = 'lemma', Profile = '{profile}');"
                    ),
                    f"CREATE TEXT SEARCH CONFIGURATION {cfg} (COPY = pg_catalog.simple);",
                    (
                        f"ALTER TEXT SEARCH CONFIGURATION {cfg} "
                        "ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part "
                        f"WITH {dct};"
                    ),
                ]
            )
            lang_configs.append((cfg, cfg))

        # Add md_balanced stem mode when compiled with WITH_STEM=1
        if has_stem:
            stem_cfg = f"lemmajet_{lang}_md_balanced_stem"
            stem_dct = f"{stem_cfg}_dict"
            sql_lines.extend(
                [
                    f"DROP TEXT SEARCH CONFIGURATION IF EXISTS {stem_cfg} CASCADE;",
                    f"DROP TEXT SEARCH DICTIONARY IF EXISTS {stem_dct} CASCADE;",
                    (
                        f"CREATE TEXT SEARCH DICTIONARY {stem_dct} "
                        f"(TEMPLATE = lemmajet_{lang}_template, Mode = 'stem', Profile = 'md_balanced');"
                    ),
                    f"CREATE TEXT SEARCH CONFIGURATION {stem_cfg} (COPY = pg_catalog.simple);",
                    (
                        f"ALTER TEXT SEARCH CONFIGURATION {stem_cfg} "
                        "ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part "
                        f"WITH {stem_dct};"
                    ),
                ]
            )
            lang_configs.append((stem_cfg, stem_cfg))

        if lang == "pl":
            flt_dct = "lemmajet_pl_md_balanced_filter_dict"
            flt_cfg = "lemmajet_pl_md_balanced_unaccent"
            sql_lines.extend(
                [
                    f"DROP TEXT SEARCH CONFIGURATION IF EXISTS {flt_cfg} CASCADE;",
                    f"DROP TEXT SEARCH DICTIONARY IF EXISTS {flt_dct} CASCADE;",
                    (
                        f"CREATE TEXT SEARCH DICTIONARY {flt_dct} "
                        "(TEMPLATE = lemmajet_pl_template, Mode = 'lemma', Profile = 'md_balanced', Filter = true);"
                    ),
                    f"CREATE TEXT SEARCH CONFIGURATION {flt_cfg} (COPY = pg_catalog.simple);",
                    (
                        f"ALTER TEXT SEARCH CONFIGURATION {flt_cfg} "
                        "ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part "
                        f"WITH {flt_dct}, unaccent, simple;"
                    ),
                ]
            )

        configs_by_lang[lang] = lang_configs

    runner.run_psql("\n".join(sql_lines) + "\n")
    return configs_by_lang


def run_smoke_for_config(runner: PgRunner, lang: str, pg_cfg: str) -> dict[str, int]:
    """Run curated inflection smoke tests (and over-stemming / unaccent checks) for a config."""
    if lang == "en":
        values = ", ".join(f"({i}, '{sql_escape(d)}')" for i, d in enumerate(EN_32_DOCS, start=1))
        sql = (
            "CREATE TEMP TABLE smoke_en (id int, body text);\n"
            f"INSERT INTO smoke_en (id, body) VALUES {values};\n"
            "SELECT count(*) FROM smoke_en "
            f"WHERE to_tsvector('{pg_cfg}', body) @@ "
            f"plainto_tsquery('{pg_cfg}', 'smart engine find exact lemma build fast index');\n"
        )
        hits = int(runner.run_psql(sql).splitlines()[-1])

        over_vals = ", ".join(
            f"({i}, '{sql_escape(d)}')" for i, d in enumerate(EN_OVERSTEM_DOCS, start=1)
        )
        false_positives = 0
        for query_term, expected_id in (("universe", 2), ("police", 3), ("organ", 6)):
            ov_sql = (
                "CREATE TEMP TABLE smoke_ov (id int, body text);\n"
                f"INSERT INTO smoke_ov (id, body) VALUES {over_vals};\n"
                "SELECT count(*) FROM smoke_ov "
                f"WHERE id <> {expected_id} "
                f"AND to_tsvector('{pg_cfg}', body) @@ plainto_tsquery('{pg_cfg}', '{query_term}');\n"
            )
            false_positives += int(runner.run_psql(ov_sql).splitlines()[-1])

        return {
            "smoke_hits": hits,
            "smoke_total": len(EN_32_DOCS),
            "overstem_false_positives": false_positives,
        }

    if lang == "pl":
        values = ", ".join(f"({i}, '{sql_escape(d)}')" for i, d in enumerate(PL_14_DOCS, start=1))
        sql = (
            "CREATE TEMP TABLE smoke_pl (id int, body text);\n"
            f"INSERT INTO smoke_pl (id, body) VALUES {values};\n"
            "SELECT count(*) FROM smoke_pl "
            f"WHERE to_tsvector('{pg_cfg}', body) @@ to_tsquery('{pg_cfg}', 'mężczyzna');\n"
        )
        hits = int(runner.run_psql(sql).splitlines()[-1])
        result: dict[str, int] = {"smoke_hits": hits, "smoke_total": len(PL_14_DOCS)}

        if pg_cfg == "lemmajet_pl_md_balanced_lemma":
            unacc_sql = (
                "CREATE TEMP TABLE smoke_pl_u (id int, body text);\n"
                f"INSERT INTO smoke_pl_u (id, body) VALUES {values};\n"
                "SELECT count(*) FROM smoke_pl_u "
                "WHERE to_tsvector('lemmajet_pl_md_balanced_unaccent', body) @@ "
                "to_tsquery('lemmajet_pl_md_balanced_unaccent', 'mezczyzna');\n"
            )
            result["unaccent_hits"] = int(runner.run_psql(unacc_sql).splitlines()[-1])
        return result

    pairs = LANG_SMOKE_PAIRS[lang]
    values = ", ".join(f"({i}, '{sql_escape(doc)}')" for i, (doc, _) in enumerate(pairs, start=1))
    checks = " UNION ALL ".join(
        f"SELECT count(*) AS c FROM smoke_{lang} "
        f"WHERE id = {i} AND to_tsvector('{pg_cfg}', body) @@ plainto_tsquery('{pg_cfg}', '{sql_escape(q)}')"
        for i, (_, q) in enumerate(pairs, start=1)
    )
    sql = (
        f"CREATE TEMP TABLE smoke_{lang} (id int, body text);\n"
        f"INSERT INTO smoke_{lang} (id, body) VALUES {values};\n"
        f"SELECT COALESCE(sum(c), 0) FROM ({checks}) s;\n"
    )
    hits = int(runner.run_psql(sql).splitlines()[-1])
    return {"smoke_hits": hits, "smoke_total": len(pairs)}


def corpus_for_lang(lang: str) -> list[str]:
    if lang == "en":
        return EN_32_DOCS + EN_OVERSTEM_DOCS
    if lang == "pl":
        return PL_14_DOCS
    return [doc for doc, _ in LANG_SMOKE_PAIRS[lang]]


def bench_language(
    runner: PgRunner,
    lang: str,
    configs: list[tuple[str, str]],
    reps: int,
) -> dict[str, dict]:
    """Benchmark smoke tests, ts_stat(), GIN build throughput & size, and ts_rank_cd() latency."""
    corpus = corpus_for_lang(lang)
    base_values = ", ".join(f"('{sql_escape(doc)}')" for doc in corpus)
    doc_count = len(corpus) * reps
    query = sql_escape(BENCH_QUERIES[lang])

    # Populate shared unindexed base documents once per language
    seed_sql = (
        "DROP TABLE IF EXISTS bench_seed;\n"
        "CREATE UNLOGGED TABLE bench_seed (body text);\n"
        f"INSERT INTO bench_seed (body) VALUES {base_values};\n"
        "DROP TABLE IF EXISTS bench_docs;\n"
        "CREATE UNLOGGED TABLE bench_docs (id serial PRIMARY KEY, body text, tsv tsvector);\n"
        f"INSERT INTO bench_docs (body) SELECT s.body FROM bench_seed AS s, generate_series(1, {reps}) AS g;\n"
        "ANALYZE bench_docs;\n"
    )
    runner.run_psql(seed_sql)

    results: dict[str, dict] = {}
    simple_lexemes: int | None = None

    for label, pg_cfg in configs:
        smoke = run_smoke_for_config(runner, lang, pg_cfg)

        bench_sql = (
            "DROP INDEX IF EXISTS bench_gin_idx;\n"
            "UPDATE bench_docs SET tsv = NULL;\n"
            "SELECT extract(epoch FROM clock_timestamp()) * 1000;\n"
            f"UPDATE bench_docs SET tsv = to_tsvector('{pg_cfg}', body);\n"
            "SELECT extract(epoch FROM clock_timestamp()) * 1000;\n"
            "CREATE INDEX bench_gin_idx ON bench_docs USING gin (tsv);\n"
            "SELECT extract(epoch FROM clock_timestamp()) * 1000;\n"
            "SELECT count(*) FROM ts_stat('SELECT tsv FROM bench_docs');\n"
            "SELECT pg_relation_size('bench_gin_idx');\n"
        )
        t_wall0 = time.perf_counter()
        out_lines = runner.run_psql(bench_sql).splitlines()
        wall_ms = (time.perf_counter() - t_wall0) * 1000.0

        # Output lines: [t0, 'UPDATE ...', t1, t2, unique_lexemes, gin_bytes] (psql -At omits command tags)
        t0_ms = float(out_lines[-5])
        t1_ms = float(out_lines[-4])
        t2_ms = float(out_lines[-3])
        unique_lexemes = int(out_lines[-2])
        gin_bytes = int(out_lines[-1])

        tsv_update_ms = max(0.01, t1_ms - t0_ms)
        gin_build_ms = max(0.01, t2_ms - t1_ms)
        total_index_ms = max(0.01, t2_ms - t0_ms)
        docs_per_sec = round(doc_count / (total_index_ms / 1000.0))

        if label == "simple":
            simple_lexemes = unique_lexemes
            reduction_pct = 0.0
        elif simple_lexemes:
            reduction_pct = round((1.0 - unique_lexemes / simple_lexemes) * 100.0, 2)
        else:
            reduction_pct = 0.0

        # Measure ts_rank_cd() query latency over 25 runs in one session (server-side clock_timestamp)
        q_runs = 25
        q_sql_parts = []
        for _ in range(q_runs):
            q_sql_parts.extend(
                [
                    "SELECT extract(epoch FROM clock_timestamp()) * 1000;",
                    (
                        "SELECT count(*), max(ts_rank_cd(tsv, qq)) "
                        f"FROM bench_docs, plainto_tsquery('{pg_cfg}', '{query}') qq WHERE tsv @@ qq;"
                    ),
                    "SELECT extract(epoch FROM clock_timestamp()) * 1000;",
                ]
            )
        q_out = [
            x for x in runner.run_psql("\n".join(q_sql_parts) + "\n").splitlines() if x.strip()
        ]
        latencies = [
            max(0.001, float(q_out[i + 2]) - float(q_out[i])) for i in range(0, len(q_out) - 2, 3)
        ]
        query_mean_ms = round(statistics.mean(latencies), 3) if latencies else 0.0
        query_median_ms = round(statistics.median(latencies), 3) if latencies else 0.0

        entry = {
            **smoke,
            "unique_lexemes": unique_lexemes,
            "lexeme_reduction_pct": reduction_pct,
            "doc_count": doc_count,
            "tsv_update_ms": round(tsv_update_ms, 2),
            "gin_build_ms": round(gin_build_ms, 2),
            "total_index_ms": round(total_index_ms, 2),
            "wall_index_ms": round(wall_ms, 2),
            "docs_per_sec": docs_per_sec,
            "gin_bytes": gin_bytes,
            "query_mean_ms": query_mean_ms,
            "query_median_ms": query_median_ms,
        }
        results[label] = entry

    runner.run_psql("DROP TABLE IF EXISTS bench_docs;\nDROP TABLE IF EXISTS bench_seed;\n")
    return results


def measure_so_sizes(runner: PgRunner, langs: list[str]) -> dict[str, int]:
    pkglibdir = runner.run_shell(["pg_config", "--pkglibdir"])
    sizes: dict[str, int] = {}
    for lang in langs:
        so_path = f"{pkglibdir}/lemmajet_{lang}.so"
        out = runner.run_shell(["stat", "-c", "%s", so_path])
        sizes[lang] = int(out)
    return sizes


def parse_smaps_rollup(text: str) -> dict[str, int]:
    wanted = {
        "Rss": "rss_kb",
        "Pss": "pss_kb",
        "Private_Dirty": "private_dirty_kb",
        "Shared_Clean": "shared_clean_kb",
    }
    metrics: dict[str, int] = {}
    for line in text.splitlines():
        for prefix, key in wanted.items():
            if line.startswith(f"{prefix}:"):
                parts = line.split()
                metrics[key] = int(parts[1])
    return metrics


def measure_single_worker_ram(runner: PgRunner, pg_cfg: str) -> dict[str, float | int]:
    """Open a fresh backend session, time cold-start to_tsvector, and read /proc/<pid>/smaps_rollup."""
    proc = subprocess.Popen(
        runner.psql_cmd(),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert proc.stdin is not None and proc.stdout is not None
    try:
        proc.stdin.write("SELECT pg_backend_pid();\n")
        proc.stdin.flush()
        pid_line = proc.stdout.readline().strip()
        pid = int(pid_line)

        sample_text = "Młodzi mężczyźni szli wąskimi drogami z psami w rękach."
        proc.stdin.write(
            "SELECT extract(epoch FROM clock_timestamp()) * 1000;\n"
            f"SELECT length(to_tsvector('{pg_cfg}', '{sample_text}')::text);\n"
            "SELECT extract(epoch FROM clock_timestamp()) * 1000;\n"
        )
        proc.stdin.flush()
        t0 = float(proc.stdout.readline().strip())
        _len = proc.stdout.readline().strip()
        t1 = float(proc.stdout.readline().strip())
        cold_start_ms = round(max(0.01, t1 - t0), 2)

        smaps_text = runner.run_shell(["cat", f"/proc/{pid}/smaps_rollup"], user="postgres")
        smaps = parse_smaps_rollup(smaps_text)
        return {"cold_start_ms": cold_start_ms, **smaps}
    finally:
        if proc.stdin:
            proc.stdin.close()
        proc.wait(timeout=10)
        if proc.returncode != 0:
            err = proc.stderr.read() if proc.stderr else ""
            raise RuntimeError(f"Worker RAM measurement failed for {pg_cfg}: {err}")


def measure_worker_ram(runner: PgRunner, has_hunspell_pl: bool) -> dict[str, dict]:
    targets = [("simple", "pg_catalog.simple"), ("lemmajet_pl", "lemmajet_pl")]
    if has_hunspell_pl:
        targets.append(("hunspell_pl", "hunspell_pl"))

    measurements: dict[str, dict] = {}
    for name, pg_cfg in targets:
        measurements[name] = measure_single_worker_ram(runner, pg_cfg)

    base = measurements["simple"]
    for name in measurements:
        m = measurements[name]
        m["delta_rss_kb"] = m.get("rss_kb", 0) - base.get("rss_kb", 0)
        m["delta_private_dirty_kb"] = m.get("private_dirty_kb", 0) - base.get("private_dirty_kb", 0)
    return measurements


def format_markdown_table(headers: list[str], rows: list[list[str]], right_align: set[int]) -> str:
    widths = [
        max(len(headers[i]), *(len(row[i]) for row in rows)) if rows else len(headers[i])
        for i in range(len(headers))
    ]

    def fmt_row(cells: list[str]) -> str:
        formatted = [
            cell.rjust(widths[i]) if i in right_align else cell.ljust(widths[i])
            for i, cell in enumerate(cells)
        ]
        return "| " + " | ".join(formatted) + " |"

    sep = "|-" + "-|-".join("-" * w for w in widths) + "-|"
    return "\n".join([fmt_row(headers), sep, *(fmt_row(row) for row in rows)])


def print_summary(data: dict) -> None:
    print("\n### PostgreSQL 6-Language FTS & GIN Benchmark\n")
    bench_headers = [
        "Lang",
        "Config",
        "Smoke",
        "Lexemes",
        "Red.",
        "Docs/sec",
        "Index ms",
        "GIN KB",
        "Query ms",
        ".so KB",
    ]
    bench_rows: list[list[str]] = []
    for lang, ldata in data["languages"].items():
        so_kb = f"{ldata['so_bytes'] / 1024:.1f}"
        for cfg_name, c in ldata["configs"].items():
            smoke = f"{c['smoke_hits']}/{c['smoke_total']}"
            if "overstem_false_positives" in c:
                smoke += f" (FP:{c['overstem_false_positives']})"
            if "unaccent_hits" in c:
                smoke += f" (unacc:{c['unaccent_hits']}/14)"
            gin_kb = f"{c['gin_bytes'] / 1024:.1f}"
            so_col = so_kb if cfg_name.startswith("lemmajet_") else "-"
            bench_rows.append(
                [
                    str(lang),
                    str(cfg_name),
                    smoke,
                    str(c["unique_lexemes"]),
                    f"{c['lexeme_reduction_pct']:+.1f}%",
                    str(c["docs_per_sec"]),
                    f"{c['total_index_ms']:.1f}",
                    gin_kb,
                    f"{c['query_median_ms']:.2f}",
                    so_col,
                ]
            )
    print(format_markdown_table(bench_headers, bench_rows, {3, 4, 5, 6, 7, 8, 9}))
    print(
        "\nColumns:\n"
        "  Lang      Language code (pl, en, de, es, fr, cs)\n"
        "  Config    TEXT SEARCH configuration (simple, Snowball, hunspell_pl, or lemmajet_*)\n"
        "  Smoke     Inflection hits (FP = over-stemming false positives; unacc = Filter+unaccent)\n"
        "  Lexemes   Unique indexed lexemes from ts_stat()\n"
        "  Red.      Unique lexeme reduction vs. simple baseline (higher is better)\n"
        "  Docs/sec  GIN index build throughput (documents/sec)\n"
        "  Index ms  Total GIN index creation wall-clock time (ms)\n"
        "  GIN KB    GIN index size via pg_relation_size() (KB)\n"
        "  Query ms  Median ts_rank_cd() query latency over 50 runs (ms)\n"
        "  .so KB    Compiled lemmajet_<lang>.so shared library size (KB)"
    )

    if data.get("worker_ram"):
        print("\n### Per-Backend Worker RAM & Cold-Start (Polish)\n")
        ram_headers = [
            "Config",
            "Cold-Start ms",
            "RSS KB",
            "PSS KB",
            "Private_Dirty KB",
            "Delta Private_Dirty KB",
        ]
        ram_rows: list[list[str]] = []
        for name, m in data["worker_ram"].items():
            ram_rows.append(
                [
                    str(name),
                    f"{m['cold_start_ms']:.2f}",
                    str(m.get("rss_kb", 0)),
                    str(m.get("pss_kb", 0)),
                    str(m.get("private_dirty_kb", 0)),
                    f"{m.get('delta_private_dirty_kb', 0):+d}",
                ]
            )
        print(format_markdown_table(ram_headers, ram_rows, {1, 2, 3, 4, 5}))
        print(
            "\nColumns:\n"
            "  Config                  Dictionary initialized in a fresh PostgreSQL backend\n"
            "  Cold-Start ms           Time for first to_tsvector() call in a new session (ms)\n"
            "  RSS KB                  Resident Set Size from /proc/<pid>/smaps_rollup (KB)\n"
            "  PSS KB                  Proportional Set Size accounting for shared pages (KB)\n"
            "  Private_Dirty KB        Unshared private dirty RAM per worker process (KB)\n"
            "  Delta Private_Dirty KB  Extra per-worker Private_Dirty RAM above simple baseline (KB)"
        )


def ensure_docker_image(image: str, no_build: bool, pg_major: str = "17") -> None:
    inspect = subprocess.run(["docker", "image", "inspect", image], capture_output=True)
    if inspect.returncode == 0:
        return
    if no_build:
        raise RuntimeError(f"Docker image {image!r} not found and --no-build was specified.")
    subprocess.run(
        [
            "docker",
            "build",
            "-f",
            str(POSTGRES_DIR / "Dockerfile"),
            "--target",
            "bench",
            "--build-arg",
            f"PG_MAJOR={pg_major}",
            "-t",
            image,
            str(POSTGRES_DIR.parent),
        ],
        check=True,
    )


def wait_for_pg_ready(container: str, timeout_s: float = 30.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        proc = subprocess.run(
            ["docker", "exec", container, "pg_isready", "-U", "postgres"],
            capture_output=True,
        )
        if proc.returncode == 0:
            return
        time.sleep(0.5)
    raise RuntimeError(
        f"PostgreSQL in container {container!r} did not become ready within {timeout_s}s."
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="LemmaJet PostgreSQL 6-language FTS & GIN benchmark suite"
    )
    parser.add_argument(
        "--langs", help="Comma or space-separated language codes (default: pl en de es fr cs)"
    )
    parser.add_argument(
        "--profiles", help="Comma or space-separated profiles (default: sm_balanced md_balanced)"
    )
    parser.add_argument(
        "--reps",
        type=int,
        default=500,
        help="Corpus repetitions per GIN benchmark table (default: 500)",
    )
    parser.add_argument(
        "--pg-major",
        default="17",
        help="PostgreSQL major version for Docker build/image tag (default: 17)",
    )
    parser.add_argument(
        "--image",
        default=None,
        help="Docker image name (default: lemmajet-pg-bench:<pg-major>)",
    )
    parser.add_argument(
        "--no-build", action="store_true", help="Skip docker build if image already exists"
    )
    parser.add_argument("--container", help="Attach to an already-running PostgreSQL container")
    parser.add_argument(
        "--psql", action="store_true", help="Run directly against local bare-metal psql"
    )
    parser.add_argument("--json-out", help="Write structured JSON benchmark results to this path")
    args = parser.parse_args()

    langs = parse_list_arg(args.langs, DEFAULT_LANGS, ALLOWED_LANGS, "language")
    explicit_profiles = bool(args.profiles)
    profiles = parse_list_arg(args.profiles, DEFAULT_PROFILES, ALLOWED_PROFILES, "profile")

    if args.psql and not shutil.which("psql"):
        print("ERROR: --psql specified but psql was not found on PATH.", file=sys.stderr)
        return 1

    ephemeral_container: str | None = None
    container = args.container

    image = args.image or f"lemmajet-pg-bench:{args.pg_major}"

    try:
        if not args.psql and not container:
            ensure_docker_image(image, args.no_build, args.pg_major)
            ephemeral_container = f"lemmajet-pg-bench-{uuid.uuid4().hex[:8]}"
            subprocess.run(
                [
                    "docker",
                    "run",
                    "-d",
                    "--name",
                    ephemeral_container,
                    "-e",
                    "POSTGRES_PASSWORD=postgres",
                    image,
                ],
                check=True,
                stdout=subprocess.DEVNULL,
            )
            container = ephemeral_container
            wait_for_pg_ready(container)

        runner = PgRunner(container=container, use_local_psql=args.psql)
        has_hunspell_pl = detect_hunspell_pl(runner)
        configs_by_lang = setup_extensions(
            runner, langs, profiles, explicit_profiles, has_hunspell_pl
        )
        so_sizes = measure_so_sizes(runner, langs)

        languages_out: dict[str, dict] = {}
        for lang in langs:
            languages_out[lang] = {
                "so_bytes": so_sizes[lang],
                "configs": bench_language(runner, lang, configs_by_lang[lang], args.reps),
            }

        worker_ram = measure_worker_ram(runner, has_hunspell_pl) if "pl" in langs else {}
        pg_version = runner.run_psql("SHOW server_version;")

        payload = {
            "pg_version": pg_version,
            "reps": args.reps,
            "has_hunspell_pl": has_hunspell_pl,
            "so_sizes": so_sizes,
            "languages": languages_out,
            "worker_ram": worker_ram,
        }

        print_summary(payload)

        if args.json_out:
            out_path = Path(args.json_out)
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")

        return 0
    finally:
        if ephemeral_container:
            subprocess.run(["docker", "rm", "-f", ephemeral_container], capture_output=True)


if __name__ == "__main__":
    raise SystemExit(main())
