/*
 * LemmaJet: Generated C Cross-Language Parity Vectors
 * Expected stem and lemma outputs evaluated from Python CompiledStemmerModel.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEMMAJET_PARITY_VECTORS_H
#define LEMMAJET_PARITY_VECTORS_H

typedef struct {
    const char* word;
    const char* expected_stem;
    const char* expected_lemma;
} parity_vector_t;

#define LEMMAJET_PARITY_HAS_CS 1
static const parity_vector_t VECTORS_CS[] = {
    {"městy", "měst", "město"},
    {"domy", "dom", "dům"},
    {"lidé", "lid", "člověk"},
    {"kočky", "koč", "kočka"},
    {"krásné", "krásn", "krásný"},
    {NULL, NULL, NULL}
};

#define LEMMAJET_PARITY_HAS_DE 1
static const parity_vector_t VECTORS_DE[] = {
    {"häuser", "haus", "haus"},
    {"bäume", "bäume", "baum"},
    {"katzen", "katze", "katze"},
    {"ging", "ging", "gehen"},
    {"große", "gro", "groß"},
    {NULL, NULL, NULL}
};

#define LEMMAJET_PARITY_HAS_EN 1
static const parity_vector_t VECTORS_EN[] = {
    {"running", "run", "run"},
    {"cats", "cat", "cat"},
    {"better", "better", "good"},
    {"went", "go", "go"},
    {"flies", "fly", "fly"},
    {NULL, NULL, NULL}
};

#define LEMMAJET_PARITY_HAS_ES 1
static const parity_vector_t VECTORS_ES[] = {
    {"casas", "cas", "casa"},
    {"perros", "perro", "perro"},
    {"comiendo", "come", "comer"},
    {"buenos", "buen", "bueno"},
    {"hablar", "habl", "hablar"},
    {NULL, NULL, NULL}
};

#define LEMMAJET_PARITY_HAS_FR 1
static const parity_vector_t VECTORS_FR[] = {
    {"chats", "chat", "chat"},
    {"maisons", "maison", "maison"},
    {"mangeant", "mang", "manger"},
    {"grands", "grand", "grand"},
    {"petite", "petit", "petit"},
    {NULL, NULL, NULL}
};

#define LEMMAJET_PARITY_HAS_PL 1
static const parity_vector_t VECTORS_PL[] = {
    {"psami", "pies", "pies"},
    {"domami", "dom", "dom"},
    {"ludzie", "ludzi", "człowiek"},
    {"chleba", "chleb", "chleb"},
    {"bogów", "bog", "bóg"},
    {NULL, NULL, NULL}
};

#endif /* LEMMAJET_PARITY_VECTORS_H */
