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
from lemmajet.stemmer.pl import stem, stem_batch

print(stem("programistami"))  # -> 'programist'
print(stem_batch(["książka", "książkami"]))  # -> ['książk', 'książk']

# Lemmatizer
from lemmajet.lemmatizer.pl import lemmatize, lemmatize_batch

print(lemmatize("poszliśmy"))  # -> 'pójść'
print(lemmatize_batch(["koty", "psami"]))  # -> ['kot', 'pies']
```

### 2. Specific Profile

You can import specific Pareto operating points directly:

```python
# Maximum recall stemmer for English
from lemmajet.stemmer.en.xl_recall import stem

print(stem("relational"))

# High precision lemmatizer for German
from lemmajet.lemmatizer.de.lg_precision import lemmatize

print(lemmatize("Häuser"))
```

### 3. Dynamic Model Loading

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

## License

MIT License. Copyright (c) 2026 Michal Šrajer and LemmaJet Contributors.

