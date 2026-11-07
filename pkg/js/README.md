# LemmaJet

Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers for JavaScript and TypeScript across 6 languages: Polish (`pl`), English (`en`), German (`de`), Spanish (`es`), French (`fr`), and Czech (`cs`).

[![NPM](https://img.shields.io/npm/v/lemmajet.svg)](https://www.npmjs.com/package/lemmajet)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Website: lemmajet.org](https://img.shields.io/badge/Website-lemmajet.org-green.svg)](https://lemmajet.org)

---

## Key Features

- **Zero Runtime Dependencies**: Pure ES modules with no external dependencies whatsoever.
- **100% Tree-Shakable**: Subpath exports ensure your bundler (Vite, Rollup, Webpack, esbuild) includes only the exact language and profile you import. Unused profiles add 0 bytes to client bundles.
- **Sub-Microsecond Latency**: Pure JavaScript radix suffix-trie and DAG evaluation with instant cold-start in browser or Node.js.
- **Dual Operating Modes**:
  - **Stemmers** (`lemmajet/stemmer/<lang>`): High-recall equivalence class clustering for client-side and full-text search indexing.
  - **Lemmatizers** (`lemmajet/lemmatizer/<lang>`): Exact canonical dictionary forms for client NLP and entity normalization.
- **TypeScript First**: Full `.d.ts` type definitions included with typed signatures.

---

## Installation

```bash
npm install lemmajet
```

---

## Quickstart

### 1. Default Profiles (Recommended)

Default profiles point to `md_balanced` (medium footprint, optimal balance of precision and recall):

```javascript
// Stemmer (ESM / TypeScript)
import { stem, stemBatch } from "lemmajet/stemmer/pl";

console.log(stem("programistami")); // -> 'programist'
console.log(stemBatch(["książka", "książkami"])); // -> ['książk', 'książk']

// Lemmatizer (ESM / TypeScript)
import { lemmatize, lemmatizeBatch } from "lemmajet/lemmatizer/pl";

console.log(lemmatize("poszliśmy")); // -> 'pójść'
console.log(lemmatizeBatch(["koty", "psami"])); // -> ['kot', 'pies']
```

Default exports are also supported:

```javascript
import stem from "lemmajet/stemmer/en";
console.log(stem("running")); // -> 'run'

import lemmatize from "lemmajet/lemmatizer/de";
console.log(lemmatize("Häuser")); // -> 'Haus'
```

### 2. Specific Pareto Profile

Import any specific Pareto operating point directly:

```javascript
// High-recall Polish stemmer
import { stem } from "lemmajet/stemmer/pl/xl_recall";

// Compact French lemmatizer
import { lemmatize } from "lemmajet/lemmatizer/fr/xs_balanced";
```

### 3. Metadata Constants

```javascript
import { VERSION, SUPPORTED_LANGUAGES, DEFAULT_PROFILE } from "lemmajet";

console.log(`LemmaJet v${VERSION}`);
console.log(`Supported: ${SUPPORTED_LANGUAGES.join(", ")}`);
```

---

## Tree-Shaking Guarantee

`lemmajet` marks `"sideEffects": false` and provides dedicated subpath exports. When you import `lemmajet/stemmer/en`, modern bundlers will only bundle the ~100 KB English stemmer module. No other languages or lemmatizer tables are loaded into your bundle.

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

