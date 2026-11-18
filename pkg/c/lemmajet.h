/*
 * LemmaJet: Ultra-fast, Zero-Dependency ANSI C99/C23 Stemmer & Lemmatizer Runtime
 * Single-header library (STB-style).
 *
 * SPDX-License-Identifier: MIT
 *
 * Include this header in files that use LemmaJet.
 * In EXACTLY ONE C or C++ source file, define LEMMAJET_IMPLEMENTATION before including:
 *
 *     #define LEMMAJET_IMPLEMENTATION
 *     #include "lemmajet.h"
 */

#ifndef LEMMAJET_H
#define LEMMAJET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define LJ_UNUSED __attribute__((unused))
#else
#define LJ_UNUSED
#endif

#if defined(__cplusplus)
#if defined(__GNUC__) || defined(__clang__)
#define LJ_RESTRICT __restrict__
#else
#define LJ_RESTRICT
#endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define LJ_RESTRICT restrict
#else
#define LJ_RESTRICT
#endif

/* Data structures representing compiled models */

typedef struct {
    uint32_t hash;
    uint16_t word_off;
    uint16_t stem_off;
    uint8_t word_len;
    uint8_t stem_len;
} lj_exc_hash_slot_t;

typedef struct {
    const lj_exc_hash_slot_t* slots;
    const uint8_t* blob;
    uint32_t mask;
    uint64_t len_mask;
} lj_exc_hash_t;

typedef struct {
    const char* pat;
    const char* rep;
    uint8_t pat_cp_len;
    uint8_t pat_len;
    uint8_t rep_len;
} lemmajet_tail_entry_t;

typedef struct {
    int32_t base;
    int32_t check;
    uint8_t need;
    uint8_t pad;
    int16_t op;
} lj_dat_node_t;

typedef struct lemmajet_model {
    uint8_t shift;
    uint32_t num_states;
    uint8_t min_stem_len;
    uint8_t affix_min_stem_len;
    uint8_t is_lemmatizer;

    /* UTF-8 byte to character ID mapping table (256 entries) */
    const uint8_t* byte_cid;

    /* Suffix trie transitions: (state << shift) | byte_cid (int16_t for states < 32768, else int32_t) */
    const int16_t* next16;
    const int32_t* next;
    const uint8_t* need;
    const int32_t* op;

    /* Optional Double-Array Trie (DAT) compact suffix trie (mutually exclusive with next16/next) */
    const lj_dat_node_t* dat_nodes;
    uint32_t num_dat_nodes;

    /* CutAdd operations */
    const uint8_t* op_cut;
    const char* const* op_add;
    const uint8_t* op_add_len;
    const uint8_t* op_add_cp_len;
    const uint8_t* op_br;

    /* Stage 1, 2.5, 3.5 Exceptions (compile-time FNV-1a open-addressed hash table) */
    lj_exc_hash_t exc_hash;

    /* Stage 2 Forward byte-level prefix trie */
    uint32_t num_pfx_states;
    const int16_t* pfx_next16;
    const int32_t* pfx_next;
    const uint8_t* pfx_need;
    const int8_t* pfx_op;

    /* Stage 2 Legacy fallback prefix string */
    const char* prefix;
    uint8_t prefix_len;

    /* Stage 2 Enclitics */
    size_t num_enclitics;
    const char* const* enclitics;
    const uint8_t* enclitic_lens;
    const uint8_t* enclitic_cp_lens;

    /* Stage 4 Tail normalizer substitutions */
    size_t num_tail_substitutions;
    const lemmajet_tail_entry_t* tail_substitutions;
} lemmajet_model_t;

/* Public Native API Functions */

/**
 * Stem a word using caller-allocated output buffer.
 *
 * @param m    Pointer to compiled model.
 * @param word Input word buffer (need not be null-terminated when len != (size_t)-1).
 * @param len  Length of word in bytes, or (size_t)-1 for auto strlen.
 * @param out  Output buffer provided by caller (may be NULL if cap == 0).
 * @param cap  Capacity of output buffer in bytes (including space for '\0').
 * @return     Required output length in bytes (excluding '\0').
 *             If return >= (int)cap, output was not written (or incomplete) and caller should retry with cap >= return + 1.
 *             Returns -1 on invalid arguments (e.g. NULL model/word, or cap > 0 with NULL out).
 */
int lemmajet_stem(const lemmajet_model_t* m, const char* word, size_t len, char* out, size_t cap);

/**
 * Lemmatize a word using caller-allocated output buffer.
 * Same snprintf-style length return and (NULL, 0) size-query contract as lemmajet_stem().
 */
int lemmajet_lemmatize(const lemmajet_model_t* m, const char* word, size_t len, char* out, size_t cap);

/**
 * Convenience C-string wrapper for null-terminated inputs in stemmer mode.
 */
static inline int lemmajet_stem_cstr(const lemmajet_model_t* m, const char* word, char* out, size_t cap) {
    return lemmajet_stem(m, word, (size_t)-1, out, cap);
}

/**
 * Convenience C-string wrapper for null-terminated inputs in lemmatizer mode.
 */
static inline int lemmajet_lemmatize_cstr(const lemmajet_model_t* m, const char* word, char* out, size_t cap) {
    return lemmajet_lemmatize(m, word, (size_t)-1, out, cap);
}

typedef const lemmajet_model_t* (*lemmajet_model_lookup_fn)(const char* algorithm);
void lemmajet_set_model_lookup(lemmajet_model_lookup_fn fn);

/* =========================================================================
 * Snowball libstemmer drop-in compatibility API (D-01)
 * ========================================================================= */

#if defined(LEMMAJET_ENABLE_SNOWBALL_COMPAT) || defined(LEMMAJET_SNOWBALL_COMPAT)
typedef unsigned char sb_symbol;

struct sb_stemmer;

struct sb_stemmer* lj_sb_stemmer_new(const char* algorithm, const char* charenc);
struct sb_stemmer* lj_sb_stemmer_new_with_model(const lemmajet_model_t* model);
void lj_sb_stemmer_delete(struct sb_stemmer* stemmer);
const sb_symbol* lj_sb_stemmer_stem(struct sb_stemmer* stemmer, const sb_symbol* word, int size);
int lj_sb_stemmer_length(struct sb_stemmer* stemmer);
const char** lj_sb_stemmer_list(void);

#define sb_stemmer_new lj_sb_stemmer_new
#define sb_stemmer_delete lj_sb_stemmer_delete
#define sb_stemmer_stem lj_sb_stemmer_stem
#define sb_stemmer_length lj_sb_stemmer_length
#define sb_stemmer_list lj_sb_stemmer_list
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_H */

#ifdef LEMMAJET_IMPLEMENTATION
#ifndef LEMMAJET_IMPLEMENTATION_INCLUDED
#define LEMMAJET_IMPLEMENTATION_INCLUDED

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Multilingual Unicode Case Folding Table (0..1423, U+0000..U+058F)
 * Covers ASCII, Latin-1, Latin Extended-A/B, Greek, Cyrillic, and Armenian */
static const uint16_t LJ_LOWER_MAP[1424] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 215, 248, 249, 250, 251, 252, 253, 254, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    257, 257, 259, 259, 261, 261, 263, 263, 265, 265, 267, 267, 269, 269, 271, 271,
    273, 273, 275, 275, 277, 277, 279, 279, 281, 281, 283, 283, 285, 285, 287, 287,
    289, 289, 291, 291, 293, 293, 295, 295, 297, 297, 299, 299, 301, 301, 303, 303,
    304, 305, 307, 307, 309, 309, 311, 311, 312, 314, 314, 316, 316, 318, 318, 320,
    320, 322, 322, 324, 324, 326, 326, 328, 328, 329, 331, 331, 333, 333, 335, 335,
    337, 337, 339, 339, 341, 341, 343, 343, 345, 345, 347, 347, 349, 349, 351, 351,
    353, 353, 355, 355, 357, 357, 359, 359, 361, 361, 363, 363, 365, 365, 367, 367,
    369, 369, 371, 371, 373, 373, 375, 375, 255, 378, 378, 380, 380, 382, 382, 383,
    384, 595, 387, 387, 389, 389, 596, 392, 392, 598, 599, 396, 396, 397, 477, 601,
    603, 402, 402, 608, 611, 405, 617, 616, 409, 409, 410, 411, 623, 626, 414, 629,
    417, 417, 419, 419, 421, 421, 640, 424, 424, 643, 426, 427, 429, 429, 648, 432,
    432, 650, 651, 436, 436, 438, 438, 658, 441, 441, 442, 443, 445, 445, 446, 447,
    448, 449, 450, 451, 454, 454, 454, 457, 457, 457, 460, 460, 460, 462, 462, 464,
    464, 466, 466, 468, 468, 470, 470, 472, 472, 474, 474, 476, 476, 477, 479, 479,
    481, 481, 483, 483, 485, 485, 487, 487, 489, 489, 491, 491, 493, 493, 495, 495,
    496, 499, 499, 499, 501, 501, 405, 447, 505, 505, 507, 507, 509, 509, 511, 511,
    513, 513, 515, 515, 517, 517, 519, 519, 521, 521, 523, 523, 525, 525, 527, 527,
    529, 529, 531, 531, 533, 533, 535, 535, 537, 537, 539, 539, 541, 541, 543, 543,
    414, 545, 547, 547, 549, 549, 551, 551, 553, 553, 555, 555, 557, 557, 559, 559,
    561, 561, 563, 563, 564, 565, 566, 567, 568, 569, 11365, 572, 572, 410, 11366, 575,
    576, 578, 578, 384, 649, 652, 583, 583, 585, 585, 587, 587, 589, 589, 591, 591,
    592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 605, 606, 607,
    608, 609, 610, 611, 612, 613, 614, 615, 616, 617, 618, 619, 620, 621, 622, 623,
    624, 625, 626, 627, 628, 629, 630, 631, 632, 633, 634, 635, 636, 637, 638, 639,
    640, 641, 642, 643, 644, 645, 646, 647, 648, 649, 650, 651, 652, 653, 654, 655,
    656, 657, 658, 659, 660, 661, 662, 663, 664, 665, 666, 667, 668, 669, 670, 671,
    672, 673, 674, 675, 676, 677, 678, 679, 680, 681, 682, 683, 684, 685, 686, 687,
    688, 689, 690, 691, 692, 693, 694, 695, 696, 697, 698, 699, 700, 701, 702, 703,
    704, 705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 715, 716, 717, 718, 719,
    720, 721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732, 733, 734, 735,
    736, 737, 738, 739, 740, 741, 742, 743, 744, 745, 746, 747, 748, 749, 750, 751,
    752, 753, 754, 755, 756, 757, 758, 759, 760, 761, 762, 763, 764, 765, 766, 767,
    768, 769, 770, 771, 772, 773, 774, 775, 776, 777, 778, 779, 780, 781, 782, 783,
    784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799,
    800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812, 813, 814, 815,
    816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827, 828, 829, 830, 831,
    832, 833, 834, 835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846, 847,
    848, 849, 850, 851, 852, 853, 854, 855, 856, 857, 858, 859, 860, 861, 862, 863,
    864, 865, 866, 867, 868, 869, 870, 871, 872, 873, 874, 875, 876, 877, 878, 879,
    881, 881, 883, 883, 884, 885, 887, 887, 888, 889, 890, 891, 892, 893, 894, 1011,
    896, 897, 898, 899, 900, 901, 940, 903, 941, 942, 943, 907, 972, 909, 973, 974,
    912, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959,
    960, 961, 930, 963, 964, 965, 966, 967, 968, 969, 970, 971, 940, 941, 942, 943,
    944, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959,
    960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973, 974, 983,
    976, 977, 978, 979, 980, 981, 982, 983, 985, 985, 987, 987, 989, 989, 991, 991,
    993, 993, 995, 995, 997, 997, 999, 999, 1001, 1001, 1003, 1003, 1005, 1005, 1007, 1007,
    1008, 1009, 1010, 1011, 952, 1013, 1014, 1016, 1016, 1010, 1019, 1019, 1020, 891, 892, 893,
    1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 1117, 1118, 1119,
    1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087,
    1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095, 1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103,
    1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087,
    1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095, 1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103,
    1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 1117, 1118, 1119,
    1121, 1121, 1123, 1123, 1125, 1125, 1127, 1127, 1129, 1129, 1131, 1131, 1133, 1133, 1135, 1135,
    1137, 1137, 1139, 1139, 1141, 1141, 1143, 1143, 1145, 1145, 1147, 1147, 1149, 1149, 1151, 1151,
    1153, 1153, 1154, 1155, 1156, 1157, 1158, 1159, 1160, 1161, 1163, 1163, 1165, 1165, 1167, 1167,
    1169, 1169, 1171, 1171, 1173, 1173, 1175, 1175, 1177, 1177, 1179, 1179, 1181, 1181, 1183, 1183,
    1185, 1185, 1187, 1187, 1189, 1189, 1191, 1191, 1193, 1193, 1195, 1195, 1197, 1197, 1199, 1199,
    1201, 1201, 1203, 1203, 1205, 1205, 1207, 1207, 1209, 1209, 1211, 1211, 1213, 1213, 1215, 1215,
    1231, 1218, 1218, 1220, 1220, 1222, 1222, 1224, 1224, 1226, 1226, 1228, 1228, 1230, 1230, 1231,
    1233, 1233, 1235, 1235, 1237, 1237, 1239, 1239, 1241, 1241, 1243, 1243, 1245, 1245, 1247, 1247,
    1249, 1249, 1251, 1251, 1253, 1253, 1255, 1255, 1257, 1257, 1259, 1259, 1261, 1261, 1263, 1263,
    1265, 1265, 1267, 1267, 1269, 1269, 1271, 1271, 1273, 1273, 1275, 1275, 1277, 1277, 1279, 1279,
    1281, 1281, 1283, 1283, 1285, 1285, 1287, 1287, 1289, 1289, 1291, 1291, 1293, 1293, 1295, 1295,
    1297, 1297, 1299, 1299, 1301, 1301, 1303, 1303, 1305, 1305, 1307, 1307, 1309, 1309, 1311, 1311,
    1313, 1313, 1315, 1315, 1317, 1317, 1319, 1319, 1321, 1321, 1323, 1323, 1325, 1325, 1327, 1327,
    1328, 1377, 1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1386, 1387, 1388, 1389, 1390, 1391,
    1392, 1393, 1394, 1395, 1396, 1397, 1398, 1399, 1400, 1401, 1402, 1403, 1404, 1405, 1406, 1407,
    1408, 1409, 1410, 1411, 1412, 1413, 1414, 1367, 1368, 1369, 1370, 1371, 1372, 1373, 1374, 1375,
    1376, 1377, 1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1386, 1387, 1388, 1389, 1390, 1391,
    1392, 1393, 1394, 1395, 1396, 1397, 1398, 1399, 1400, 1401, 1402, 1403, 1404, 1405, 1406, 1407,
    1408, 1409, 1410, 1411, 1412, 1413, 1414, 1415, 1416, 1417, 1418, 1419, 1420, 1421, 1422, 1423
};

static inline uint32_t lj_fnv1a(const char* s, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h ? h : 1u;
}

static inline const char* lj_lookup_exc_hashed(
    const lj_exc_hash_t* LJ_RESTRICT ht,
    const char* LJ_RESTRICT word,
    size_t len,
    uint32_t h,
    size_t* LJ_RESTRICT out_len
) {
    if (len < 64 && !(ht->len_mask & (1ULL << len))) return NULL;
    if (!ht->slots) return NULL;
    uint32_t idx = h & ht->mask;
    for (;;) {
        const lj_exc_hash_slot_t* slot = &ht->slots[idx];
        uint32_t sh = slot->hash;
        if (sh == 0) return NULL;
        if (sh == h && slot->word_len == len &&
            memcmp(ht->blob + slot->word_off, word, len) == 0) {
            *out_len = slot->stem_len;
            return (const char*)(ht->blob + slot->stem_off);
        }
        idx = (idx + 1) & ht->mask;
    }
}

static inline int lj_strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && *s2) {
        int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (*s1 + 32) : (unsigned char)*s1;
        int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (*s2 + 32) : (unsigned char)*s2;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/* Core inference engine executing the V5+V4+V2 4-stage pipeline */
static int lj_stem_internal(
    const lemmajet_model_t* m,
    const char* word,
    size_t len,
    char* out,
    size_t cap,
    int is_lemmatize_call
) {
    if (!m || !word || (cap > 0 && !out)) return -1;

    if (len == (size_t)-1) {
        len = strlen(word);
    }

    if (len == 0) {
        if (cap > 0) out[0] = '\0';
        return 0;
    }

    /*
     * Stage 0: Single-pass UTF-8 decoding, case folding, is_ascii tracking, and inline FNV-1a.
     * No cp_arr[128] or byte_off[129] stack buffers.
     */
    char w_utf8[512];
    size_t byte_idx = 0;
    size_t w_len = 0;
    size_t utf8_len = 0;
    uint32_t h1 = 2166136261u;
    int is_ascii = 1;

    while (byte_idx < len) {
        if (w_len >= 127 || utf8_len >= 500) {
            if (cap < len + 1) return (int)len;
            memmove(out, word, len);
            out[len] = '\0';
            return (int)len;
        }

        uint8_t b0 = (uint8_t)word[byte_idx];
        if (b0 < 0x80) {
            uint8_t c = (uint8_t)(b0 | (((unsigned)(b0 - 'A') < 26u) << 5));
            w_utf8[utf8_len++] = (char)c;
            h1 = (h1 ^ c) * 16777619u;
            w_len++;
            byte_idx++;
            continue;
        }

        is_ascii = 0;
        uint32_t cp = 0;
        size_t char_bytes = 0;
        if ((b0 & 0xE0) == 0xC0) {
            if (byte_idx + 1 >= len) { cp = b0; char_bytes = 1; }
            else {
                uint8_t b1 = (uint8_t)word[byte_idx + 1];
                if ((b1 & 0xC0) == 0x80) { cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F); char_bytes = 2; }
                else { cp = b0; char_bytes = 1; }
            }
        } else if ((b0 & 0xF0) == 0xE0) {
            if (byte_idx + 2 >= len) { cp = b0; char_bytes = 1; }
            else {
                uint8_t b1 = (uint8_t)word[byte_idx + 1], b2 = (uint8_t)word[byte_idx + 2];
                if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
                    cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F); char_bytes = 3;
                } else { cp = b0; char_bytes = 1; }
            }
        } else if ((b0 & 0xF8) == 0xF0) {
            if (byte_idx + 3 >= len) { cp = b0; char_bytes = 1; }
            else {
                uint8_t b1 = (uint8_t)word[byte_idx + 1], b2 = (uint8_t)word[byte_idx + 2], b3 = (uint8_t)word[byte_idx + 3];
                if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80) {
                    cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F); char_bytes = 4;
                } else { cp = b0; char_bytes = 1; }
            }
        } else {
            cp = b0;
            char_bytes = 1;
        }

        byte_idx += char_bytes;
        if (cp < 1424) cp = LJ_LOWER_MAP[cp];
        w_len++;

        if (cp < 0x80) {
            uint8_t c0 = (uint8_t)cp;
            w_utf8[utf8_len++] = (char)c0;
            h1 = (h1 ^ c0) * 16777619u;
        } else if (cp < 0x800) {
            uint8_t c0 = (uint8_t)(0xC0 | (cp >> 6)), c1 = (uint8_t)(0x80 | (cp & 0x3F));
            w_utf8[utf8_len++] = (char)c0; w_utf8[utf8_len++] = (char)c1;
            h1 = (h1 ^ c0) * 16777619u; h1 = (h1 ^ c1) * 16777619u;
        } else if (cp < 0x10000) {
            uint8_t c0 = (uint8_t)(0xE0 | (cp >> 12)), c1 = (uint8_t)(0x80 | ((cp >> 6) & 0x3F)), c2 = (uint8_t)(0x80 | (cp & 0x3F));
            w_utf8[utf8_len++] = (char)c0; w_utf8[utf8_len++] = (char)c1; w_utf8[utf8_len++] = (char)c2;
            h1 = (h1 ^ c0) * 16777619u; h1 = (h1 ^ c1) * 16777619u; h1 = (h1 ^ c2) * 16777619u;
        } else {
            uint8_t c0 = (uint8_t)(0xF0 | (cp >> 18)), c1 = (uint8_t)(0x80 | ((cp >> 12) & 0x3F)), c2 = (uint8_t)(0x80 | ((cp >> 6) & 0x3F)), c3 = (uint8_t)(0x80 | (cp & 0x3F));
            w_utf8[utf8_len++] = (char)c0; w_utf8[utf8_len++] = (char)c1; w_utf8[utf8_len++] = (char)c2; w_utf8[utf8_len++] = (char)c3;
            h1 = (h1 ^ c0) * 16777619u; h1 = (h1 ^ c1) * 16777619u; h1 = (h1 ^ c2) * 16777619u; h1 = (h1 ^ c3) * 16777619u;
        }
    }
    w_utf8[utf8_len] = '\0';
    if (h1 == 0) h1 = 1u;

    /* Stage 1: Hashed exception lookup */
    size_t el = 0;
    const lj_exc_hash_t* LJ_RESTRICT ht = &m->exc_hash;
    const char* exc = lj_lookup_exc_hashed(ht, w_utf8, utf8_len, h1, &el);
    if (exc != NULL) {
        if (cap < el + 1) return (int)el;
        memcpy(out, exc, el);
        out[el] = '\0';
        return (int)el;
    }

    const uint8_t shift = m->shift;
    const uint8_t* LJ_RESTRICT bcid = m->byte_cid;

    /* Stage 2: Byte-level forward prefix trie */
    size_t bestP_cp = 0;
    size_t bestP_bytes = 0;
    if (m->num_pfx_states > 0 && (m->pfx_next16 || m->pfx_next) && m->pfx_need && m->pfx_op && bcid) {
        int32_t pstate = 0;
        const uint8_t* LJ_RESTRICT pneed = m->pfx_need;
        const int8_t* LJ_RESTRICT pop = m->pfx_op;
        if (pneed[0] <= w_len && pop[0] >= 0) bestP_cp = (size_t)pop[0];

        if (m->pfx_next16) {
            const int16_t* LJ_RESTRICT pnext16 = m->pfx_next16;
            for (size_t i = 0; i < utf8_len; i++) {
                uint8_t cid = bcid[(uint8_t)w_utf8[i]];
                int32_t nxt = (int32_t)pnext16[(pstate << shift) | cid];
                if (nxt > 0) {
                    pstate = nxt;
                    if (pneed[pstate] <= w_len && pop[pstate] >= 0) bestP_cp = (size_t)pop[pstate];
                } else if (nxt < 0) {
                    int32_t s = ~nxt;
                    if (pneed[s] <= w_len && pop[s] >= 0) bestP_cp = (size_t)pop[s];
                    break;
                } else {
                    break;
                }
            }
        } else {
            const int32_t* LJ_RESTRICT pnext32 = m->pfx_next;
            for (size_t i = 0; i < utf8_len; i++) {
                uint8_t cid = bcid[(uint8_t)w_utf8[i]];
                int32_t nxt = pnext32[(pstate << shift) | cid];
                if (nxt > 0) {
                    pstate = nxt;
                    if (pneed[pstate] <= w_len && pop[pstate] >= 0) bestP_cp = (size_t)pop[pstate];
                } else if (nxt < 0) {
                    int32_t s = ~nxt;
                    if (pneed[s] <= w_len && pop[s] >= 0) bestP_cp = (size_t)pop[s];
                    break;
                } else {
                    break;
                }
            }
        }

        if (bestP_cp > 0) {
            if (is_ascii) {
                bestP_bytes = bestP_cp;
            } else {
                size_t b = 0, cp_seen = 0;
                while (b < utf8_len && cp_seen < bestP_cp) {
                    uint8_t bx = (uint8_t)w_utf8[b];
                    if (bx < 0x80) b += 1;
                    else if ((bx & 0xE0) == 0xC0) b += 2;
                    else if ((bx & 0xF0) == 0xE0) b += 3;
                    else b += 4;
                    cp_seen++;
                }
                bestP_bytes = b;
            }
        }
    } else if (m->prefix && m->prefix_len > 0) {
        if (w_len > (size_t)m->prefix_len + (size_t)m->affix_min_stem_len) {
            size_t p_bytes = strlen(m->prefix);
            if (utf8_len >= p_bytes && memcmp(w_utf8, m->prefix, p_bytes) == 0) {
                bestP_cp = (size_t)m->prefix_len;
                bestP_bytes = p_bytes;
            }
        }
    }

    const char* active_utf8 = w_utf8 + bestP_bytes;
    size_t active_utf8_len = utf8_len - bestP_bytes;
    size_t active_w_len = w_len - bestP_cp;

    if (bestP_cp > 0) {
        /* Stage 2.5: Intermediate exception lookup post-prefix */
        uint32_t hp = lj_fnv1a(active_utf8, active_utf8_len);
        const char* exc_pfx = lj_lookup_exc_hashed(ht, active_utf8, active_utf8_len, hp, &el);
        if (exc_pfx != NULL) {
            if (cap < el + 1) return (int)el;
            memcpy(out, exc_pfx, el);
            out[el] = '\0';
            return (int)el;
        }
    }

    /* Enclitics */
    if (m->num_enclitics > 0 && m->enclitics) {
        for (size_t e = 0; e < m->num_enclitics; e++) {
            const char* enc = m->enclitics[e];
            size_t enc_byte_len = m->enclitic_lens ? (size_t)m->enclitic_lens[e] : strlen(enc);
            uint8_t enc_cp = m->enclitic_cp_lens ? m->enclitic_cp_lens[e] : (uint8_t)enc_byte_len;
            if (active_utf8_len >= enc_byte_len &&
                memcmp(active_utf8 + active_utf8_len - enc_byte_len, enc, enc_byte_len) == 0 &&
                active_w_len >= (size_t)enc_cp + (size_t)m->affix_min_stem_len) {
                active_w_len -= enc_cp;
                active_utf8_len -= enc_byte_len;
                uint32_t he = lj_fnv1a(active_utf8, active_utf8_len);
                const char* exc_enc = lj_lookup_exc_hashed(ht, active_utf8, active_utf8_len, he, &el);
                if (exc_enc != NULL) {
                    if (cap < el + 1) return (int)el;
                    memcpy(out, exc_enc, el);
                    out[el] = '\0';
                    return (int)el;
                }
                break;
            }
        }
    }

    /* Stage 3: Byte-level suffix trie walk (DAT or hoisted dense next16/next32 + restrict) */
    int32_t bestOp = -1;
    if (m->dat_nodes && m->num_dat_nodes > 0 && bcid) {
        const lj_dat_node_t* LJ_RESTRICT nodes = m->dat_nodes;
        const uint32_t num_nodes = m->num_dat_nodes;
        int32_t state = 0;
        if (nodes[0].need <= active_w_len) bestOp = (int32_t)nodes[0].op;

        for (int i = (int)active_utf8_len - 1; i >= 0; i--) {
            int32_t b = nodes[state].base;
            if (b < 0) break;
            uint8_t cid = bcid[(uint8_t)active_utf8[i]];
            uint32_t t = (uint32_t)(b + cid);
            if (t >= num_nodes || nodes[t].check != state) break;
            state = (int32_t)t;
            if (nodes[state].need <= active_w_len) bestOp = (int32_t)nodes[state].op;
        }
    } else if (m->num_states > 0 && (m->next16 || m->next) && m->need && m->op && bcid) {
        int32_t state = 0;
        const uint8_t* LJ_RESTRICT need = m->need;
        const int32_t* LJ_RESTRICT op = m->op;
        if (need[0] <= active_w_len) bestOp = op[0];

        if (m->next16) {
            const int16_t* LJ_RESTRICT next16 = m->next16;
            for (int i = (int)active_utf8_len - 1; i >= 0; i--) {
                uint8_t cid = bcid[(uint8_t)active_utf8[i]];
                int32_t nxt = (int32_t)next16[(state << shift) | cid];
                if (nxt > 0) {
                    state = nxt;
                    if (need[state] <= active_w_len) bestOp = op[state];
                } else if (nxt < 0) {
                    int32_t s = ~nxt;
                    if (need[s] <= active_w_len) bestOp = op[s];
                    break;
                } else {
                    break;
                }
            }
        } else {
            const int32_t* LJ_RESTRICT next32 = m->next;
            for (int i = (int)active_utf8_len - 1; i >= 0; i--) {
                uint8_t cid = bcid[(uint8_t)active_utf8[i]];
                int32_t nxt = next32[(state << shift) | cid];
                if (nxt > 0) {
                    state = nxt;
                    if (need[state] <= active_w_len) bestOp = op[state];
                } else if (nxt < 0) {
                    int32_t s = ~nxt;
                    if (need[s] <= active_w_len) bestOp = op[s];
                    break;
                } else {
                    break;
                }
            }
        }
    }

    /* Direct-to-out write + UTF-8 char boundary cut */
    size_t stem_len = 0;
    size_t stem_cp_len = 0;
    if (bestOp >= 0) {
        uint8_t cut = m->op_cut ? m->op_cut[bestOp] : 0;
        const char* add = (m->op_add && m->op_add[bestOp]) ? m->op_add[bestOp] : "";
        size_t add_len = m->op_add_len ? (size_t)m->op_add_len[bestOp] : strlen(add);
        size_t add_cp_len = m->op_add_cp_len ? (size_t)m->op_add_cp_len[bestOp] : add_len;

        size_t keep_byte_len = active_utf8_len;
        if (cut > 0) {
            if (is_ascii) {
                keep_byte_len = (active_utf8_len >= cut) ? (active_utf8_len - cut) : 0;
            } else {
                for (uint8_t k = 0; k < cut && keep_byte_len > 0; k++) {
                    do {
                        keep_byte_len--;
                    } while (keep_byte_len > 0 && (((uint8_t)active_utf8[keep_byte_len] & 0xC0) == 0x80));
                }
            }
        }

        stem_len = keep_byte_len + add_len;
        stem_cp_len = (active_w_len >= cut ? active_w_len - cut : 0) + add_cp_len;
        if (stem_len >= 511) {
            if (cap < active_utf8_len + 1) return (int)active_utf8_len;
            memcpy(out, active_utf8, active_utf8_len);
            out[active_utf8_len] = '\0';
            return (int)active_utf8_len;
        }
        if (cap < stem_len + 1) return (int)stem_len;
        memcpy(out, active_utf8, keep_byte_len);
        if (add_len > 0) memcpy(out + keep_byte_len, add, add_len);
        out[stem_len] = '\0';

        /* Stage 3.5: Synthetic stem bridge lookup in exceptions */
        if (cut > 0 || add_len > 0) {
            int can_bridge = m->is_lemmatizer ? (m->op_br ? (m->op_br[bestOp] == 1) : 1) : 1;
            if (can_bridge && (stem_len >= 64 || (ht->len_mask & (1ULL << stem_len)))) {
                uint32_t hb = lj_fnv1a(out, stem_len);
                const char* exc_bridge = lj_lookup_exc_hashed(ht, out, stem_len, hb, &el);
                if (exc_bridge != NULL) {
                    if (cap < el + 1) return (int)el;
                    memcpy(out, exc_bridge, el);
                    out[el] = '\0';
                    return (int)el;
                }
            }
        }
    } else {
        stem_len = active_utf8_len;
        stem_cp_len = active_w_len;
        if (cap < stem_len + 1) return (int)stem_len;
        memcpy(out, active_utf8, stem_len);
        out[stem_len] = '\0';
    }

    /* Stage 4: Tail substitutions in-place on out (precomputed lengths + arithmetic stem_cp_len) */
    if (m->num_tail_substitutions > 0 && m->tail_substitutions) {
        for (size_t t = 0; t < m->num_tail_substitutions; t++) {
            const lemmajet_tail_entry_t* sub = &m->tail_substitutions[t];
            size_t pat_bytes = (size_t)sub->pat_len;
            if (stem_len >= pat_bytes &&
                stem_cp_len >= (size_t)sub->pat_cp_len + 2 &&
                memcmp(out + stem_len - pat_bytes, sub->pat, pat_bytes) == 0) {
                size_t rep_bytes = (size_t)sub->rep_len;
                size_t new_len = stem_len - pat_bytes + rep_bytes;
                if (new_len < 511) {
                    if (cap < new_len + 1) return (int)new_len;
                    memcpy(out + stem_len - pat_bytes, sub->rep, rep_bytes);
                    stem_len = new_len;
                    out[stem_len] = '\0';
                }
                break;
            }
        }
    }

    (void)is_lemmatize_call;
    return (int)stem_len;
}

int lemmajet_stem(const lemmajet_model_t* m, const char* word, size_t len, char* out, size_t cap) {
    return lj_stem_internal(m, word, len, out, cap, 0);
}

int lemmajet_lemmatize(const lemmajet_model_t* m, const char* word, size_t len, char* out, size_t cap) {
    return lj_stem_internal(m, word, len, out, cap, 1);
}

static lemmajet_model_lookup_fn lj_global_model_lookup = NULL;

void lemmajet_set_model_lookup(lemmajet_model_lookup_fn fn) {
    lj_global_model_lookup = fn;
}

/* =========================================================================
 * Snowball libstemmer adapter implementation
 * ========================================================================= */

#if defined(LEMMAJET_ENABLE_SNOWBALL_COMPAT) || defined(LEMMAJET_SNOWBALL_COMPAT)
struct sb_stemmer {
    const lemmajet_model_t* model;
    char buf[512];
    int len;
};

struct sb_stemmer* lj_sb_stemmer_new_with_model(const lemmajet_model_t* model) {
    if (!model) return NULL;
    struct sb_stemmer* s = (struct sb_stemmer*)malloc(sizeof(struct sb_stemmer));
    if (!s) return NULL;
    s->model = model;
    s->len = 0;
    s->buf[0] = '\0';
    return s;
}

struct sb_stemmer* lj_sb_stemmer_new(const char* algorithm, const char* charenc) {
    if (!algorithm) return NULL;
    if (charenc != NULL) {
        if (lj_strcasecmp(charenc, "UTF_8") != 0 && lj_strcasecmp(charenc, "UTF-8") != 0) {
            return NULL;
        }
    }
    const lemmajet_model_t* m = NULL;
    if (lj_global_model_lookup != NULL) {
        m = lj_global_model_lookup(algorithm);
    }
    if (!m) return NULL;
    return lj_sb_stemmer_new_with_model(m);
}

void lj_sb_stemmer_delete(struct sb_stemmer* stemmer) {
    if (stemmer) {
        free(stemmer);
    }
}

const sb_symbol* lj_sb_stemmer_stem(struct sb_stemmer* stemmer, const sb_symbol* word, int size) {
    if (!stemmer || !stemmer->model || !word || size < 0) return NULL;
    int res = lemmajet_stem(stemmer->model, (const char*)word, (size_t)size, stemmer->buf, sizeof(stemmer->buf));
    if (res >= 0 && (size_t)res < sizeof(stemmer->buf)) {
        stemmer->len = res;
    } else {
        stemmer->len = 0;
        stemmer->buf[0] = '\0';
    }
    return (const sb_symbol*)stemmer->buf;
}

int lj_sb_stemmer_length(struct sb_stemmer* stemmer) {
    return stemmer ? stemmer->len : 0;
}

static const char* const LJ_SNOWBALL_LIST[] = {
    "polish", "english", "german", "spanish", "french", "czech",
    "pl", "en", "de", "es", "fr", "cs",
    NULL
};

const char** lj_sb_stemmer_list(void) {
    return (const char**)LJ_SNOWBALL_LIST;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEMMAJET_IMPLEMENTATION_INCLUDED */
#endif /* LEMMAJET_IMPLEMENTATION */
