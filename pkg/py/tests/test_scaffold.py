"""Unit tests for LemmaJet Python runtime scaffold and metadata."""

from unittest.mock import MagicMock, patch

import pytest

import lemmajet


def test_metadata_constants() -> None:
    assert lemmajet.__version__ == "0.7.0"
    assert lemmajet.SUPPORTED_LANGUAGES == ("pl", "en", "de", "es", "fr", "cs")
    assert lemmajet.SIZES == ("xxs", "xs", "sm", "md", "lg", "xl")
    assert lemmajet.VARIANTS == ("precision", "balanced", "recall")
    assert lemmajet.MODES == ("stemmer", "lemmatizer")
    assert lemmajet.DEFAULT_SIZE == "md"
    assert lemmajet.DEFAULT_VARIANT == "balanced"
    assert lemmajet.DEFAULT_PROFILE == "md_balanced"


def test_validation_errors() -> None:
    with pytest.raises(ValueError, match="Unsupported language 'xx'"):
        lemmajet.load_stemmer("xx")

    with pytest.raises(ValueError, match="Unknown profile size 'huge'"):
        lemmajet.load_stemmer("pl", size="huge")

    with pytest.raises(ValueError, match="Unknown variant 'fast'"):
        lemmajet.load_stemmer("pl", variant="fast")

    with pytest.raises(ValueError, match="Unknown mode 'unknown'"):
        lemmajet.load("pl", mode="unknown")


def test_dynamic_loader_dispatches() -> None:
    mock_module = MagicMock()

    with patch("importlib.import_module", return_value=mock_module) as mock_import:
        # Default profile stemmer
        res = lemmajet.load_stemmer("pl")
        assert res is mock_module
        mock_import.assert_called_with("lemmajet.stemmer.pl.md_balanced")

        # Explicit profile stemmer
        lemmajet.load_stemmer("en", size="xl", variant="recall")
        mock_import.assert_called_with("lemmajet.stemmer.en.xl_recall")

        # Explicit profile lemmatizer
        lemmajet.load_lemmatizer("de", size="sm", variant="precision")
        mock_import.assert_called_with("lemmajet.lemmatizer.de.sm_precision")

        # Unified load function with mode aliases
        lemmajet.load("fr", mode="stem", size="xxs", variant="balanced")
        mock_import.assert_called_with("lemmajet.stemmer.fr.xxs_balanced")

        lemmajet.load("cs", mode="lemma")
        mock_import.assert_called_with("lemmajet.lemmatizer.cs.md_balanced")

        # Case and whitespace normalization
        lemmajet.load("pl", mode="  STEMMER  ")
        mock_import.assert_called_with("lemmajet.stemmer.pl.md_balanced")

        lemmajet.load("en", mode="LEMMATIZER")
        mock_import.assert_called_with("lemmajet.lemmatizer.en.md_balanced")
