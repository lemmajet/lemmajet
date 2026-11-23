# LemmaJet SQLite FTS5 Extension Makefile (Offline)
#
# Usage:
#   make SQLITE_SRC=/path/to/sqlite-src
#   make combined SQLITE_SRC=/path/to/sqlite-src LANGS="pl en" PROFILES="sm_balanced md_balanced"
#   make combined SQLITE_SRC=/path/to/sqlite-src MODELS="pl:sm_balanced en:md_balanced"
#   make model SQLITE_SRC=/path/to/sqlite-src LANG=pl PROFILE=sm_balanced
#   make models SQLITE_SRC=/path/to/sqlite-src PROFILES="sm_balanced md_balanced"
#   make test SQLITE_SRC=/path/to/sqlite-src

CC ?= clang
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O3 -fPIC
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    SO_EXT := dylib
    SHARED_FLAGS := -dynamiclib -undefined dynamic_lookup
else
    SO_EXT := so
    SHARED_FLAGS := -shared -fPIC -Wl,-Bsymbolic
endif

ifdef SQLITE_SRC
    SQLITE_INC := $(SQLITE_SRC)
endif

ifndef SQLITE_INC
    PKG_INC := $(shell pkg-config --cflags-only-I sqlite3 2>/dev/null | sed 's/-I//g')
    ifneq ($(PKG_INC),)
        SQLITE_INC := $(firstword $(PKG_INC))
    else ifneq ($(wildcard /usr/include/fts5.h),)
        SQLITE_INC := /usr/include
    else ifneq ($(wildcard /usr/local/include/fts5.h),)
        SQLITE_INC := /usr/local/include
    else ifneq ($(wildcard /opt/homebrew/include/fts5.h),)
        SQLITE_INC := /opt/homebrew/include
    endif
endif

ifndef SQLITE_CLI
    ifdef SQLITE_SRC
        ifneq ($(wildcard $(SQLITE_SRC)/sqlite3),)
            SQLITE_CLI := $(SQLITE_SRC)/sqlite3
        endif
    endif
endif
ifndef SQLITE_CLI
    SQLITE_CLI := $(shell command -v sqlite3 2>/dev/null)
endif

CORE_DIR := ..
BUILD_DIR := build
COMBINED_TARGET := $(BUILD_DIR)/lemmajet_fts5.$(SO_EXT)

LANGS ?= pl en de es fr cs
PROFILES ?= md_balanced
MODELS ?=
DOCKER_IMAGE ?= lemmajet-sqlite:latest
BENCH_ARGS ?=

# Resolve active (lang:profile) pairs from MODELS or LANGS x PROFILES
ifneq ($(strip $(MODELS)),)
    RAW_PAIRS := $(subst /, :,$(MODELS))
    COMBINED_PAIRS := $(strip $(foreach pair,$(subst /,:,$(MODELS)),\
        $(if $(and $(wildcard $(CORE_DIR)/stemmer/$(word 1,$(subst :, ,$(pair)))/$(word 2,$(subst :, ,$(pair))).h),\
                   $(wildcard $(CORE_DIR)/lemmatizer/$(word 1,$(subst :, ,$(pair)))/$(word 2,$(subst :, ,$(pair))).h)),\
            $(pair),)))
else
    COMBINED_PAIRS := $(strip $(foreach l,$(LANGS),$(foreach p,$(PROFILES),\
        $(if $(and $(wildcard $(CORE_DIR)/stemmer/$(l)/$(p).h),$(wildcard $(CORE_DIR)/lemmatizer/$(l)/$(p).h)),$(l):$(p),))))
endif

COMBINED_LANGS := $(sort $(foreach pair,$(COMBINED_PAIRS),$(word 1,$(subst :, ,$(pair)))))
COMBINED_PROFILES := $(sort $(foreach pair,$(COMBINED_PAIRS),$(word 2,$(subst :, ,$(pair)))))

COMBINED_MODEL_OBJS := $(foreach pair,$(COMBINED_PAIRS),\
    $(CORE_DIR)/build/models/$(word 1,$(subst :, ,$(pair)))_stem_$(word 2,$(subst :, ,$(pair))).o \
    $(CORE_DIR)/build/models/$(word 1,$(subst :, ,$(pair)))_lemma_$(word 2,$(subst :, ,$(pair))).o)

COMBINED_DICT_DEFINES := $(foreach pair,$(COMBINED_PAIRS),\
    -DLEMMAJET_ENABLE_$(shell printf '%s_%s' '$(word 1,$(subst :, ,$(pair)))' '$(word 2,$(subst :, ,$(pair)))' | tr '[:lower:]-' '[:upper:]_')=1)

RUNTIME_OBJ := $(CORE_DIR)/build/lemmajet_runtime.o

# Per-model variables (used when LANG and PROFILE are set)
ifneq ($(and $(strip $(LANG)),$(strip $(PROFILE))),)
    MODEL_TARGET := $(BUILD_DIR)/lemmajet_fts5_$(LANG)_$(PROFILE).$(SO_EXT)
    MODEL_DICT_OBJ := $(BUILD_DIR)/lemmajet_dict_$(LANG)_$(PROFILE).o
    MODEL_STEM_OBJ := $(CORE_DIR)/build/models/$(LANG)_stem_$(PROFILE).o
    MODEL_LEMMA_OBJ := $(CORE_DIR)/build/models/$(LANG)_lemma_$(PROFILE).o
    MODEL_DICT_DEF := -DLEMMAJET_ENABLE_$(shell printf '%s_%s' '$(LANG)' '$(PROFILE)' | tr '[:lower:]-' '[:upper:]_')=1
    ENTRY_SUFFIX := $(shell printf 'lemmajet_fts5_%s_%s' '$(LANG)' '$(PROFILE)' | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z')
    ENTRY_NAME := sqlite3_$(ENTRY_SUFFIX)_init
    MODEL_DEFINES := \
        -DLJ_FTS5_ENTRY_NAME=$(ENTRY_NAME) \
        -DLJ_FTS5_TOK_NAME='"lemmajet_$(LANG)_$(PROFILE)"' \
        -DLJ_FTS5_MODEL_LANG='"$(LANG)"' \
        -DLJ_FTS5_MODEL_PROFILE='"$(PROFILE)"'
endif

.PHONY: all combined model models test check-sqlite check-model-args clean docker-build docker-test docker-bench help

all: combined

check-sqlite:
	@if [ -z "$(SQLITE_INC)" ] || [ ! -f "$(SQLITE_INC)/sqlite3ext.h" ] || [ ! -f "$(SQLITE_INC)/fts5.h" ]; then \
		echo "ERROR: SQLite headers (sqlite3ext.h, fts5.h) not found." >&2; \
		echo "  Checked SQLITE_INC='$(SQLITE_INC)'." >&2; \
		echo "  Pass SQLITE_SRC=/path/to/sqlite-src or SQLITE_INC=/path/to/include." >&2; \
		echo "  See docs/sqlite-fts5.md for build instructions." >&2; \
		exit 1; \
	fi

check-model-args:
	@if [ -z "$(LANG)" ] || [ -z "$(PROFILE)" ]; then \
		echo "ERROR: 'make model' requires LANG=<lang> PROFILE=<profile> (e.g. LANG=pl PROFILE=sm_balanced)." >&2; \
		exit 1; \
	fi
	@if [ ! -f "$(CORE_DIR)/stemmer/$(LANG)/$(PROFILE).h" ] || [ ! -f "$(CORE_DIR)/lemmatizer/$(LANG)/$(PROFILE).h" ]; then \
		echo "ERROR: Model headers not found for $(LANG)/$(PROFILE)." >&2; \
		exit 1; \
	fi

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

combined: check-sqlite | $(BUILD_DIR)
	@if [ -z "$(COMBINED_PAIRS)" ]; then \
		echo "ERROR: No valid (lang, profile) pairs resolved from LANGS='$(LANGS)' PROFILES='$(PROFILES)' MODELS='$(MODELS)'." >&2; \
		exit 1; \
	fi
	$(MAKE) -C $(CORE_DIR) build/lemmajet_runtime.o $(patsubst $(CORE_DIR)/%,%,$(COMBINED_MODEL_OBJS)) LANGS="$(COMBINED_LANGS)" PROFILES="$(COMBINED_PROFILES)"
	$(CC) $(CFLAGS) -I$(CORE_DIR) $(COMBINED_DICT_DEFINES) -c $(CORE_DIR)/lemmajet_dict.c -o $(BUILD_DIR)/lemmajet_dict_combined.o
	$(CC) $(CFLAGS) -I$(CORE_DIR) -I$(SQLITE_INC) $(SHARED_FLAGS) lemmajet_fts5.c $(RUNTIME_OBJ) $(BUILD_DIR)/lemmajet_dict_combined.o $(COMBINED_MODEL_OBJS) -o $(COMBINED_TARGET)

model: check-sqlite check-model-args | $(BUILD_DIR)
	$(MAKE) -C $(CORE_DIR) build/lemmajet_runtime.o build/models/$(LANG)_stem_$(PROFILE).o build/models/$(LANG)_lemma_$(PROFILE).o LANGS="$(LANG)" PROFILES="$(PROFILE)"
	$(CC) $(CFLAGS) -I$(CORE_DIR) $(MODEL_DICT_DEF) -c $(CORE_DIR)/lemmajet_dict.c -o $(MODEL_DICT_OBJ)
	$(CC) $(CFLAGS) -I$(CORE_DIR) -I$(SQLITE_INC) $(MODEL_DEFINES) $(SHARED_FLAGS) lemmajet_fts5.c $(RUNTIME_OBJ) $(MODEL_DICT_OBJ) $(MODEL_STEM_OBJ) $(MODEL_LEMMA_OBJ) -o $(MODEL_TARGET)

models: check-sqlite
	@for pair in $(COMBINED_PAIRS); do \
		l=$${pair%%:*}; \
		p=$${pair#*:}; \
		$(MAKE) model LANG="$$l" PROFILE="$$p" SQLITE_INC="$(SQLITE_INC)" CC="$(CC)" || exit 1; \
	done

test: check-sqlite
	$(MAKE) model LANG=pl PROFILE=sm_balanced SQLITE_INC="$(SQLITE_INC)" CC="$(CC)"
	$(MAKE) model LANG=en PROFILE=md_balanced SQLITE_INC="$(SQLITE_INC)" CC="$(CC)"
	@if [ -z "$(SQLITE_CLI)" ] || [ ! -x "$(SQLITE_CLI)" ]; then \
		echo "ERROR: sqlite3 CLI not found. Set SQLITE_CLI=/path/to/sqlite3 or SQLITE_SRC=/path/to/sqlite-src." >&2; \
		exit 1; \
	fi
	@out=$$("$(SQLITE_CLI)" :memory: \
		".load $(BUILD_DIR)/lemmajet_fts5_pl_sm_balanced" \
		".load $(BUILD_DIR)/lemmajet_fts5_en_md_balanced" \
		"CREATE VIRTUAL TABLE pl_docs USING fts5(body, tokenize='lemmajet_pl_sm_balanced lemma');" \
		"INSERT INTO pl_docs VALUES ('Dwaj mężczyźni szli drogą.'), ('Widziałem mężczyznę w ogrodzie.');" \
		"CREATE VIRTUAL TABLE en_docs USING fts5(body, tokenize='lemmajet_en_md_balanced lemma');" \
		"INSERT INTO en_docs VALUES ('The mice were running quickly.'), ('A small mouse hid.');" \
		"SELECT 'pl_lemma=' || count(*) FROM pl_docs WHERE pl_docs MATCH 'mężczyzna';" \
		"SELECT 'pl_prefix=' || count(*) FROM pl_docs WHERE pl_docs MATCH 'mężczyz*';" \
		"SELECT 'en_lemma=' || count(*) FROM en_docs WHERE en_docs MATCH 'mouse';"); \
	echo "$$out"; \
	echo "$$out" | grep -q "pl_lemma=2" || { echo "FAIL: expected pl_lemma=2" >&2; exit 1; }; \
	echo "$$out" | grep -q "pl_prefix=2" || { echo "FAIL: expected pl_prefix=2" >&2; exit 1; }; \
	echo "$$out" | grep -q "en_lemma=2" || { echo "FAIL: expected en_lemma=2" >&2; exit 1; }; \
	echo "PASS: multi-extension SQLite FTS5 test passed."

clean:
	rm -rf $(BUILD_DIR)

docker-build:
	docker build -f Dockerfile \
		--build-arg LANGS="$(LANGS)" \
		--build-arg PROFILES="$(PROFILES)" \
		--build-arg MODELS="$(MODELS)" \
		-t "$(DOCKER_IMAGE)" $(CORE_DIR)

docker-test: docker-build
	docker run --rm "$(DOCKER_IMAGE)" make -C /src/pkg/c/sqlite test

docker-bench: docker-build
	docker run --rm "$(DOCKER_IMAGE)" python3 /src/pkg/c/sqlite/bench_fts5.py $(BENCH_ARGS)

help:
	@echo "LemmaJet SQLite FTS5 Extension Build:"
	@echo "  make combined SQLITE_SRC=... [LANGS=...] [PROFILES=...] [MODELS=...]"
	@echo "  make model SQLITE_SRC=... LANG=pl PROFILE=sm_balanced"
	@echo "  make models SQLITE_SRC=... [LANGS=...] [PROFILES=...]"
	@echo "  make test SQLITE_SRC=... [SQLITE_CLI=...]"
	@echo "  make clean"
	@echo "  make docker-build [LANGS=...] [PROFILES=...] [MODELS=...]"
	@echo "  make docker-test"
	@echo "  make docker-bench [BENCH_ARGS=\"--langs en --docs 1000\"]"
