"""Language root re-export for de default stemmer profile."""

from .md_balanced import (
    Stemmer,
    GermanStemmer,
    clear_cache,
    stem,
    stem_word,
    stem_words,
    stemWord,
    stemWords,
)

LANGUAGE = "de"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Stemmer",
    "GermanStemmer",
    "clear_cache",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
