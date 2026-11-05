<div align="center">
  <a href="https://lemmajet.org">
    <img src="https://lemmajet.org/assets/img/logo.svg" alt="LemmaJet Logo" width="96" height="96">
  </a>

  # LemmaJet

  **Lightweight as a stemmer. Accurate as a lemmatizer.**

  [**Live Interactive Demo**](https://lemmajet.org/#demo) · [**Benchmarks (6 Languages)**](https://lemmajet.org/benchmarks/) · [**Architecture & Docs**](https://lemmajet.org/)
</div>

---

LemmaJet is a corpus-driven compiler for micro-stemmers and micro-lemmatizers. Heavy morphological processing—finite-state lexicons (PoliMorf, UniMorph), neural taggers (Stanza, spaCy), and contextual LLM tie-breakers—runs once, offline, over a target corpus.

What you ship to production is a tiny, deterministic, zero-dependency function that maps inflected words to their canonical dictionary headwords (`lemma` mode) or morphological stems (`stem` mode), handling irregular verbs, umlaut shifts, and suppletive forms that rule-based stemmers miss.

![LemmaJet Benchmark — English General (Lemma Mode)](https://lemmajet.org/assets/reports/en/general/lemma_benchmark_plot.svg)

---

## Key Characteristics

* **Snowball speed, neural precision:** Matches or exceeds Stanza's lemmatization accuracy on evaluated corpora while running hundreds of times faster on a single CPU core (~1,000,000+ words/sec in pure Python and JavaScript).
* **Tiny footprint, zero dependencies:** Compiled models range from **1.5 KB to 88 KB gzipped** (`md-balanced` profile). No PyTorch, no model weights to download at runtime, no C/Cython extensions.
* **True dictionary headwords or stems:** Compile models in `lemma` mode (e.g., `went -> go`, `mice -> mouse`, `ludzie -> człowiek`, `gingen -> gehen`) or `stem` mode for compact search indexing.
* **Multi-target code generation:**
  * **JavaScript / TypeScript:** Standalone ES Module (`.js` + `.d.ts`) for browsers, Node.js, and edge runtimes.
  * **Python:** Single zero-dependency `.py` module with bit-for-bit identical output to the JS target.
  * **Snowball DSL (`.sbl`):** Transpiles the compiled model into valid Snowball source code, enabling compilation to native C (PostgreSQL `tsvector`) or Java (Apache Lucene / Elasticsearch).
* **Six languages supported today:** English (`en`), Polish (`pl`), German (`de`), Spanish (`es`), French (`fr`), and Czech (`cs`), across 18 Pareto-calibrated size and objective profiles (`xxs` through `xl`).

---

## Try It Now

While the repository code is being finalized for public release, you can test the compiled models and inspect the full evaluation reports directly in your browser:

1. **Interactive Live Demo (EN, PL, DE, ES, FR, CS):** [https://lemmajet.org/#demo](https://lemmajet.org/#demo)  
   *Runs the actual compiled ES Modules client-side in your browser alongside official Snowball baselines.*
2. **Reproducible Benchmarks & Pareto Charts:** [https://lemmajet.org/benchmarks/](https://lemmajet.org/benchmarks/)

---

## Target API Preview

### JavaScript / TypeScript (ES Module)

```javascript
import { lemmatize, lemmatizeBatch } from './lemmatizer_en_md-balanced.js';

lemmatize('children');   // -> 'child'
lemmatize('went');       // -> 'go'
lemmatize('phenomena');  // -> 'phenomenon'

lemmatizeBatch(['mice', 'were', 'running', 'faster']);
// -> ['mouse', 'be', 'run', 'fast']
```

### Python (Zero-Dependency Module)

```python
from lemmatizer_pl_md_balanced import lemmatize, lemmatize_batch

lemmatize("ludziom")       # -> "człowiek"
lemmatize("poszliśmy")     # -> "pójść"
lemmatize("najlepszych")   # -> "dobry"

lemmatize_batch(["dzieci", "jadły", "dojrzałe", "jabłka"])
# -> ["dziecko", "jeść", "dojrzały", "jabłko"]
```

---

## How the Compiler Works

LemmaJet compiles models through a 4-stage pipeline:

1. **Stage 1 — Lexicon-Constrained Ensemble Teacher:** Extracts `(word, lemma, tf, df)` statistics from a domain corpus using finite-state morphological dictionaries, dual neural teachers (Stanza + spaCy) weighted by 2-Pass Expectation-Maximization (EM) family mass priors, and a context-grounded local LLM adjudicator for contested homographs.
2. **Stage 2 — Reversed Suffix-Trie & MDL Pruning:** Induces canonical targets and builds a Patricia/Radix-compressed reversed character trie of `CutAdd(cut, add)` operations, pruned bottom-up via Minimum Description Length (MDL) cost-complexity penalties.
3. **Stage 3 — Suppletive Stem Bridging:** Extracts residual irregulars into an $O(1)$ exception table and bridges regular inflections of suppletive roots (e.g., `lepsz- -> dobry`) so a single bridge entry covers an entire irregular inflectional family.
4. **Stage 4 — Multi-Target Emission:** Serializes the pruned automaton into standalone `.js`, `.d.ts`, `.py`, and `.sbl` source files.

---

## Release Roadmap & Status

We are rolling out the open-source artifacts in three stages over the coming days:

- [x] **Stage 1 (Live):** Interactive browser demo and multi-language benchmark suite at [lemmajet.org](https://lemmajet.org).
- [ ] **Stage 2 (Up next):** Precompiled zero-dependency lemmatizers and stemmers for all 6 languages (`npm` and `PyPI` packages + standalone file downloads).
- [ ] **Stage 3:** Full `lemmajet` Python compiler CLI, Pareto calibration suite, and Snowball `.sbl` emitter source code.

**Star or Watch this repository** to get notified as soon as the precompiled packages and compiler source code are pushed.

---

## License

MIT License. See [lemmajet.org](https://lemmajet.org) for details.