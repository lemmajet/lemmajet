"""LemmaJet: Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers.

Supports Polish (pl), English (en), German (de), Spanish (es), French (fr), and Czech (cs).
"""

from __future__ import annotations

import importlib
from types import ModuleType

__version__ = "0.6.0"

SUPPORTED_LANGUAGES: tuple[str, ...] = ("pl", "en", "de", "es", "fr", "cs")
SIZES: tuple[str, ...] = ("xxs", "xs", "sm", "md", "lg", "xl")
VARIANTS: tuple[str, ...] = ("precision", "balanced", "recall")
MODES: tuple[str, ...] = ("stemmer", "lemmatizer")

DEFAULT_SIZE = "md"
DEFAULT_VARIANT = "balanced"
DEFAULT_PROFILE = "md_balanced"


def _validate_profile(lang: str, size: str, variant: str) -> None:
    """Validate language code, profile size, and variant parameters.

    Args:
        lang: ISO 639-1 language code (e.g. 'pl', 'en').
        size: Model size ('xxs', 'xs', 'sm', 'md', 'lg', 'xl').
        variant: Operating point variant ('precision', 'balanced', 'recall').

    Raises:
        ValueError: If any parameter is not recognized.
    """
    if lang not in SUPPORTED_LANGUAGES:
        raise ValueError(
            f"Unsupported language {lang!r}. Supported languages: {SUPPORTED_LANGUAGES}"
        )
    if size not in SIZES:
        raise ValueError(f"Unknown profile size {size!r}. Available sizes: {SIZES}")
    if variant not in VARIANTS:
        raise ValueError(f"Unknown variant {variant!r}. Available variants: {VARIANTS}")


def load_stemmer(
    lang: str,
    size: str = DEFAULT_SIZE,
    variant: str = DEFAULT_VARIANT,
) -> ModuleType:
    """Dynamically load a precompiled LemmaJet stemmer module.

    Args:
        lang: Target language code ('pl', 'en', 'de', 'es', 'fr', 'cs').
        size: Profile size ('xxs', 'xs', 'sm', 'md', 'lg', 'xl'). Default is 'md'.
        variant: Operating point ('precision', 'balanced', 'recall'). Default is 'balanced'.

    Returns:
        Imported Python module containing `stem` and `stem_batch` functions.
    """
    _validate_profile(lang, size, variant)
    return importlib.import_module(f"lemmajet.stemmer.{lang}.{size}_{variant}")


def load_lemmatizer(
    lang: str,
    size: str = DEFAULT_SIZE,
    variant: str = DEFAULT_VARIANT,
) -> ModuleType:
    """Dynamically load a precompiled LemmaJet lemmatizer module.

    Args:
        lang: Target language code ('pl', 'en', 'de', 'es', 'fr', 'cs').
        size: Profile size ('xxs', 'xs', 'sm', 'md', 'lg', 'xl'). Default is 'md'.
        variant: Operating point ('precision', 'balanced', 'recall'). Default is 'balanced'.

    Returns:
        Imported Python module containing `lemmatize` and `lemmatize_batch` functions.
    """
    _validate_profile(lang, size, variant)
    return importlib.import_module(f"lemmajet.lemmatizer.{lang}.{size}_{variant}")


def load(
    lang: str,
    mode: str = "stemmer",
    size: str = DEFAULT_SIZE,
    variant: str = DEFAULT_VARIANT,
) -> ModuleType:
    """Dynamically load a precompiled LemmaJet model by mode.

    Args:
        lang: Target language code ('pl', 'en', 'de', 'es', 'fr', 'cs').
        mode: Model mode ('stemmer', 'stem', 'lemmatizer', 'lemma'). Default is 'stemmer'.
        size: Profile size ('xxs', 'xs', 'sm', 'md', 'lg', 'xl'). Default is 'md'.
        variant: Operating point ('precision', 'balanced', 'recall'). Default is 'balanced'.

    Returns:
        Imported Python module.

    Raises:
        ValueError: If mode is not recognized.
    """
    norm_mode = mode.lower().strip()
    if norm_mode in ("stem", "stemmer"):
        return load_stemmer(lang, size=size, variant=variant)
    if norm_mode in ("lemma", "lemmatizer"):
        return load_lemmatizer(lang, size=size, variant=variant)
    raise ValueError(
        f"Unknown mode {mode!r}. Available modes: 'stemmer' (or 'stem'), 'lemmatizer' (or 'lemma')"
    )


__all__ = [
    "DEFAULT_PROFILE",
    "DEFAULT_SIZE",
    "DEFAULT_VARIANT",
    "MODES",
    "SIZES",
    "SUPPORTED_LANGUAGES",
    "VARIANTS",
    "__version__",
    "load",
    "load_lemmatizer",
    "load_stemmer",
]
