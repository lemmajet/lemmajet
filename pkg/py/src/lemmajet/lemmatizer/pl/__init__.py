"""Language root re-export for pl default lemmatizer profile."""

from .md_balanced import (
    Lemmatizer,
    PolishLemmatizer,
    Stemmer,
    PolishStemmer,
    clear_cache,
    lemmatize,
    lemmatize_word,
    lemmatize_words,
    lemmatizeWord,
    lemmatizeWords,
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
    "Lemmatizer",
    "PolishLemmatizer",
    "Stemmer",
    "PolishStemmer",
    "clear_cache",
    "lemmatize",
    "lemmatizeWord",
    "lemmatize_word",
    "lemmatizeWords",
    "lemmatize_words",
    "stem",
    "stemWord",
    "stem_word",
    "stemWords",
    "stem_words",
]
