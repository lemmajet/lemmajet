/**
 * LemmaJet: Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers.
 *
 * Supported languages: pl, en, de, es, fr, cs.
 */

export const VERSION = "0.6.0";
export const SUPPORTED_LANGUAGES = ["pl", "en", "de", "es", "fr", "cs"];
export const SIZES = ["xxs", "xs", "sm", "md", "lg", "xl"];
export const VARIANTS = ["precision", "balanced", "recall"];
export const MODES = ["stemmer", "lemmatizer"];
export const DEFAULT_SIZE = "md";
export const DEFAULT_VARIANT = "balanced";
export const DEFAULT_PROFILE = "md_balanced";

export default {
  VERSION,
  SUPPORTED_LANGUAGES,
  SIZES,
  VARIANTS,
  MODES,
  DEFAULT_SIZE,
  DEFAULT_VARIANT,
  DEFAULT_PROFILE,
};

