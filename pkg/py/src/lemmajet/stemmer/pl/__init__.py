"""Language root re-export for pl default stemmer profile."""

from .md_balanced import (
    Stemmer,
    PolishStemmer,
    clear_cache,
    stem,
    stem_word,
    stem_words,
    stemWord,
    stemWords,
)

LANGUAGE = "pl"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Stemmer",
    "PolishStemmer",
    "clear_cache",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
