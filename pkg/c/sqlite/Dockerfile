ARG SQLITE_VERSION=3540100
ARG SQLITE_YEAR=2026
ARG LANGS="pl en de es fr cs"
ARG PROFILES="sm_balanced md_balanced"
ARG MODELS=""

FROM debian:bookworm-slim AS sqlite-builder
ARG SQLITE_VERSION
ARG SQLITE_YEAR

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gcc \
        libc6-dev \
        make \
        tcl \
        unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/sqlite
RUN curl -fsSL "https://www.sqlite.org/${SQLITE_YEAR}/sqlite-src-${SQLITE_VERSION}.zip" -o sqlite-src.zip \
    && unzip -q sqlite-src.zip \
    && cd "sqlite-src-${SQLITE_VERSION}" \
    && CC=gcc ./configure --enable-json --enable-load-extension --enable-fts5 \
    && make -j"$(nproc)" sqlite3 \
    && cp sqlite3 /usr/local/bin/sqlite3 \
    && cp sqlite3.h sqlite3ext.h fts5.h /usr/local/include/

FROM debian:bookworm-slim AS runtime
ARG LANGS
ARG PROFILES
ARG MODELS

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        make \
        python3 \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc

COPY --from=sqlite-builder /usr/local/bin/sqlite3 /usr/local/bin/sqlite3
COPY --from=sqlite-builder /usr/local/include/sqlite3.h /usr/local/include/sqlite3.h
COPY --from=sqlite-builder /usr/local/include/sqlite3ext.h /usr/local/include/sqlite3ext.h
COPY --from=sqlite-builder /usr/local/include/fts5.h /usr/local/include/fts5.h

WORKDIR /src/pkg/c
COPY lemmajet.h lemmajet_dict.h lemmajet_dict.c Makefile ./
COPY stemmer ./stemmer
COPY lemmatizer ./lemmatizer
COPY sqlite ./sqlite

RUN rm -rf /src/pkg/c/build /src/pkg/c/sqlite/build \
    && make -C /src/pkg/c/sqlite combined LANGS="${LANGS}" PROFILES="${PROFILES}" MODELS="${MODELS}"

WORKDIR /src/pkg/c/sqlite
