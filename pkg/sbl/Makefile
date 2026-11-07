# LemmaJet Snowball batch transpilation Makefile
SNOWBALL ?= snowball
#SBL_FILES := $(shell find stemmer lemmatizer -name '*.sbl' 2>/dev/null)
SBL_FILES := $(shell find stemmer lemmatizer -name 'md_balanced.sbl' 2>/dev/null)

.PHONY: all c rust java go clean help

all: help

help:
	@echo "LemmaJet Snowball (.sbl) Build Targets:"
	@echo "  make c     - Compile .sbl files to C (.c and .h)"
	@echo "  make rust  - Compile .sbl files to Rust (.rs)"
	@echo "  make java  - Compile .sbl files to Java (.java)"
	@echo "  make go    - Compile .sbl files to Go (.go)"

c:
	@for f in $(SBL_FILES); do \
		dir=$$(dirname "$$f"); \
		base=$$(basename "$$f" .sbl); \
		echo "Compiling $$f -> $$dir/$$base.c"; \
		$(SNOWBALL) "$$f" -o "$$dir/$$base" -utf8 || exit 1; \
	done

rust:
	@for f in $(SBL_FILES); do \
		dir=$$(dirname "$$f"); \
		base=$$(basename "$$f" .sbl); \
		echo "Compiling $$f -> $$dir/$$base.rs"; \
		$(SNOWBALL) "$$f" -rust -o "$$dir/$$base" || exit 1; \
	done

java:
	@for f in $(SBL_FILES); do \
		dir=$$(dirname "$$f"); \
		base=$$(basename "$$f" .sbl); \
		echo "Compiling $$f -> $$dir/$$base.java"; \
		$(SNOWBALL) "$$f" -java -o "$$dir/$$base" || exit 1; \
	done

go:
	@for f in $(SBL_FILES); do \
		dir=$$(dirname "$$f"); \
		base=$$(basename "$$f" .sbl); \
		echo "Compiling $$f -> $$dir/$$base.go"; \
		$(SNOWBALL) "$$f" -go -o "$$dir/$$base" || exit 1; \
	done
