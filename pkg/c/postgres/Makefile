# LemmaJet PostgreSQL TEXT SEARCH Extension Makefile (PGXS + Docker)
#
# Dual-mode recursive Makefile:
#   - Orchestrator mode (EXT_LANG unset): loops over LANGS invoking PGXS per language
#   - PGXS mode (EXT_LANG=<lang>): builds and installs lemmajet_<lang>.{so,control,sql}

PG_CONFIG ?= pg_config
PG_MAJOR ?= 17
LANGS ?= pl en de es fr cs
PROFILES ?= md_balanced
WITH_STEM ?= 0
DOCKER_IMAGE ?= lemmajet-pg:$(PG_MAJOR)
DOCKER_BENCH_IMAGE ?= lemmajet-pg-bench:$(PG_MAJOR)
USE_DOCKER ?= 0

CORE_DIR := ..
BUILD_DIR := build
UNAME_S := $(shell uname -s)
HAS_PG_CONFIG := $(shell command -v $(PG_CONFIG) 2>/dev/null)

# Helper: profiles registered in pkg/c/lemmajet_dict.c for a given language
define lang_supported_profiles
$(if $(filter en,$(1)),xs_balanced sm_balanced md_balanced,sm_balanced md_balanced)
endef

define lang_active_profiles
$(strip $(foreach p,$(PROFILES),\
    $(if $(and $(filter $(p),$(call lang_supported_profiles,$(1))),\
               $(wildcard $(CORE_DIR)/stemmer/$(1)/$(p).h),\
               $(wildcard $(CORE_DIR)/lemmatizer/$(1)/$(p).h)),$(p),)))
endef

ifdef EXT_LANG
# =========================================================================
# Recursive PGXS Mode (single language extension: lemmajet_$(EXT_LANG))
# =========================================================================
ACTIVE_PROFILES := $(call lang_active_profiles,$(EXT_LANG))
ifeq ($(ACTIVE_PROFILES),)
$(error No valid profiles found for EXT_LANG=$(EXT_LANG) in PROFILES='$(PROFILES)')
endif

ifeq ($(WITH_STEM),1)
MODEL_OBJS := $(foreach p,$(ACTIVE_PROFILES),\
    $(CORE_DIR)/build/models/$(EXT_LANG)_stem_$(p).o \
    $(CORE_DIR)/build/models/$(EXT_LANG)_lemma_$(p).o)
DICT_DEFINES := $(foreach p,$(ACTIVE_PROFILES),\
    -DLEMMAJET_ENABLE_$(shell printf '%s_%s' '$(EXT_LANG)' '$(p)' | tr '[:lower:]-' '[:upper:]_')=1)
else
MODEL_OBJS := $(foreach p,$(ACTIVE_PROFILES),\
    $(CORE_DIR)/build/models/$(EXT_LANG)_lemma_$(p).o)
DICT_DEFINES := -DLEMMAJET_DISABLE_STEM $(foreach p,$(ACTIVE_PROFILES),\
    -DLEMMAJET_ENABLE_$(shell printf '%s_%s' '$(EXT_LANG)' '$(p)' | tr '[:lower:]-' '[:upper:]_')=1)
endif

EXTENSION := lemmajet_$(EXT_LANG)
MODULE_big := lemmajet_$(EXT_LANG)
DATA_built := lemmajet_$(EXT_LANG)--1.0.sql
OBJS := $(BUILD_DIR)/lemmajet_pg_$(EXT_LANG).o \
        $(BUILD_DIR)/lemmajet_dict_$(EXT_LANG).o \
        $(CORE_DIR)/build/lemmajet_runtime.o \
        $(MODEL_OBJS)

EXTRA_CLEAN := lemmajet_$(EXT_LANG).control \
               lemmajet_$(EXT_LANG)--1.0.sql \
               $(BUILD_DIR)/lemmajet_pg_$(EXT_LANG).o \
               $(BUILD_DIR)/lemmajet_dict_$(EXT_LANG).o

ifeq ($(UNAME_S),Linux)
SHLIB_LINK += -Wl,-Bsymbolic
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: lemmajet_$(EXT_LANG).control

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/lemmajet_pg_$(EXT_LANG).o: lemmajet_pg.c $(CORE_DIR)/lemmajet_dict.h $(CORE_DIR)/lemmajet.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -I$(CORE_DIR) \
		-DLJ_PG_LANG='"$(EXT_LANG)"' \
		-DLJ_PG_DINIT=lemmajet_$(EXT_LANG)_dinit \
		-DLJ_PG_DLEXIZE=lemmajet_$(EXT_LANG)_dlexize \
		-c $< -o $@

$(BUILD_DIR)/lemmajet_dict_$(EXT_LANG).o: $(CORE_DIR)/lemmajet_dict.c $(CORE_DIR)/lemmajet_dict.h $(CORE_DIR)/lemmajet.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -I$(CORE_DIR) $(DICT_DEFINES) -c $< -o $@

lemmajet_$(EXT_LANG).control: lemmajet.control.in
	sed 's/@LANG@/$(EXT_LANG)/g' $< > $@

lemmajet_$(EXT_LANG)--1.0.sql: lemmajet--1.0.sql.in
	sed 's/@LANG@/$(EXT_LANG)/g' $< > $@

else
# =========================================================================
# Orchestrator Mode (multi-language loop + Docker targets)
# =========================================================================
ifeq ($(WITH_STEM),1)
CORE_MODEL_OBJS := $(foreach l,$(LANGS),$(foreach p,$(call lang_active_profiles,$(l)),\
    build/models/$(l)_stem_$(p).o build/models/$(l)_lemma_$(p).o))
else
CORE_MODEL_OBJS := $(foreach l,$(LANGS),$(foreach p,$(call lang_active_profiles,$(l)),\
    build/models/$(l)_lemma_$(p).o))
endif

.PHONY: all install uninstall clean core-objs docker-build docker-build-bench pg-test help

all: core-objs
	@for l in $(LANGS); do \
		$(MAKE) EXT_LANG="$$l" with_llvm=no PG_CONFIG="$(PG_CONFIG)" PROFILES="$(PROFILES)" WITH_STEM="$(WITH_STEM)" || exit 1; \
	done

core-objs:
	$(MAKE) -C $(CORE_DIR) LANGS="$(LANGS)" PROFILES="$(PROFILES)" build/lemmajet_runtime.o $(CORE_MODEL_OBJS)

install: all
	@for l in $(LANGS); do \
		$(MAKE) EXT_LANG="$$l" with_llvm=no PG_CONFIG="$(PG_CONFIG)" PROFILES="$(PROFILES)" WITH_STEM="$(WITH_STEM)" install || exit 1; \
	done

uninstall:
	@for l in $(LANGS); do \
		$(MAKE) EXT_LANG="$$l" with_llvm=no PG_CONFIG="$(PG_CONFIG)" PROFILES="$(PROFILES)" WITH_STEM="$(WITH_STEM)" uninstall || exit 1; \
	done

docker-build:
	docker build -f Dockerfile --target runtime \
		--build-arg PG_MAJOR="$(PG_MAJOR)" \
		--build-arg LANGS="$(LANGS)" \
		--build-arg PROFILES="$(PROFILES)" \
		--build-arg WITH_STEM="$(WITH_STEM)" \
		-t "$(DOCKER_IMAGE)" $(CORE_DIR)

docker-build-bench:
	docker build -f Dockerfile --target bench \
		--build-arg PG_MAJOR="$(PG_MAJOR)" \
		--build-arg LANGS="$(LANGS)" \
		--build-arg PROFILES="$(PROFILES)" \
		--build-arg WITH_STEM="$(WITH_STEM)" \
		-t "$(DOCKER_BENCH_IMAGE)" $(CORE_DIR)

define PG_SMOKE_SQL
CREATE EXTENSION IF NOT EXISTS unaccent;
CREATE EXTENSION IF NOT EXISTS lemmajet_pl;
CREATE EXTENSION IF NOT EXISTS lemmajet_en;
CREATE EXTENSION IF NOT EXISTS lemmajet_de;
CREATE EXTENSION IF NOT EXISTS lemmajet_es;
CREATE EXTENSION IF NOT EXISTS lemmajet_fr;
CREATE EXTENSION IF NOT EXISTS lemmajet_cs;

DO $$$$
BEGIN
    IF (to_tsvector('lemmajet_pl', 'Młodzi mężczyźni szli drogami') @@ to_tsquery('lemmajet_pl', 'mężczyzna')) IS NOT TRUE THEN
        RAISE EXCEPTION 'pl smoke failed';
    END IF;
    IF (to_tsvector('lemmajet_en', 'Smart engines found lemmas') @@ to_tsquery('lemmajet_en', 'find')) IS NOT TRUE THEN
        RAISE EXCEPTION 'en smoke failed';
    END IF;
    IF (to_tsvector('lemmajet_de', 'Die Häuser stehen in großen Gärten') @@ to_tsquery('lemmajet_de', 'haus')) IS NOT TRUE THEN
        RAISE EXCEPTION 'de smoke failed';
    END IF;
    IF (to_tsvector('lemmajet_es', 'Los niños corrían rápidamente') @@ to_tsquery('lemmajet_es', 'niño')) IS NOT TRUE THEN
        RAISE EXCEPTION 'es smoke failed';
    END IF;
    IF (to_tsvector('lemmajet_fr', 'Les chevaux galopaient dans les champs') @@ to_tsquery('lemmajet_fr', 'cheval')) IS NOT TRUE THEN
        RAISE EXCEPTION 'fr smoke failed';
    END IF;
    IF (to_tsvector('lemmajet_cs', 'Mladí muži chodili po cestách') @@ to_tsquery('lemmajet_cs', 'muž')) IS NOT TRUE THEN
        RAISE EXCEPTION 'cs smoke failed';
    END IF;
END $$$$;

CREATE TEXT SEARCH DICTIONARY pl_lemma_md (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = 'md_balanced'
);

DO $$$$
BEGIN
    IF ts_lexize('pl_lemma_md', 'mężczyznami') IS NULL THEN
        RAISE EXCEPTION 'pl_lemma_md ts_lexize failed';
    END IF;
END $$$$;

CREATE TEXT SEARCH DICTIONARY pl_flt (
    TEMPLATE = lemmajet_pl_template,
    Mode = 'lemma',
    Profile = 'md_balanced',
    Filter = true
);
CREATE TEXT SEARCH CONFIGURATION pl_unacc (COPY = pg_catalog.simple);
ALTER TEXT SEARCH CONFIGURATION pl_unacc
    ALTER MAPPING FOR asciiword, asciihword, hword_asciipart, word, hword, hword_part
    WITH pl_flt, unaccent, simple;

DO $$$$
BEGIN
    IF (to_tsvector('pl_unacc', 'Rozmawialiśmy z młodymi mężczyznami') @@ to_tsquery('pl_unacc', 'mezczyzna')) IS NOT TRUE THEN
        RAISE EXCEPTION 'Filter=true + unaccent chaining failed';
    END IF;
END $$$$;

CREATE TEMP TABLE lj_gin_test (
    body text,
    tsv tsvector GENERATED ALWAYS AS (to_tsvector('lemmajet_pl', body)) STORED
);
CREATE INDEX lj_gin_test_idx ON lj_gin_test USING gin (tsv);
INSERT INTO lj_gin_test (body) VALUES ('Młodzi mężczyźni szli drogą.');

DO $$$$
DECLARE c int;
BEGIN
    SELECT count(*) INTO c FROM lj_gin_test WHERE tsv @@ to_tsquery('lemmajet_pl', 'mężczyzna');
    IF c IS DISTINCT FROM 1 THEN
        RAISE EXCEPTION 'GIN query expected 1, got %', c;
    END IF;
END $$$$;

DO $$$$
BEGIN
    BEGIN
        CREATE TEXT SEARCH DICTIONARY lj_bad_param (TEMPLATE = lemmajet_pl_template, StopWords = 'polish');
        RAISE EXCEPTION 'expected error for unknown parameter';
    EXCEPTION WHEN invalid_parameter_value THEN
        NULL;
    END;
    BEGIN
        CREATE TEXT SEARCH DICTIONARY lj_bad_prof (TEMPLATE = lemmajet_pl_template, Profile = 'nonexistent');
        RAISE EXCEPTION 'expected error for unknown profile';
    EXCEPTION WHEN invalid_parameter_value THEN
        NULL;
    END;
END $$$$;
endef
export PG_SMOKE_SQL

pg-test:
ifeq ($(or $(filter 1,$(USE_DOCKER)),$(if $(HAS_PG_CONFIG),,1)),)
	@printf '%s\n' "$$PG_SMOKE_SQL" | psql -v ON_ERROR_STOP=1
else
	@$(MAKE) docker-build
	@cid=$$(docker run -d -e POSTGRES_PASSWORD=postgres "$(DOCKER_IMAGE)"); \
	trap "docker rm -f $$cid >/dev/null 2>&1 || true" EXIT; \
	until docker exec $$cid pg_isready -U postgres >/dev/null 2>&1; do sleep 1; done; \
	printf '%s\n' "$$PG_SMOKE_SQL" | docker exec -i $$cid psql -U postgres -v ON_ERROR_STOP=1
endif
	@echo "PASS: PostgreSQL 6-language PGXS smoke suite passed."

clean:
	rm -rf $(BUILD_DIR) lemmajet_*.so lemmajet_*.dylib lemmajet_*.control lemmajet_*--*.sql *.bc

help:
	@echo "LemmaJet PostgreSQL PGXS Build System:"
	@echo "  make all                - Build all per-language PGXS extensions (requires pg_config)"
	@echo "  make install            - Install extensions into PostgreSQL libdir/extension"
	@echo "  make uninstall          - Remove extensions from PostgreSQL libdir/extension"
	@echo "  make docker-build       - Build lean runtime Docker image ($(DOCKER_IMAGE))"
	@echo "  make docker-build-bench - Build benchmark Docker image with hunspell-pl ($(DOCKER_BENCH_IMAGE))"
	@echo "  make pg-test            - Run 6-language SQL + GIN + Filter smoke suite (Docker or local psql)"
	@echo "  make clean              - Remove build artifacts"
	@echo "Variables: LANGS=\"$(LANGS)\" PROFILES=\"$(PROFILES)\" PG_MAJOR=\"$(PG_MAJOR)\" WITH_STEM=\"$(WITH_STEM)\""

endif
