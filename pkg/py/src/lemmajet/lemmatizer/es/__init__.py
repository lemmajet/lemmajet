"""Language root re-export for es default lemmatizer profile."""

from .md_balanced import (
    Lemmatizer,
    SpanishLemmatizer,
    Stemmer,
    SpanishStemmer,
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

LANGUAGE = "es"
DEFAULT_PROFILE = "md_balanced"

__all__ = [
    "DEFAULT_PROFILE",
    "LANGUAGE",
    "Lemmatizer",
    "SpanishLemmatizer",
    "Stemmer",
    "SpanishStemmer",
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
