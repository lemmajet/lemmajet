"""Language root re-export for es default stemmer profile."""

from .md_balanced import (
    Stemmer,
    SpanishStemmer,
    clear_cache,
    stem,
    stem_word,
    stem_words,
    stemWord,
    stemWords,
)

LANGUAGE = "es"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Stemmer",
    "SpanishStemmer",
    "clear_cache",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
