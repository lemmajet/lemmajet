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
export type WordStemmerFn = (word: string) => string;
export type WordsStemmerFn = (words: readonly string[]) => string[];
export type LemmatizerFn = (word: string) => string;
export type WordLemmatizerFn = (word: string) => string;
export type WordsLemmatizerFn = (words: readonly string[]) => string[];
export type ClearCacheFn = () => void;

export class Stemmer {
  constructor(options?: { cacheSize?: number });
  static stem(word: string): string;
  static stemWord(word: string): string;
  static stemWords(words: readonly string[]): string[];
  static clearCache(): void;

  stem(word: string): string;
  stemWord(word: string): string;
  stemWords(words: readonly string[]): string[];
  clearCache(): void;
}

export class Lemmatizer {
  constructor(options?: { cacheSize?: number });
  static lemmatize(word: string): string;
  static lemmatizeWord(word: string): string;
  static lemmatizeWords(words: readonly string[]): string[];
  static stem(word: string): string;
  static stemWord(word: string): string;
  static stemWords(words: readonly string[]): string[];
  static clearCache(): void;

  lemmatize(word: string): string;
  lemmatizeWord(word: string): string;
  lemmatizeWords(words: readonly string[]): string[];
  stem(word: string): string;
  stemWord(word: string): string;
  stemWords(words: readonly string[]): string[];
  clearCache(): void;
}

export interface CallableStemmer {
  (word: string): string;
  new (options?: { cacheSize?: number }): Stemmer;
  stem: StemmerFn;
  stemWord: WordStemmerFn;
  stemWords: WordsStemmerFn;
  clearCache: ClearCacheFn;
}

export interface CallableLemmatizer {
  (word: string): string;
  new (options?: { cacheSize?: number }): Lemmatizer;
  lemmatize: LemmatizerFn;
  lemmatizeWord: WordLemmatizerFn;
  lemmatizeWords: WordsLemmatizerFn;
  stem: StemmerFn;
  stemWord: WordStemmerFn;
  stemWords: WordsStemmerFn;
  clearCache: ClearCacheFn;
}

export interface StemmerModule {
  stem: StemmerFn;
  stemWord: WordStemmerFn;
  stemWords: WordsStemmerFn;
  clearCache: ClearCacheFn;
  Stemmer: typeof Stemmer;
  default: CallableStemmer;
}

export interface LemmatizerModule {
  lemmatize: LemmatizerFn;
  lemmatizeWord: WordLemmatizerFn;
  lemmatizeWords: WordsLemmatizerFn;
  stem: StemmerFn;
  stemWord: WordStemmerFn;
  stemWords: WordsStemmerFn;
  clearCache: ClearCacheFn;
  Lemmatizer: typeof Lemmatizer;
  Stemmer: typeof Lemmatizer;
  default: CallableLemmatizer;
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

