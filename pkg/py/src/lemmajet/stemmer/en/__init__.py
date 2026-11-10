"""Language root re-export for en default stemmer profile."""

from .md_balanced import (
    Stemmer,
    EnglishStemmer,
    clear_cache,
    stem,
    stem_word,
    stem_words,
    stemWord,
    stemWords,
)

LANGUAGE = "en"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Stemmer",
    "EnglishStemmer",
    "clear_cache",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
