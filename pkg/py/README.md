# LemmaJet

Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers for 6 languages: Polish (`pl`), English (`en`), German (`de`), Spanish (`es`), French (`fr`), and Czech (`cs`).

[![PyPI](https://img.shields.io/pypi/v/lemmajet.svg)](https://pypi.org/project/lemmajet/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Website: lemmajet.org](https://img.shields.io/badge/Website-lemmajet.org-green.svg)](https://lemmajet.org)

---

## Key Features

- **Zero Runtime Dependencies**: Pure Python code with no external dependencies whatsoever.
- **Sub-Microsecond Latency**: Highly optimized suffix-trie and DAG evaluation with instant cold-start.
- **Dual Operating Modes**:
  - **Stemmers** (`lemmajet.stemmer.<lang>`): Equivalence class reductions for high-recall BM25 / search indexing.
  - **Lemmatizers** (`lemmajet.lemmatizer.<lang>`): Dictionary-canonical dictionary forms for NLP, NER, and entity normalization.
- **Pareto Trade-Off Profiles**: 6 footprint sizes (`xxs` through `xl`) across 3 optimization variants (`precision`, `balanced`, `recall`).

---

## Installation

```bash
pip install lemmajet
```

---

## Quickstart

### 1. Default Profile (Recommended)

Default profiles point to `md_balanced` (medium footprint, optimal balance of precision and recall):

```python
# Stemmer
from lemmajet.stemmer.pl import stem, stem_words

print(stem("programistami"))  # -> 'programist'
print(stem_words(["książka", "książkami"]))  # -> ['książk', 'książk']

# Lemmatizer
from lemmajet.lemmatizer.pl import lemmatize, lemmatize_words

print(lemmatize("poszliśmy"))  # -> 'pójść'
print(lemmatize_words(["koty", "psami"]))  # -> ['kot', 'pies']
```

### 2. Snowball Drop-In Compatibility

LemmaJet provides 100% API compatibility with `snowballstemmer`:

```python
# Import language-specific class or Snowball-compatible functions:
from lemmajet.stemmer.en import EnglishStemmer, stemWord, stemWords

# 1. Snowball-style functions (camelCase and snake_case supported):
print(stemWord("running"))  # -> 'run'
print(stemWords(["running", "faster"]))  # -> ['run', 'fast']

# 2. Snowball class instance (identical interface to snowballstemmer):
stemmer = EnglishStemmer()
print(stemmer.stemWord("running"))  # -> 'run'
print(stemmer.stemWords(["cats", "dogs"]))  # -> ['cat', 'dog']

# Lemmatizers also support the Snowball interface as drop-in upgrades:
from lemmajet.lemmatizer.en import EnglishStemmer as EnglishLemmatizer

lemmatizer = EnglishLemmatizer()
print(lemmatizer.stemWord("went"))  # -> 'go' (true headword lemma!)
print(lemmatizer.stemWords(["mice", "went"]))  # -> ['mouse', 'go']
```

### 3. Specific Profile

You can import specific Pareto operating points directly:

```python
# Maximum recall stemmer for English
from lemmajet.stemmer.en.xl_recall import stem

print(stem("relational"))

# High precision lemmatizer for German
from lemmajet.lemmatizer.de.lg_precision import lemmatize

print(lemmatize("Häuser"))
```

### 4. Dynamic Model Loading

Load models programmatically by language, mode, size, and variant:

```python
import lemmajet

# Load stemmer
stemmer = lemmajet.load_stemmer("pl", size="md", variant="balanced")
print(stemmer.stem("ludzie"))

# Load lemmatizer
lemmatizer = lemmajet.load_lemmatizer("de", size="lg", variant="recall")
print(lemmatizer.lemmatize("Bücher"))

# Unified loader
mod = lemmajet.load("es", mode="stemmer", size="sm", variant="balanced")
print(mod.stem("estudiantes"))
```

---

## Supported Languages & Profiles

| Language Code | Language | Default Profile |
| :--- | :--- | :--- |
| `pl` | Polish | `md_balanced` |
| `en` | English | `md_balanced` |
| `de` | German | `md_balanced` |
| `es` | Spanish | `md_balanced` |
| `fr` | French | `md_balanced` |
| `cs` | Czech | `md_balanced` |

### Profile Footprints & Variants

- **Sizes**: `xxs` (~5 KB), `xs` (~15 KB), `sm` (~40 KB), `md` (~120 KB), `lg` (~350 KB), `xl` (~1 MB)
- **Variants**: `precision` ($F_{0.5}$), `balanced` ($F_{1.0}$), `recall` ($F_{2.0}$)

---

## Architecture & Optimization Internals

LemmaJet models execute a unified, high-performance 4-stage inference pipeline:
1. **Stage 1 (Surface Exception Lookup)**: $O(1)$ fast path for irregular and suppletive forms.
2. **Stage 2 (Prefix Trie & Enclitic Stripping)**: Longest-match forward prefix stripping with protective overrides for false prefixes.
3. **Stage 3 (Reversed Suffix-Trie Transducer)**: High-speed reversed character matching computing the longest valid `(cut, add)` rule.
4. **Stage 3.5 (Synthetic Stem-Bridge Lookup)**: Unifies stemming and lemmatization by resolving intermediate regularized stems to canonical dictionary lemmas in $O(1)$.

For deep technical specifications, bytecode analysis, and VM optimization details, see the bundled architecture guides:
- [`Model Inference & Stem-Bridge Architecture`](docs/model-inference-and-bridge-architecture.md): Language-agnostic specification of the universal 4-stage inference engine and stem bridging.
- [`Python Runtime & Bytecode Optimizations`](docs/python-emitter-optimizations.md): CPython bytecode, inverted radix search, negative slicing, and caching techniques.

---

## License

MIT License. Copyright (c) 2026 Michal Šrajer and LemmaJet Contributors.

