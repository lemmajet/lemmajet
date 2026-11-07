"""Language root re-export for fr default lemmatizer profile."""

from .md_balanced import lemmatize, lemmatize_batch

LANGUAGE = "fr"
DEFAULT_PROFILE = "md_balanced"

__all__ = ["DEFAULT_PROFILE", "LANGUAGE", "lemmatize", "lemmatize_batch"]
