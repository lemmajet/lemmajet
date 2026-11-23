ARG PG_MAJOR=17
ARG LANGS="pl en de es fr cs"
ARG PROFILES="md_balanced"
ARG WITH_STEM=0

FROM postgres:${PG_MAJOR} AS builder
ARG PG_MAJOR
ARG LANGS
ARG PROFILES
ARG WITH_STEM

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        postgresql-server-dev-${PG_MAJOR} \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src/pkg/c
COPY lemmajet.h lemmajet_dict.h lemmajet_dict.c Makefile ./
COPY stemmer ./stemmer
COPY lemmatizer ./lemmatizer
COPY postgres ./postgres

RUN rm -rf /src/pkg/c/postgres/build /src/pkg/c/build \
    && make -C /src/pkg/c/postgres install LANGS="${LANGS}" PROFILES="${PROFILES}" WITH_STEM="${WITH_STEM}"

FROM postgres:${PG_MAJOR} AS runtime
ARG PG_MAJOR

COPY --from=builder /usr/lib/postgresql/${PG_MAJOR}/lib/lemmajet_*.so /usr/lib/postgresql/${PG_MAJOR}/lib/
COPY --from=builder /usr/share/postgresql/${PG_MAJOR}/extension/lemmajet_* /usr/share/postgresql/${PG_MAJOR}/extension/

FROM runtime AS bench

RUN apt-get update && apt-get install -y --no-install-recommends \
        hunspell-pl \
    && rm -rf /var/lib/apt/lists/* \
    && SHAREDIR="$(pg_config --sharedir)/tsearch_data" \
    && iconv -f ISO-8859-2 -t UTF-8 /usr/share/hunspell/pl_PL.aff \
        | sed '1s/ISO8859-2/UTF-8/' > "${SHAREDIR}/pl_pl.affix" \
    && iconv -f ISO-8859-2 -t UTF-8 /usr/share/hunspell/pl_PL.dic \
        > "${SHAREDIR}/pl_pl.dict"
