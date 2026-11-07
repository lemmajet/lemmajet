"""Language root re-export for de default stemmer profile."""

from .md_balanced import stem, stem_batch

LANGUAGE = "de"
DEFAULT_PROFILE = "md_balanced"

__all__ = ["DEFAULT_PROFILE", "LANGUAGE", "stem", "stem_batch"]
