"""Language root re-export for fr default stemmer profile."""

from .md_balanced import stem, stem_batch

LANGUAGE = "fr"
DEFAULT_PROFILE = "md_balanced"

__all__ = ["DEFAULT_PROFILE", "LANGUAGE", "stem", "stem_batch"]
