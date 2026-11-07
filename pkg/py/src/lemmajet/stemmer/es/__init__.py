"""Language root re-export for es default stemmer profile."""

from .md_balanced import stem, stem_batch

LANGUAGE = "es"
DEFAULT_PROFILE = "md_balanced"

__all__ = ["DEFAULT_PROFILE", "LANGUAGE", "stem", "stem_batch"]
