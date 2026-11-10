"""Language root re-export for fr default stemmer profile."""

from .md_balanced import (
    Stemmer,
    FrenchStemmer,
    clear_cache,
    stem,
    stem_word,
    stem_words,
    stemWord,
    stemWords,
)

LANGUAGE = "fr"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Stemmer",
    "FrenchStemmer",
    "clear_cache",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
