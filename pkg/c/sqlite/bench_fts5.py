#!/usr/bin/env python3
"""Layer 1 & Layer 3 SQLite FTS5 smoke, fts5vocab, throughput, and size benchmark suite."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SQLITE_DIR = Path(__file__).resolve().parent
C_PKG_DIR = SQLITE_DIR.parent
BUILD_DIR = SQLITE_DIR / "build"
SO_EXT = "dylib" if platform.system() == "Darwin" else "so"

DEFAULT_LANGS = ["pl", "en", "de", "es", "fr", "cs"]
DEFAULT_PROFILES = ["sm_balanced", "md_balanced"]

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


def sql_escape(text: str) -> str:
    return text.replace("'", "''")


def detect_sqlite_cli(
    explicit_cli: str | None,
    sqlite_src: str | None,
) -> tuple[str | None, str | None]:
    """Return (sqlite_cli_path, sqlite_src_dir) or (None, None) if unavailable."""
    src_dir = explicit_cli_src = sqlite_src or os.environ.get("SQLITE_SRC")
    candidates: list[str] = []

    if explicit_cli:
        candidates.append(explicit_cli)
    if os.environ.get("SQLITE_CLI"):
        candidates.append(os.environ["SQLITE_CLI"])
    if explicit_cli_src:
        candidates.append(str(Path(explicit_cli_src) / "sqlite3"))
    which_cli = shutil.which("sqlite3")
    if which_cli:
        candidates.append(which_cli)

    for cand in candidates:
        p = Path(cand)
        if not (p.is_file() and os.access(p, os.X_OK)):
            continue
        # Verify .load support via .help
        proc = subprocess.run(
            [str(p), ":memory:", ".help load"],
            text=True,
            capture_output=True,
        )
        if proc.returncode == 0 and ".load" in proc.stdout:
            if not src_dir:
                for parent in (p.resolve().parent, p.resolve().parent.parent):
                    if (parent / "sqlite3ext.h").is_file():
                        src_dir = str(parent)
                        break
            return str(p), src_dir

    return None, src_dir


def run_sqlite_json(
    sqlite_cli: str,
    ext_base: str | None,
    sql_commands: str,
) -> list[dict]:
    prefix = f".load {ext_base}\n" if ext_base else ""
    full_sql = f"{prefix}.mode json\n{sql_commands}\n"
    proc = subprocess.run(
        [sqlite_cli],
        input=full_sql,
        text=True,
        capture_output=True,
        check=True,
    )
    out = proc.stdout.strip()
    return json.loads(out) if out else []


def build_model_extension(
    lang: str,
    profile: str,
    sqlite_src: str | None,
) -> tuple[Path, Path]:
    """Build per-model SQLite FTS5 extension and return (ext_path, obj_path)."""
    cmd = ["make", "-C", str(SQLITE_DIR), "model", f"LANG={lang}", f"PROFILE={profile}"]
    if sqlite_src:
        cmd.append(f"SQLITE_SRC={sqlite_src}")
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    except subprocess.CalledProcessError as exc:
        if exc.stderr:
            print(exc.stderr, file=sys.stderr, end="")
        raise
    ext_path = BUILD_DIR / f"lemmajet_fts5_{lang}_{profile}.{SO_EXT}"
    obj_path = C_PKG_DIR / "build" / "models" / f"{lang}_lemma_{profile}.o"
    return ext_path, obj_path


def run_smoke_for_config(
    sqlite_cli: str,
    lang: str,
    tokenizer: str,
    ext_base: str | None,
) -> tuple[int, int]:
    """Return (passed, total) smoke check counts for (lang, tokenizer)."""
    if lang == "en":
        en32_inserts = ", ".join(f"('{sql_escape(d)}')" for d in EN_32_DOCS)
        r32 = run_sqlite_json(
            sqlite_cli,
            ext_base,
            f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tokenizer}');"
            f"INSERT INTO t(body) VALUES {en32_inserts};"
            "SELECT count(*) AS hits FROM t WHERE t MATCH 'smart engine find exact lemma build fast index';",
        )
        passed = int(r32[0]["hits"])
        total = len(EN_32_DOCS)

        over_inserts = ", ".join(f"('{sql_escape(d)}')" for d in EN_OVERSTEM_DOCS)
        for query, expected_rowids in (("police", [3]), ("organ", [6]), ("universe", [2])):
            ro = run_sqlite_json(
                sqlite_cli,
                ext_base,
                f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tokenizer}');"
                f"INSERT INTO t(body) VALUES {over_inserts};"
                f"SELECT rowid FROM t WHERE t MATCH '{query}' ORDER BY rowid;",
            )
            total += 1
            if [r["rowid"] for r in ro] == expected_rowids:
                passed += 1
        return passed, total

    if lang == "pl":
        pl_inserts = ", ".join(f"('{sql_escape(d)}')" for d in PL_14_DOCS)
        r_exact = run_sqlite_json(
            sqlite_cli,
            ext_base,
            f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tokenizer}');"
            f"INSERT INTO t(body) VALUES {pl_inserts};"
            "SELECT count(*) AS hits FROM t WHERE t MATCH 'mężczyzna droga ręka';",
        )
        r_prefix = run_sqlite_json(
            sqlite_cli,
            ext_base,
            f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tokenizer}');"
            f"INSERT INTO t(body) VALUES {pl_inserts};"
            "SELECT count(*) AS hits FROM t WHERE t MATCH 'mężczyz*';",
        )
        passed = int(r_exact[0]["hits"]) + int(r_prefix[0]["hits"])
        total = len(PL_14_DOCS) * 2
        return passed, total

    pairs = LANG_SMOKE_PAIRS[lang]
    inserts = ", ".join(f"('{sql_escape(doc)}')" for doc, _ in pairs)
    passed = 0
    for idx, (_, query) in enumerate(pairs, start=1):
        rows = run_sqlite_json(
            sqlite_cli,
            ext_base,
            f"CREATE VIRTUAL TABLE t USING fts5(body, tokenize = '{tokenizer}');"
            f"INSERT INTO t(body) VALUES {inserts};"
            f"SELECT rowid FROM t WHERE t MATCH '{sql_escape(query)}' ORDER BY rowid;",
        )
        if idx in [r["rowid"] for r in rows]:
            passed += 1
    return passed, len(pairs)


def run_bench_for_config(
    sqlite_cli: str,
    lang: str,
    tokenizer: str,
    ext_base: str | None,
    n_docs: int,
    tmpdir: Path,
) -> dict[str, float | int]:
    """Measure indexing throughput, fts5vocab unique terms, and 50x query latency."""
    if lang == "en":
        corpus = EN_32_DOCS + EN_OVERSTEM_DOCS
    elif lang == "pl":
        corpus = PL_14_DOCS
    else:
        corpus = [doc for doc, _ in LANG_SMOKE_PAIRS[lang]]

    safe_name = tokenizer.replace(" ", "_")
    db_path = tmpdir / f"{lang}_{safe_name}.db"
    sql_path = tmpdir / f"{lang}_{safe_name}.sql"
    docs = [corpus[i % len(corpus)] for i in range(n_docs)]

    lines: list[str] = []
    if ext_base:
        lines.append(f".load {ext_base}")
    lines.append(f"CREATE VIRTUAL TABLE docs USING fts5(body, tokenize = '{tokenizer}');")
    lines.append("BEGIN;")
    lines.extend(f"INSERT INTO docs(body) VALUES ('{sql_escape(d)}');" for d in docs)
    lines.append("COMMIT;")
    sql_path.write_text("\n".join(lines), encoding="utf-8")

    t0 = time.perf_counter()
    with sql_path.open("r", encoding="utf-8") as fh:
        subprocess.run(
            [sqlite_cli, str(db_path)],
            stdin=fh,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=True,
        )
    index_ms = (time.perf_counter() - t0) * 1000.0
    docs_per_sec = round(n_docs / (index_ms / 1000.0))

    vocab_sql = (
        f"{f'.load {ext_base}' if ext_base else ''}\n"
        ".mode json\n"
        "CREATE VIRTUAL TABLE v USING fts5vocab(docs, row);\n"
        "SELECT count(*) AS vocab_terms FROM v;\n"
    )
    vocab_proc = subprocess.run(
        [sqlite_cli, str(db_path)],
        input=vocab_sql,
        text=True,
        capture_output=True,
        check=True,
    )
    vocab_terms = int(json.loads(vocab_proc.stdout.strip())[0]["vocab_terms"])

    query = BENCH_QUERIES[lang]
    q_lines = ([f".load {ext_base}"] if ext_base else []) + [
        f"SELECT count(*) FROM docs WHERE docs MATCH '{sql_escape(query)}';"
        for _ in range(50)
    ]
    t1 = time.perf_counter()
    subprocess.run(
        [sqlite_cli, str(db_path)],
        input="\n".join(q_lines),
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=True,
    )
    query_ms = (time.perf_counter() - t1) * 1000.0

    return {
        "vocab_terms": vocab_terms,
        "index_ms": round(index_ms, 1),
        "docs_per_sec": docs_per_sec,
        "query_ms": round(query_ms, 1),
    }


def format_markdown_table(results: list[dict]) -> str:
    header = (
        "| Lang | Config | Smoke | Vocab Terms | Vocab Red. | Docs/sec | Index ms | Query 50x ms | Ext KB | Obj KB |\n"
        "|------|--------|-------|-------------|------------|----------|----------|--------------|--------|--------|"
    )
    lines = [header]
    for r in results:
        ext_kb = f"{r['ext_bytes'] / 1024:.1f}" if r["ext_bytes"] else "-"
        obj_kb = f"{r['obj_bytes'] / 1024:.1f}" if r["obj_bytes"] else "-"
        red = f"{r['vocab_reduction_pct']:+.1f}%"
        lines.append(
            f"| {r['lang']} | {r['config']} | {r['smoke_passed']}/{r['smoke_total']} "
            f"| {r['vocab_terms']} | {red} | {r['docs_per_sec']} "
            f"| {r['index_ms']:.1f} | {r['query_ms']:.1f} | {ext_kb} | {obj_kb} |"
        )
    return "\n".join(lines)


def parse_list_arg(raw: str | None, default: list[str]) -> list[str]:
    if not raw:
        return default
    return [item.strip() for item in raw.replace(",", " ").split() if item.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description="LemmaJet SQLite FTS5 6-language benchmark suite")
    parser.add_argument("--sqlite-cli", help="Path to sqlite3 CLI binary with .load support")
    parser.add_argument("--sqlite-src", help="Path to SQLite source directory (contains sqlite3ext.h, fts5.h)")
    parser.add_argument("--langs", help="Comma or space-separated language codes (default: pl en de es fr cs)")
    parser.add_argument("--profiles", help="Comma or space-separated profiles (default: sm_balanced md_balanced)")
    parser.add_argument("--docs", type=int, default=4000, help="Number of documents for throughput benchmark")
    parser.add_argument("--json-out", help="Write JSON results to this file path")
    args = parser.parse_args()

    sqlite_cli, sqlite_src = detect_sqlite_cli(args.sqlite_cli, args.sqlite_src)
    if not sqlite_cli:
        print(
            "SKIP: No sqlite3 CLI with .load extension support found. "
            "Pass --sqlite-cli /path/to/sqlite3 or --sqlite-src /path/to/sqlite-src.",
            file=sys.stderr,
        )
        return 0

    langs = parse_list_arg(args.langs, DEFAULT_LANGS)
    profiles = parse_list_arg(args.profiles, DEFAULT_PROFILES)

    results: list[dict] = []
    with tempfile.TemporaryDirectory() as td:
        tmpdir = Path(td)
        for lang in langs:
            # 1. unicode61 baseline
            uni_smoke_ok, uni_smoke_total = run_smoke_for_config(sqlite_cli, lang, "unicode61", None)
            uni_bench = run_bench_for_config(sqlite_cli, lang, "unicode61", None, args.docs, tmpdir)
            base_vocab = int(uni_bench["vocab_terms"])
            results.append(
                {
                    "lang": lang,
                    "config": "unicode61",
                    "profile": None,
                    "smoke_passed": uni_smoke_ok,
                    "smoke_total": uni_smoke_total,
                    "vocab_terms": base_vocab,
                    "vocab_reduction_pct": 0.0,
                    "docs_per_sec": uni_bench["docs_per_sec"],
                    "index_ms": uni_bench["index_ms"],
                    "query_ms": uni_bench["query_ms"],
                    "ext_bytes": 0,
                    "obj_bytes": 0,
                }
            )

            # 2. porter baseline (en only)
            if lang == "en":
                p_ok, p_total = run_smoke_for_config(sqlite_cli, lang, "porter", None)
                p_bench = run_bench_for_config(sqlite_cli, lang, "porter", None, args.docs, tmpdir)
                p_vocab = int(p_bench["vocab_terms"])
                results.append(
                    {
                        "lang": lang,
                        "config": "porter",
                        "profile": None,
                        "smoke_passed": p_ok,
                        "smoke_total": p_total,
                        "vocab_terms": p_vocab,
                        "vocab_reduction_pct": round((1.0 - p_vocab / base_vocab) * 100.0, 2) if base_vocab else 0.0,
                        "docs_per_sec": p_bench["docs_per_sec"],
                        "index_ms": p_bench["index_ms"],
                        "query_ms": p_bench["query_ms"],
                        "ext_bytes": 0,
                        "obj_bytes": 0,
                    }
                )

            # 3. LemmaJet per-model extensions
            lang_profiles = list(profiles)
            if lang == "en" and not args.profiles and "xs_balanced" not in lang_profiles:
                lang_profiles = ["xs_balanced", *lang_profiles]

            for profile in lang_profiles:
                stem_h = C_PKG_DIR / "stemmer" / lang / f"{profile}.h"
                lemma_h = C_PKG_DIR / "lemmatizer" / lang / f"{profile}.h"
                if not (stem_h.exists() and lemma_h.exists()):
                    continue

                ext_path, obj_path = build_model_extension(lang, profile, sqlite_src)
                ext_base = str(ext_path.with_suffix(""))
                tok = f"lemmajet_{lang}_{profile} lemma"

                s_ok, s_total = run_smoke_for_config(sqlite_cli, lang, tok, ext_base)
                bench = run_bench_for_config(sqlite_cli, lang, tok, ext_base, args.docs, tmpdir)
                vocab = int(bench["vocab_terms"])
                results.append(
                    {
                        "lang": lang,
                        "config": f"lemmajet_{profile}",
                        "profile": profile,
                        "smoke_passed": s_ok,
                        "smoke_total": s_total,
                        "vocab_terms": vocab,
                        "vocab_reduction_pct": round((1.0 - vocab / base_vocab) * 100.0, 2) if base_vocab else 0.0,
                        "docs_per_sec": bench["docs_per_sec"],
                        "index_ms": bench["index_ms"],
                        "query_ms": bench["query_ms"],
                        "ext_bytes": ext_path.stat().st_size,
                        "obj_bytes": obj_path.stat().st_size,
                    }
                )

    print(format_markdown_table(results))

    if args.json_out:
        out_path = Path(args.json_out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(
            json.dumps(
                {
                    "sqlite_cli": sqlite_cli,
                    "docs": args.docs,
                    "results": results,
                },
                indent=2,
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
