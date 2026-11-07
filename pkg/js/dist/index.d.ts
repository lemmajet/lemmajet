/**
 * LemmaJet: Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers.
 */

export const VERSION: string;
export const SUPPORTED_LANGUAGES: readonly ["pl", "en", "de", "es", "fr", "cs"];
export const SIZES: readonly ["xxs", "xs", "sm", "md", "lg", "xl"];
export const VARIANTS: readonly ["precision", "balanced", "recall"];
export const MODES: readonly ["stemmer", "lemmatizer"];
export const DEFAULT_SIZE: "md";
export const DEFAULT_VARIANT: "balanced";
export const DEFAULT_PROFILE: "md_balanced";

export type LanguageCode = "pl" | "en" | "de" | "es" | "fr" | "cs";
export type ProfileSize = "xxs" | "xs" | "sm" | "md" | "lg" | "xl";
export type ProfileVariant = "precision" | "balanced" | "recall";
export type ModelMode = "stemmer" | "lemmatizer" | "stem" | "lemma";

export type StemmerFn = (word: string) => string;
export type BatchStemmerFn = (words: string[]) => string[];
export type LemmatizerFn = (word: string) => string;
export type BatchLemmatizerFn = (words: string[]) => string[];

export interface StemmerModule {
  stem: StemmerFn;
  stemBatch: BatchStemmerFn;
  default: StemmerFn;
}

export interface LemmatizerModule {
  lemmatize: LemmatizerFn;
  lemmatizeBatch: BatchLemmatizerFn;
  default: LemmatizerFn;
}

declare const _default: {
  VERSION: typeof VERSION;
  SUPPORTED_LANGUAGES: typeof SUPPORTED_LANGUAGES;
  SIZES: typeof SIZES;
  VARIANTS: typeof VARIANTS;
  MODES: typeof MODES;
  DEFAULT_SIZE: typeof DEFAULT_SIZE;
  DEFAULT_VARIANT: typeof DEFAULT_VARIANT;
  DEFAULT_PROFILE: typeof DEFAULT_PROFILE;
};

export default _default;

