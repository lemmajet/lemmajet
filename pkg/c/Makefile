# LemmaJet ANSI C Makefile
# Zero-dependency ANSI C99 / C23 modular build system

CC ?= clang
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O3 -I.
SAN_FLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I.
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    SO_EXT := dylib
    SHARED_FLAGS := -dynamiclib
else
    SO_EXT := so
    SHARED_FLAGS := -shared -fPIC
endif

LANGS ?= pl en de es fr cs
PROFILES ?= md_balanced

BUILD_DIR := build
MODELS_DIR := $(BUILD_DIR)/models
STATIC_LIB := $(BUILD_DIR)/liblemmajet.a
SHARED_LIB := $(BUILD_DIR)/liblemmajet.$(SO_EXT)
TEST_BIN := $(BUILD_DIR)/test_parity
TEST_SAN_BIN := $(BUILD_DIR)/test_fts_abi
BENCH_BIN := $(BUILD_DIR)/bench_c
CLI_BIN := $(BUILD_DIR)/lemmajet

REGISTRY_H := lemmajet_registry.h
PARITY_VECTORS_H := lemmajet_parity_vectors.h

# Discover available (lang, profile) pairs in stemmer/ and lemmatizer/
ACTIVE_PAIRS := $(strip $(foreach l,$(LANGS),$(foreach p,$(PROFILES),\
    $(if $(and $(wildcard stemmer/$(l)/$(p).h),$(wildcard lemmatizer/$(l)/$(p).h)),$(l):$(p),))))

MODEL_NAMES := $(foreach pair,$(ACTIVE_PAIRS),\
    $(word 1,$(subst :, ,$(pair)))_stem_$(word 2,$(subst :, ,$(pair))) \
    $(word 1,$(subst :, ,$(pair)))_lemma_$(word 2,$(subst :, ,$(pair))))

MODEL_OBJS := $(addprefix $(MODELS_DIR)/,$(addsuffix .o,$(MODEL_NAMES)))

DICT_DEFINES := $(foreach pair,$(ACTIVE_PAIRS),\
    -DLEMMAJET_ENABLE_$(shell printf '%s_%s' '$(word 1,$(subst :, ,$(pair)))' '$(word 2,$(subst :, ,$(pair)))' | tr '[:lower:]-' '[:upper:]_')=1)

CORE_OBJS := $(BUILD_DIR)/lemmajet_runtime.o $(BUILD_DIR)/lemmajet_dict.o

.PHONY: all cli test test-san bench sqlite postgres clean help

all: $(STATIC_LIB) $(SHARED_LIB) $(CLI_BIN)

cli: $(CLI_BIN)

$(CLI_BIN): examples/lemmajet_cli.c $(STATIC_LIB) lemmajet_dict.h lemmajet.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/lemmajet_cli.c $(STATIC_LIB) -o $@

$(BUILD_DIR) $(MODELS_DIR):
	@mkdir -p $@

$(REGISTRY_H):
	@if [ ! -f $(REGISTRY_H) ]; then \
		echo "Generating C models via build_pkg.py..."; \
		(cd ../.. && uv run python3 scripts/build_pkg.py --target c --profile md_balanced) || \
		(cd ../.. && python3 scripts/build_pkg.py --target c --profile md_balanced); \
	fi

$(PARITY_VECTORS_H):
	@if [ ! -f $(PARITY_VECTORS_H) ]; then \
		echo "Generating C parity vectors via build_pkg.py..."; \
		(cd ../.. && uv run python3 scripts/build_pkg.py --target c --profile md_balanced) || \
		(cd ../.. && python3 scripts/build_pkg.py --target c --profile md_balanced); \
	fi

$(BUILD_DIR)/lemmajet_runtime.c: | $(BUILD_DIR)
	@printf '#define LEMMAJET_IMPLEMENTATION\n#define LEMMAJET_ENABLE_SNOWBALL_COMPAT 1\n#include "lemmajet.h"\n' > $@

$(BUILD_DIR)/lemmajet_runtime.o: $(BUILD_DIR)/lemmajet_runtime.c lemmajet.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_DIR)/lemmajet_dict.o: lemmajet_dict.c lemmajet_dict.h lemmajet.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC $(DICT_DEFINES) -c $< -o $@

# Generate thin per-model .c wrappers into build/models/
define MODEL_RULE
$$(MODELS_DIR)/$(1)_stem_$(2).c: stemmer/$(1)/$(2).h | $$(MODELS_DIR)
	@printf '#include "lemmajet.h"\n#include "stemmer/$(1)/$(2).h"\nconst lemmajet_model_t* lj_get_$(1)_stem_$(2)(void) { return &lemmajet_model_$(1)_general_$(2)_stem; }\n' > $$@

$$(MODELS_DIR)/$(1)_lemma_$(2).c: lemmatizer/$(1)/$(2).h | $$(MODELS_DIR)
	@printf '#include "lemmajet.h"\n#include "lemmatizer/$(1)/$(2).h"\nconst lemmajet_model_t* lj_get_$(1)_lemma_$(2)(void) { return &lemmajet_model_$(1)_general_$(2)_lemma; }\n' > $$@
endef

$(foreach pair,$(ACTIVE_PAIRS),$(eval $(call MODEL_RULE,$(word 1,$(subst :, ,$(pair))),$(word 2,$(subst :, ,$(pair))))))

$(MODELS_DIR)/%.o: $(MODELS_DIR)/%.c lemmajet.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(STATIC_LIB): $(CORE_OBJS) $(MODEL_OBJS)
	rm -f $@
	ar rcs $@ $^

$(SHARED_LIB): $(CORE_OBJS) $(MODEL_OBJS)
	$(CC) $(SHARED_FLAGS) $^ -o $@

test: $(TEST_BIN)
	@echo "Running native C parity test suite..."
	@$(TEST_BIN)

$(TEST_BIN): tests/test_parity.c lemmajet.h $(REGISTRY_H) $(PARITY_VECTORS_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_parity.c -o $@

test-san: $(TEST_SAN_BIN)
	@echo "Running Layer 2 ASan/UBSan C FTS ABI test suite..."
	@$(TEST_SAN_BIN)

$(TEST_SAN_BIN): tests/test_fts_abi.c $(BUILD_DIR)/lemmajet_runtime.c lemmajet_dict.c lemmajet_dict.h lemmajet.h $(MODEL_OBJS) | $(BUILD_DIR)
	$(CC) $(SAN_FLAGS) $(DICT_DEFINES) tests/test_fts_abi.c $(BUILD_DIR)/lemmajet_runtime.c lemmajet_dict.c $(MODEL_OBJS) -o $@

bench: $(BENCH_BIN)
	@echo "Running native C microbenchmark suite..."
	@$(BENCH_BIN)

$(BENCH_BIN): benchmarks/bench_c.c lemmajet.h $(REGISTRY_H) | $(BUILD_DIR)
	$(CC) $(CFLAGS) benchmarks/bench_c.c -o $@

sqlite: $(STATIC_LIB)
	$(MAKE) -C sqlite LANGS="$(LANGS)" PROFILES="$(PROFILES)"

postgres:
	$(MAKE) -C postgres LANGS="$(LANGS)" PROFILES="$(PROFILES)"

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "LemmaJet C Build System:"
	@echo "  make all      - Build static/shared libraries and CLI demo ($(CLI_BIN))"
	@echo "  make cli      - Build standalone stdin/stdout CLI binary ($(CLI_BIN))"
	@echo "  make test     - Build and execute native parity test suite ($(TEST_BIN))"
	@echo "  make test-san - Build and execute ASan/UBSan C FTS ABI test suite ($(TEST_SAN_BIN))"
	@echo "  make bench    - Build and execute microbenchmark harness ($(BENCH_BIN))"
	@echo "  make sqlite   - Build SQLite FTS5 extension (pass SQLITE_SRC=...)"
	@echo "  make postgres - Build PostgreSQL PGXS extensions (delegates to pkg/c/postgres/Makefile)"
	@echo "  make clean    - Remove build artifacts"
	@echo "Variables: LANGS=\"$(LANGS)\" PROFILES=\"$(PROFILES)\""
