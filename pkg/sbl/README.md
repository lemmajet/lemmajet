# LemmaJet Snowball (.sbl) Source Distribution

Ultra-fast, zero-dependency micro-stemmers and micro-lemmatizers emitted in the official
[Snowball DSL](https://snowballstem.org/).

## Directory Structure

```text
pkg/sbl/
├── README.md
├── LICENSE
├── Makefile
├── stemmer/
│   ├── pl/
│   │   ├── md_balanced.sbl  # Default Polish stemmer profile
│   │   └── ... (18 profiles per language)
│   └── ... (en, de, es, fr, cs)
└── lemmatizer/
    ├── pl/
    │   ├── md_balanced.sbl  # Default Polish lemmatizer profile
    │   └── ... (18 profiles per language)
    └── ... (en, de, es, fr, cs)
```

## Compilation to Target Languages

You can transpile any `.sbl` algorithm using the official Snowball compiler:

```bash
# Compile to C (emits <base>.c and <base>.h next to -o base)
snowball stemmer/pl/md_balanced.sbl -o md_balanced

# Compile to Rust (emits <base>.rs)
snowball stemmer/pl/md_balanced.sbl -rust -o md_balanced

# Compile to Java (emits <base>.java)
snowball stemmer/pl/md_balanced.sbl -java -o md_balanced

# Compile to Go (emits <base>.go)
snowball stemmer/pl/md_balanced.sbl -go -o md_balanced
```

Note: `-o` takes an **output base name** — Snowball appends the target-language
extension itself. C is the default target, so there is no `-c` flag; the header is
written automatically as `<base>.h` (there is no `-h <file>` output option; `-hheader`
only sets the `#include` name baked into the generated header).

Or run the included Makefile for batch generation:

```bash
make c
make rust
make java
make go
```

## License

MIT License. Copyright (c) 2026 Michal Šrajer and LemmaJet Contributors.
