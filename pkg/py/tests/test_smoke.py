"""Smoke tests for precompiled Python models and dynamic loaders across all 6 languages.

Validates default imports, dynamic loaders, sample words, and all 216 profile modules.
Skips with a clear reason if precompiled models have not been generated yet.
"""

from __future__ import annotations

import importlib
import sys
from pathlib import Path

import pytest

import lemmajet
from lemmajet import MODES, SIZES, SUPPORTED_LANGUAGES, VARIANTS

_PL_MD = (
    Path(__file__).resolve().parent.parent
    / "src"
    / "lemmajet"
    / "stemmer"
    / "pl"
    / "md_balanced.py"
)
if not _PL_MD.is_file():
    pytest.skip(
        "Precompiled model artifacts not generated (run scripts/build_pkg.py first)",
        allow_module_level=True,
    )

SAMPLE_WORDS: dict[str, str] = {
    "pl": "psami",
    "en": "running",
    "de": "häuser",
    "es": "casas",
    "fr": "chats",
    "cs": "městy",
}

EXPECTED_MD_BALANCED: dict[str, dict[str, set[str]]] = {
    "pl": {"stem": {"pies", "ps"}, "lemma": {"pies"}},
    "en": {"stem": {"run"}, "lemma": {"run"}},
    "de": {"stem": {"haus", "häuser"}, "lemma": {"haus"}},
    "es": {"stem": {"cas", "casas"}, "lemma": {"casa"}},
    "fr": {"stem": {"chat", "chats"}, "lemma": {"chat", "chats"}},
    "cs": {"stem": {"měst"}, "lemma": {"město"}},
}


@pytest.mark.parametrize("lang", SUPPORTED_LANGUAGES)
def test_default_stemmer_root_imports(lang: str) -> None:
    """Test default stemmer root re-exports (from lemmajet.stemmer.<lang> import stem, stem_batch)."""
    mod = importlib.import_module(f"lemmajet.stemmer.{lang}")
    assert hasattr(mod, "stem") and callable(mod.stem)
    assert hasattr(mod, "stem_batch") and callable(mod.stem_batch)
    assert mod.LANGUAGE == lang
    assert mod.DEFAULT_PROFILE == "md_balanced"

    word = SAMPLE_WORDS[lang]
    res = mod.stem(word)
    assert isinstance(res, str) and len(res) > 0
    assert res in EXPECTED_MD_BALANCED[lang]["stem"]
    assert mod.stem_batch([word, word]) == [res, res]


@pytest.mark.parametrize("lang", SUPPORTED_LANGUAGES)
def test_default_lemmatizer_root_imports(lang: str) -> None:
    """Test default lemmatizer root re-exports (from lemmajet.lemmatizer.<lang> import lemmatize)."""
    mod = importlib.import_module(f"lemmajet.lemmatizer.{lang}")
    assert hasattr(mod, "lemmatize") and callable(mod.lemmatize)
    assert hasattr(mod, "lemmatize_batch") and callable(mod.lemmatize_batch)
    assert not hasattr(mod, "stem")
    assert mod.LANGUAGE == lang
    assert mod.DEFAULT_PROFILE == "md_balanced"

    word = SAMPLE_WORDS[lang]
    res = mod.lemmatize(word)
    assert isinstance(res, str) and len(res) > 0
    assert res in EXPECTED_MD_BALANCED[lang]["lemma"]
    assert mod.lemmatize_batch([word, word]) == [res, res]


@pytest.mark.parametrize("lang", SUPPORTED_LANGUAGES)
def test_dynamic_loaders(lang: str) -> None:
    """Test lemmajet.load_stemmer, load_lemmatizer, and load across all languages."""
    s_mod = lemmajet.load_stemmer(lang)
    assert callable(s_mod.stem) and callable(s_mod.stem_batch)

    l_mod = lemmajet.load_lemmatizer(lang)
    assert callable(l_mod.lemmatize) and callable(l_mod.lemmatize_batch)

    s_mod2 = lemmajet.load(lang, mode="stemmer")
    assert s_mod2 is s_mod

    l_mod2 = lemmajet.load(lang, mode="lemmatizer")
    assert l_mod2 is l_mod


@pytest.mark.parametrize("lang", SUPPORTED_LANGUAGES)
@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("size", SIZES)
@pytest.mark.parametrize("variant", VARIANTS)
def test_all_216_profile_modules(lang: str, mode: str, size: str, variant: str) -> None:
    """Validate all 216 explicit profile modules can be imported and executed."""
    mod_name = f"lemmajet.{mode}.{lang}.{size}_{variant}"
    mod = importlib.import_module(mod_name)

    fn_name = "stem" if mode == "stemmer" else "lemmatize"
    batch_fn_name = "stem_batch" if mode == "stemmer" else "lemmatize_batch"

    fn = getattr(mod, fn_name)
    batch_fn = getattr(mod, batch_fn_name)

    assert callable(fn)
    assert callable(batch_fn)

    word = SAMPLE_WORDS[lang]
    res = fn(word)
    assert isinstance(res, str) and len(res) > 0
    assert batch_fn([word]) == [res]

    # Clean up sys.modules to keep test memory compact
    if mod_name in sys.modules:
        del sys.modules[mod_name]
