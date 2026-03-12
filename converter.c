/*************************************************************
 * converter.c - Standalone custom encoding converter
 * UTF-8 <-> Unicode <-> ISCII <-> Acharya 2-byte
 * All logic from large-code-base/libimli2-branch-cnj4
 *************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

typedef unsigned char  byte_t;
typedef unsigned short syl_t;

#define MAX_CONSONANTS  64
#define MAX_CONJUNCTS   64
#define MAX_VOWELS      16
#define N_CONSONANTS    55

#define CONSONANT_MASK 0xFC00
#define CONJUNCT_MASK  0x03F0
#define VOWEL_MASK     0x000F
#define ASCII_START    (0xFB << 8)
#define SPECIAL_START  (0xFC << 8)
#define SYL_INVALID    0xFFFF
#define SYL_TERMINATOR 0xFB00

#define SYLLABLE(c, cj, v) (((c)<<10)|(((cj)<<4)&CONJUNCT_MASK)|((v)&VOWEL_MASK))
#define CONSONANT(s) (((s)&CONSONANT_MASK)>>10)
#define CONJUNCT(s)  (((s)&CONJUNCT_MASK)>>4)
#define VOWEL(s)     ((s)&VOWEL_MASK)
#define IS_SYL_SPECIAL(s) (((s)&0xFF00)==SPECIAL_START)
#define IS_SYL_ASCII(s)   (((s)&0xFF00)==ASCII_START)
#define SWITCH_CODE 0xFA00
#define SWITCH_MASK 0xFF00
#define IS_SYL_SWITCH(s)  (((s)&SWITCH_MASK)==SWITCH_CODE)
#define LANGCODE_MASK 0x00FF

typedef enum { SCRIPT_UNSUPPORTED=-1, SCRIPT_SANSKRIT=0, SCRIPT_HINDI=1,
    SCRIPT_TAMIL=2, SCRIPT_TELUGU=3, SCRIPT_KANNADA=4, SCRIPT_MALAYALAM=5,
    SCRIPT_ORIYA=6, SCRIPT_BANGLA=7, SCRIPT_GUJARATI=8, SCRIPT_PUNJABI=9,
    SCRIPT_ASAMIYA=10, SCRIPT_UNUSED1=11, SCRIPT_MARATHI=12,
    SCRIPT_MAX=15, SCRIPT_ASCII=25 } imli_script_t;

typedef enum { TYPE_INVALID=-1, TYPE_CONSONANT=0, TYPE_VOWEL, TYPE_SPECIAL, TYPE_ASCII } imli_char_type_t;
typedef enum { FLAG_NONE=0, FLAG_STANDALONE_VOWEL=1, FLAG_INSERT_HALANT=2 } imli_compose_flag_t;

static const char* script_names[] = {
    "sanskrit","hindi","tamil","telugu","kannada","malayalam",
    "oriya","bangla","gujarati","punjabi","asamiya","filler1","marathi"
};

static const byte_t lang_switch_codes[] = {
    0x52,0x42,0x44,0x45,0x48,0x49,0x47,0x43,0x4A,0x4B,0x46,0x20,0x60
};

static byte_t imli_to_iscii_spl[] = {
    0xA2,0xA3,0xA1,0xEA,'@','!','\'','(',')',',','-','.','/','\\',';','?',0xAE
};

static byte_t imli_to_iscii_vow[] = {
    0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xE8
};

static byte_t imli_to_iscii_vow_matras[] = {
    '@',0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8
};

static byte_t imli_to_iscii_con[] = {
    '?',0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,
    0xBB,0xBC,0xBD,0xBE,0xBF,0xC0,0xC1,0xC2,0xC3,
    0xC4,0xC5,0xC6,0x20,0xC7,0xC8,0xC9,0xCA,0xCB,
    0xCC,0xCD,0xCE,0xCF,0x20,0x20,0x20,0xD0,0xD1,
    0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x20,0x20,0xE9
};

#define HALANT_V   15
#define NUKHTA_C   54
#define KA_C 1
#define SHA_C 40
#define KSHA_C 45
#define JA_C 8
#define NJA_C 10
#define JNYA_C 46
#define NA_C 20
#define NEX_C 21
#define RA_C 30
#define REX_C 31
#define RXL_C 32
#define RXXL_C 33
#define DDRA_C 43
#define DDRHA_C 44

static const byte_t iscii_halant = 0xE8;
static const byte_t iscii_nukhta = 0xE9;

/* ISCII <-> IMLI map */
typedef struct { imli_char_type_t type; byte_t value; imli_compose_flag_t flags; } iscii_imli_map_rec;
static iscii_imli_map_rec iscii_imli_map[256];

static void setup_iscii_imli_table(void) {
    int i;
    for(i=0;i<128;i++){iscii_imli_map[i].type=TYPE_ASCII;iscii_imli_map[i].value=i;iscii_imli_map[i].flags=FLAG_NONE;}
    for(i=0;i<16;i++){iscii_imli_map[imli_to_iscii_vow[i]].type=TYPE_VOWEL;iscii_imli_map[imli_to_iscii_vow[i]].value=i;iscii_imli_map[imli_to_iscii_vow[i]].flags=FLAG_STANDALONE_VOWEL;}
    int ncon=sizeof(imli_to_iscii_con)/sizeof(imli_to_iscii_con[0]);
    for(i=0;i<ncon;i++){iscii_imli_map[imli_to_iscii_con[i]].type=TYPE_CONSONANT;iscii_imli_map[imli_to_iscii_con[i]].value=i;iscii_imli_map[imli_to_iscii_con[i]].flags=FLAG_NONE;}
    for(i=0;i<16;i++){iscii_imli_map[imli_to_iscii_vow_matras[i]].type=TYPE_VOWEL;iscii_imli_map[imli_to_iscii_vow_matras[i]].value=i;iscii_imli_map[imli_to_iscii_vow_matras[i]].flags=FLAG_NONE;}
    int nspl=sizeof(imli_to_iscii_spl)/sizeof(imli_to_iscii_spl[0]);
    for(i=0;i<nspl;i++){iscii_imli_map[imli_to_iscii_spl[i]].type=TYPE_SPECIAL;iscii_imli_map[imli_to_iscii_spl[i]].value=i;iscii_imli_map[imli_to_iscii_spl[i]].flags=FLAG_NONE;}
}

/* Conjunct table */
typedef struct { int count; unsigned int conjuncts[MAX_CONJUNCTS]; } cnj_entry_t;
static cnj_entry_t conjunct_table[MAX_CONSONANTS];

static void add_cnj(int base, int c2, int c3, int c4) {
    cnj_entry_t *e = &conjunct_table[base];
    unsigned int v = c2 | (c3<<8) | (c4<<16);
    e->count++;
    if(e->count < MAX_CONJUNCTS) e->conjuncts[e->count] = v;
}

static void init_conjunct_tables(void) {
    memset(conjunct_table, 0, sizeof(conjunct_table));
    add_cnj(1,1,0,0);
    add_cnj(1,1,28,0);
    add_cnj(1,1,29,0);
    add_cnj(1,2,0,0);
    add_cnj(1,6,0,0);
    add_cnj(1,8,0,0);
    add_cnj(1,9,0,0);
    add_cnj(1,11,0,0);
    add_cnj(1,11,30,0);
    add_cnj(1,11,41,0);
    add_cnj(1,15,0,0);
    add_cnj(1,16,0,0);
    add_cnj(1,16,28,0);
    add_cnj(1,16,29,0);
    add_cnj(1,16,30,0);
    add_cnj(1,16,38,0);
    add_cnj(1,17,0,0);
    add_cnj(1,17,20,0);
    add_cnj(1,17,28,0);
    add_cnj(1,17,29,0);
    add_cnj(1,18,0,0);
    add_cnj(1,20,0,0);
    add_cnj(1,20,28,0);
    add_cnj(1,20,29,0);
    add_cnj(1,23,0,0);
    add_cnj(1,24,0,0);
    add_cnj(1,24,54,0);
    add_cnj(1,25,0,0);
    add_cnj(1,27,0,0);
    add_cnj(1,27,28,0);
    add_cnj(1,27,29,0);
    add_cnj(1,28,0,0);
    add_cnj(1,29,0,0);
    add_cnj(1,30,0,0);
    add_cnj(1,30,28,0);
    add_cnj(1,30,29,0);
    add_cnj(1,35,0,0);
    add_cnj(1,35,28,0);
    add_cnj(1,35,29,0);
    add_cnj(1,36,0,0);
    add_cnj(1,38,0,0);
    add_cnj(1,39,0,0);
    add_cnj(1,41,0,0);
    add_cnj(1,41,16,0);
    add_cnj(1,41,38,0);
    add_cnj(1,54,0,0);
    add_cnj(1,54,35,0);
    add_cnj(1,54,41,0);
    add_cnj(2,45,0,0);
    add_cnj(2,2,0,0);
    add_cnj(2,2,28,0);
    add_cnj(2,2,29,0);
    add_cnj(2,16,0,0);
    add_cnj(2,20,0,0);
    add_cnj(2,27,0,0);
    add_cnj(2,28,0,0);
    add_cnj(2,29,0,0);
    add_cnj(2,30,0,0);
    add_cnj(2,34,0,0);
    add_cnj(2,35,0,0);
    add_cnj(2,36,0,0);
    add_cnj(2,38,0,0);
    add_cnj(2,39,0,0);
    add_cnj(2,40,0,0);
    add_cnj(2,41,0,0);
    add_cnj(2,54,0,0);
    add_cnj(2,54,41,0);
    add_cnj(3,3,0,0);
    add_cnj(3,3,28,0);
    add_cnj(3,3,29,0);
    add_cnj(3,4,0,0);
    add_cnj(3,4,34,0);
    add_cnj(3,6,0,0);
    add_cnj(3,7,0,0);
    add_cnj(3,8,0,0);
    add_cnj(3,11,0,0);
    add_cnj(3,12,0,0);
    add_cnj(3,13,0,0);
    add_cnj(3,15,0,0);
    add_cnj(3,16,0,0);
    add_cnj(3,18,0,0);
    add_cnj(3,19,0,0);
    add_cnj(3,20,0,0);
    add_cnj(3,20,28,0);
    add_cnj(3,20,29,0);
    add_cnj(3,23,0,0);
    add_cnj(3,25,0,0);
    add_cnj(3,26,0,0);
    add_cnj(3,26,30,0);
    add_cnj(3,26,34,0);
    add_cnj(3,27,0,0);
    add_cnj(3,28,0,0);
    add_cnj(3,29,0,0);
    add_cnj(3,30,0,0);
    add_cnj(3,30,28,0);
    add_cnj(3,30,29,0);
    add_cnj(3,34,0,0);
    add_cnj(3,35,0,0);
    add_cnj(3,35,28,0);
    add_cnj(3,35,29,0);
    add_cnj(3,36,0,0);
    add_cnj(3,38,0,0);
    add_cnj(3,38,28,0);
    add_cnj(3,38,29,0);
    add_cnj(3,39,0,0);
    add_cnj(3,40,0,0);
    add_cnj(3,41,0,0);
    add_cnj(3,54,0,0);
    add_cnj(4,4,0,0);
    add_cnj(4,4,28,0);
    add_cnj(4,4,29,0);
    add_cnj(4,15,0,0);
    add_cnj(4,20,0,0);
    add_cnj(4,20,28,0);
    add_cnj(4,20,29,0);
    add_cnj(4,27,0,0);
    add_cnj(4,28,0,0);
    add_cnj(4,29,0,0);
    add_cnj(4,30,0,0);
    add_cnj(4,34,0,0);
    add_cnj(4,35,0,0);
    add_cnj(4,36,0,0);
    add_cnj(4,38,0,0);
    add_cnj(4,39,0,0);
    add_cnj(4,40,0,0);
    add_cnj(4,41,0,0);
    add_cnj(5,1,0,0);
    add_cnj(5,1,16,0);
    add_cnj(5,1,28,0);
    add_cnj(5,1,29,0);
    add_cnj(5,1,30,0);
    add_cnj(5,45,0,0);
    add_cnj(5,2,0,0);
    add_cnj(5,2,28,0);
    add_cnj(5,2,29,0);
    add_cnj(5,3,0,0);
    add_cnj(5,3,28,0);
    add_cnj(5,3,29,0);
    add_cnj(5,3,30,0);
    add_cnj(5,3,34,0);
    add_cnj(5,3,35,0);
    add_cnj(5,4,0,0);
    add_cnj(5,4,28,0);
    add_cnj(5,4,29,0);
    add_cnj(5,4,30,0);
    add_cnj(5,4,34,0);
    add_cnj(5,5,0,0);
    add_cnj(5,16,0,0);
    add_cnj(5,16,38,0);
    add_cnj(5,27,0,0);
    add_cnj(5,28,0,0);
    add_cnj(5,29,0,0);
    add_cnj(6,3,0,0);
    add_cnj(6,6,0,0);
    add_cnj(6,6,28,0);
    add_cnj(6,6,29,0);
    add_cnj(6,7,0,0);
    add_cnj(6,7,6,0);
    add_cnj(6,7,25,0);
    add_cnj(6,7,30,0);
    add_cnj(6,7,34,0);
    add_cnj(6,7,38,0);
    add_cnj(6,10,0,0);
    add_cnj(6,20,0,0);
    add_cnj(6,23,0,0);
    add_cnj(6,24,0,0);
    add_cnj(6,26,0,0);
    add_cnj(6,27,0,0);
    add_cnj(6,28,0,0);
    add_cnj(6,29,0,0);
    add_cnj(6,30,0,0);
    add_cnj(6,34,0,0);
    add_cnj(6,35,0,0);
    add_cnj(6,36,0,0);
    add_cnj(6,38,0,0);
    add_cnj(6,39,0,0);
    add_cnj(6,40,0,0);
    add_cnj(6,41,0,0);
    add_cnj(6,54,0,0);
    add_cnj(7,6,0,0);
    add_cnj(7,7,0,0);
    add_cnj(7,7,28,0);
    add_cnj(7,7,29,0);
    add_cnj(7,20,0,0);
    add_cnj(7,27,0,0);
    add_cnj(7,28,0,0);
    add_cnj(7,29,0,0);
    add_cnj(7,30,0,0);
    add_cnj(7,34,0,0);
    add_cnj(7,35,0,0);
    add_cnj(7,38,0,0);
    add_cnj(7,41,0,0);
    add_cnj(8,3,0,0);
    add_cnj(8,6,0,0);
    add_cnj(8,8,0,0);
    add_cnj(8,8,25,0);
    add_cnj(8,8,28,0);
    add_cnj(8,8,29,0);
    add_cnj(8,8,38,0);
    add_cnj(8,8,54,0);
    add_cnj(8,46,0,0);
    add_cnj(8,9,0,0);
    add_cnj(8,16,0,0);
    add_cnj(8,18,0,0);
    add_cnj(8,19,0,0);
    add_cnj(8,20,0,0);
    add_cnj(8,23,0,0);
    add_cnj(8,24,0,0);
    add_cnj(8,25,0,0);
    add_cnj(8,26,0,0);
    add_cnj(8,27,0,0);
    add_cnj(8,28,0,0);
    add_cnj(8,29,0,0);
    add_cnj(8,30,0,0);
    add_cnj(8,34,0,0);
    add_cnj(8,35,0,0);
    add_cnj(8,38,0,0);
    add_cnj(8,39,0,0);
    add_cnj(8,40,0,0);
    add_cnj(8,41,0,0);
    add_cnj(8,54,0,0);
    add_cnj(9,9,0,0);
    add_cnj(9,9,28,0);
    add_cnj(9,9,29,0);
    add_cnj(9,20,0,0);
    add_cnj(9,27,0,0);
    add_cnj(9,28,0,0);
    add_cnj(9,29,0,0);
    add_cnj(9,30,0,0);
    add_cnj(9,34,0,0);
    add_cnj(9,35,0,0);
    add_cnj(9,38,0,0);
    add_cnj(9,40,0,0);
    add_cnj(9,41,0,0);
    add_cnj(10,6,0,0);
    add_cnj(10,7,0,0);
    add_cnj(10,8,0,0);
    add_cnj(10,8,6,0);
    add_cnj(10,8,28,0);
    add_cnj(10,8,29,0);
    add_cnj(10,8,38,0);
    add_cnj(10,9,0,0);
    add_cnj(10,10,0,0);
    add_cnj(10,10,28,0);
    add_cnj(10,10,29,0);
    add_cnj(10,14,0,0);
    add_cnj(10,23,0,0);
    add_cnj(10,25,0,0);
    add_cnj(10,28,0,0);
    add_cnj(10,29,0,0);
    add_cnj(10,34,0,0);
    add_cnj(11,1,0,0);
    add_cnj(11,2,0,0);
    add_cnj(11,4,0,0);
    add_cnj(11,6,0,0);
    add_cnj(11,8,0,0);
    add_cnj(11,8,54,0);
    add_cnj(11,9,0,0);
    add_cnj(11,11,0,0);
    add_cnj(11,11,28,0);
    add_cnj(11,11,29,0);
    add_cnj(11,12,0,0);
    add_cnj(11,12,28,0);
    add_cnj(11,12,29,0);
    add_cnj(11,20,0,0);
    add_cnj(11,16,0,0);
    add_cnj(11,16,30,0);
    add_cnj(11,16,34,0);
    add_cnj(11,23,0,0);
    add_cnj(11,25,0,0);
    add_cnj(11,27,0,0);
    add_cnj(11,28,0,0);
    add_cnj(11,29,0,0);
    add_cnj(11,30,0,0);
    add_cnj(11,30,28,0);
    add_cnj(11,30,29,0);
    add_cnj(11,34,0,0);
    add_cnj(11,35,0,0);
    add_cnj(11,36,0,0);
    add_cnj(11,38,0,0);
    add_cnj(11,41,0,0);
    add_cnj(11,41,16,0);
    add_cnj(12,1,0,0);
    add_cnj(12,12,0,0);
    add_cnj(12,12,28,0);
    add_cnj(12,12,29,0);
    add_cnj(12,27,0,0);
    add_cnj(12,28,0,0);
    add_cnj(12,29,0,0);
    add_cnj(12,30,0,0);
    add_cnj(12,34,0,0);
    add_cnj(12,36,0,0);
    add_cnj(12,38,0,0);
    add_cnj(13,1,0,0);
    add_cnj(13,3,0,0);
    add_cnj(13,4,0,0);
    add_cnj(13,4,30,0);
    add_cnj(13,6,0,0);
    add_cnj(13,8,0,0);
    add_cnj(13,13,0,0);
    add_cnj(13,13,28,0);
    add_cnj(13,13,29,0);
    add_cnj(13,14,0,0);
    add_cnj(13,15,0,0);
    add_cnj(13,16,0,0);
    add_cnj(13,20,0,0);
    add_cnj(13,27,0,0);
    add_cnj(13,28,0,0);
    add_cnj(13,29,0,0);
    add_cnj(13,30,0,0);
    add_cnj(13,30,28,0);
    add_cnj(13,30,29,0);
    add_cnj(13,34,0,0);
    add_cnj(13,35,0,0);
    add_cnj(13,36,0,0);
    add_cnj(13,38,0,0);
    add_cnj(13,41,0,0);
    add_cnj(13,54,0,0);
    add_cnj(14,14,0,0);
    add_cnj(14,14,28,0);
    add_cnj(14,14,29,0);
    add_cnj(14,28,0,0);
    add_cnj(14,29,0,0);
    add_cnj(14,30,0,0);
    add_cnj(14,34,0,0);
    add_cnj(14,36,0,0);
    add_cnj(14,54,0,0);
    add_cnj(15,1,0,0);
    add_cnj(15,3,0,0);
    add_cnj(15,11,0,0);
    add_cnj(15,11,30,0);
    add_cnj(15,11,28,0);
    add_cnj(15,11,29,0);
    add_cnj(15,12,0,0);
    add_cnj(15,12,28,0);
    add_cnj(15,12,29,0);
    add_cnj(15,13,0,0);
    add_cnj(15,13,30,0);
    add_cnj(15,14,0,0);
    add_cnj(15,15,0,0);
    add_cnj(15,23,0,0);
    add_cnj(15,25,0,0);
    add_cnj(15,27,0,0);
    add_cnj(15,28,0,0);
    add_cnj(15,29,0,0);
    add_cnj(15,34,0,0);
    add_cnj(15,36,0,0);
    add_cnj(15,38,0,0);
    add_cnj(16,1,0,0);
    add_cnj(16,1,30,0);
    add_cnj(16,45,0,0);
    add_cnj(16,2,0,0);
    add_cnj(16,6,0,0);
    add_cnj(16,8,0,0);
    add_cnj(16,9,0,0);
    add_cnj(16,16,0,0);
    add_cnj(16,16,24,0);
    add_cnj(16,16,25,0);
    add_cnj(16,16,28,0);
    add_cnj(16,16,29,0);
    add_cnj(16,16,30,0);
    add_cnj(16,16,38,0);
    add_cnj(16,17,0,0);
    add_cnj(16,20,0,0);
    add_cnj(16,20,28,0);
    add_cnj(16,20,29,0);
    add_cnj(16,23,0,0);
    add_cnj(16,23,30,0);
    add_cnj(16,24,0,0);
    add_cnj(16,25,0,0);
    add_cnj(16,26,0,0);
    add_cnj(16,27,0,0);
    add_cnj(16,27,28,0);
    add_cnj(16,27,29,0);
    add_cnj(16,28,0,0);
    add_cnj(16,29,0,0);
    add_cnj(16,30,0,0);
    add_cnj(16,30,28,0);
    add_cnj(16,30,29,0);
    add_cnj(16,34,0,0);
    add_cnj(16,34,28,0);
    add_cnj(16,34,29,0);
    add_cnj(16,35,0,0);
    add_cnj(16,36,0,0);
    add_cnj(16,38,0,0);
    add_cnj(16,39,0,0);
    add_cnj(16,40,0,0);
    add_cnj(16,41,0,0);
    add_cnj(16,41,20,0);
    add_cnj(16,41,28,0);
    add_cnj(16,41,29,0);
    add_cnj(16,41,38,0);
    add_cnj(17,17,0,0);
    add_cnj(17,17,28,0);
    add_cnj(17,17,29,0);
    add_cnj(17,20,0,0);
    add_cnj(17,23,0,0);
    add_cnj(17,25,0,0);
    add_cnj(17,26,0,0);
    add_cnj(17,26,28,0);
    add_cnj(17,26,29,0);
    add_cnj(17,27,0,0);
    add_cnj(17,28,0,0);
    add_cnj(17,29,0,0);
    add_cnj(17,30,0,0);
    add_cnj(17,34,0,0);
    add_cnj(17,35,0,0);
    add_cnj(17,36,0,0);
    add_cnj(17,38,0,0);
    add_cnj(17,39,0,0);
    add_cnj(17,41,0,0);
    add_cnj(18,3,0,0);
    add_cnj(18,3,34,0);
    add_cnj(18,4,0,0);
    add_cnj(18,6,0,0);
    add_cnj(18,8,0,0);
    add_cnj(18,18,0,0);
    add_cnj(18,18,28,0);
    add_cnj(18,18,29,0);
    add_cnj(18,19,0,0);
    add_cnj(18,19,28,0);
    add_cnj(18,19,29,0);
    add_cnj(18,19,34,0);
    add_cnj(18,19,38,0);
    add_cnj(18,20,0,0);
    add_cnj(18,20,28,0);
    add_cnj(18,20,29,0);
    add_cnj(18,23,0,0);
    add_cnj(18,24,0,0);
    add_cnj(18,24,28,0);
    add_cnj(18,24,29,0);
    add_cnj(18,25,0,0);
    add_cnj(18,25,30,0);
    add_cnj(18,25,34,0);
    add_cnj(18,26,0,0);
    add_cnj(18,26,28,0);
    add_cnj(18,26,29,0);
    add_cnj(18,26,34,0);
    add_cnj(18,27,0,0);
    add_cnj(18,28,0,0);
    add_cnj(18,29,0,0);
    add_cnj(18,30,0,0);
    add_cnj(18,30,28,0);
    add_cnj(18,30,29,0);
    add_cnj(18,34,0,0);
    add_cnj(18,35,0,0);
    add_cnj(18,36,0,0);
    add_cnj(18,38,0,0);
    add_cnj(18,38,28,0);
    add_cnj(18,38,29,0);
    add_cnj(18,38,30,0);
    add_cnj(18,38,34,0);
    add_cnj(19,19,0,0);
    add_cnj(19,19,28,0);
    add_cnj(19,19,29,0);
    add_cnj(19,20,0,0);
    add_cnj(19,20,28,0);
    add_cnj(19,20,29,0);
    add_cnj(19,25,0,0);
    add_cnj(19,27,0,0);
    add_cnj(19,28,0,0);
    add_cnj(19,29,0,0);
    add_cnj(19,30,0,0);
    add_cnj(19,30,28,0);
    add_cnj(19,30,29,0);
    add_cnj(19,34,0,0);
    add_cnj(19,36,0,0);
    add_cnj(19,38,0,0);
    add_cnj(20,1,0,0);
    add_cnj(20,1,30,0);
    add_cnj(20,3,0,0);
    add_cnj(20,6,0,0);
    add_cnj(20,8,0,0);
    add_cnj(20,13,0,0);
    add_cnj(20,13,30,0);
    add_cnj(20,11,0,0);
    add_cnj(20,11,30,0);
    add_cnj(20,16,0,0);
    add_cnj(20,16,25,0);
    add_cnj(20,16,28,0);
    add_cnj(20,16,29,0);
    add_cnj(20,16,30,0);
    add_cnj(20,16,38,0);
    add_cnj(20,17,0,0);
    add_cnj(20,17,28,0);
    add_cnj(20,17,29,0);
    add_cnj(20,18,0,0);
    add_cnj(20,18,25,0);
    add_cnj(20,18,30,0);
    add_cnj(20,18,38,0);
    add_cnj(20,18,28,0);
    add_cnj(20,18,29,0);
    add_cnj(20,19,0,0);
    add_cnj(20,19,28,0);
    add_cnj(20,19,29,0);
    add_cnj(20,19,30,0);
    add_cnj(20,19,34,0);
    add_cnj(20,20,0,0);
    add_cnj(20,20,28,0);
    add_cnj(20,20,29,0);
    add_cnj(20,23,0,0);
    add_cnj(20,23,30,0);
    add_cnj(20,24,0,0);
    add_cnj(20,24,30,0);
    add_cnj(20,25,0,0);
    add_cnj(20,26,0,0);
    add_cnj(20,27,0,0);
    add_cnj(20,28,0,0);
    add_cnj(20,29,0,0);
    add_cnj(20,30,0,0);
    add_cnj(20,34,0,0);
    add_cnj(20,35,0,0);
    add_cnj(20,38,0,0);
    add_cnj(20,39,0,0);
    add_cnj(20,40,0,0);
    add_cnj(20,41,0,0);
    add_cnj(20,41,11,0);
    add_cnj(20,41,23,0);
    add_cnj(20,41,28,0);
    add_cnj(20,41,29,0);
    add_cnj(20,42,0,0);
    add_cnj(21,21,0,0);
    add_cnj(22,1,0,0);
    add_cnj(22,2,0,0);
    add_cnj(22,3,0,0);
    add_cnj(22,4,0,0);
    add_cnj(22,5,0,0);
    add_cnj(22,6,0,0);
    add_cnj(22,7,0,0);
    add_cnj(22,8,0,0);
    add_cnj(22,10,0,0);
    add_cnj(22,11,0,0);
    add_cnj(22,12,0,0);
    add_cnj(22,13,0,0);
    add_cnj(22,15,0,0);
    add_cnj(22,16,0,0);
    add_cnj(22,18,0,0);
    add_cnj(22,19,0,0);
    add_cnj(22,20,0,0);
    add_cnj(22,23,0,0);
    add_cnj(22,25,0,0);
    add_cnj(22,26,0,0);
    add_cnj(22,27,0,0);
    add_cnj(22,28,0,0);
    add_cnj(22,29,0,0);
    add_cnj(22,37,0,0);
    add_cnj(22,38,0,0);
    add_cnj(22,39,0,0);
    add_cnj(22,40,0,0);
    add_cnj(22,41,0,0);
    add_cnj(22,42,0,0);
    add_cnj(23,1,0,0);
    add_cnj(23,6,0,0);
    add_cnj(23,6,28,0);
    add_cnj(23,6,29,0);
    add_cnj(23,11,0,0);
    add_cnj(23,12,0,0);
    add_cnj(23,13,0,0);
    add_cnj(23,14,0,0);
    add_cnj(23,15,0,0);
    add_cnj(23,16,0,0);
    add_cnj(23,16,28,0);
    add_cnj(23,16,29,0);
    add_cnj(23,16,38,0);
    add_cnj(23,19,0,0);
    add_cnj(23,19,38,0);
    add_cnj(23,20,0,0);
    add_cnj(23,23,0,0);
    add_cnj(23,23,28,0);
    add_cnj(23,23,29,0);
    add_cnj(23,24,0,0);
    add_cnj(23,27,0,0);
    add_cnj(23,28,0,0);
    add_cnj(23,29,0,0);
    add_cnj(23,30,0,0);
    add_cnj(23,30,28,0);
    add_cnj(23,30,29,0);
    add_cnj(23,34,0,0);
    add_cnj(23,35,0,0);
    add_cnj(23,35,28,0);
    add_cnj(23,35,29,0);
    add_cnj(23,36,0,0);
    add_cnj(23,38,0,0);
    add_cnj(23,39,0,0);
    add_cnj(23,40,0,0);
    add_cnj(23,41,0,0);
    add_cnj(23,41,38,0);
    add_cnj(24,1,0,0);
    add_cnj(24,3,0,0);
    add_cnj(24,4,0,0);
    add_cnj(24,8,0,0);
    add_cnj(24,8,54,0);
    add_cnj(24,9,0,0);
    add_cnj(24,11,0,0);
    add_cnj(24,16,0,0);
    add_cnj(24,16,38,0);
    add_cnj(24,22,0,0);
    add_cnj(24,24,0,0);
    add_cnj(24,24,28,0);
    add_cnj(24,24,29,0);
    add_cnj(24,28,0,0);
    add_cnj(24,29,0,0);
    add_cnj(24,30,0,0);
    add_cnj(24,34,0,0);
    add_cnj(24,35,0,0);
    add_cnj(24,35,28,0);
    add_cnj(24,35,29,0);
    add_cnj(24,36,0,0);
    add_cnj(24,41,0,0);
    add_cnj(24,54,0,0);
    add_cnj(24,54,41,0);
    add_cnj(25,1,0,0);
    add_cnj(25,8,0,0);
    add_cnj(25,16,0,0);
    add_cnj(25,18,0,0);
    add_cnj(25,19,0,0);
    add_cnj(25,19,38,0);
    add_cnj(25,20,0,0);
    add_cnj(25,25,0,0);
    add_cnj(25,25,28,0);
    add_cnj(25,25,29,0);
    add_cnj(25,26,0,0);
    add_cnj(25,27,0,0);
    add_cnj(25,28,0,0);
    add_cnj(25,29,0,0);
    add_cnj(25,30,0,0);
    add_cnj(25,30,28,0);
    add_cnj(25,30,29,0);
    add_cnj(25,34,0,0);
    add_cnj(25,35,0,0);
    add_cnj(25,35,28,0);
    add_cnj(25,35,29,0);
    add_cnj(25,36,0,0);
    add_cnj(25,38,0,0);
    add_cnj(25,41,0,0);
    add_cnj(26,1,0,0);
    add_cnj(26,20,0,0);
    add_cnj(26,26,0,0);
    add_cnj(26,26,28,0);
    add_cnj(26,26,29,0);
    add_cnj(26,28,0,0);
    add_cnj(26,29,0,0);
    add_cnj(26,30,0,0);
    add_cnj(26,34,0,0);
    add_cnj(26,35,0,0);
    add_cnj(26,36,0,0);
    add_cnj(26,38,0,0);
    add_cnj(27,1,0,0);
    add_cnj(27,6,0,0);
    add_cnj(27,7,0,0);
    add_cnj(27,8,0,0);
    add_cnj(27,11,0,0);
    add_cnj(27,16,0,0);
    add_cnj(27,18,0,0);
    add_cnj(27,20,0,0);
    add_cnj(27,23,0,0);
    add_cnj(27,23,28,0);
    add_cnj(27,23,29,0);
    add_cnj(27,23,30,0);
    add_cnj(27,23,34,0);
    add_cnj(27,24,0,0);
    add_cnj(27,25,0,0);
    add_cnj(27,25,28,0);
    add_cnj(27,25,29,0);
    add_cnj(27,25,30,0);
    add_cnj(27,26,0,0);
    add_cnj(27,26,30,0);
    add_cnj(27,27,0,0);
    add_cnj(27,27,28,0);
    add_cnj(27,27,29,0);
    add_cnj(27,28,0,0);
    add_cnj(27,29,0,0);
    add_cnj(27,30,0,0);
    add_cnj(27,30,28,0);
    add_cnj(27,30,29,0);
    add_cnj(27,34,0,0);
    add_cnj(27,35,0,0);
    add_cnj(27,36,0,0);
    add_cnj(27,38,0,0);
    add_cnj(27,39,0,0);
    add_cnj(27,41,0,0);
    add_cnj(27,42,0,0);
    add_cnj(28,1,0,0);
    add_cnj(28,1,1,0);
    add_cnj(28,3,0,0);
    add_cnj(28,5,0,0);
    add_cnj(28,6,0,0);
    add_cnj(28,11,0,0);
    add_cnj(28,13,0,0);
    add_cnj(28,16,0,0);
    add_cnj(28,18,0,0);
    add_cnj(28,20,0,0);
    add_cnj(28,23,0,0);
    add_cnj(28,25,0,0);
    add_cnj(28,28,0,0);
    add_cnj(28,29,0,0);
    add_cnj(28,30,0,0);
    add_cnj(28,30,28,0);
    add_cnj(28,30,29,0);
    add_cnj(28,35,0,0);
    add_cnj(28,38,0,0);
    add_cnj(28,41,0,0);
    add_cnj(29,1,0,0);
    add_cnj(29,28,0,0);
    add_cnj(29,29,0,0);
    add_cnj(30,1,0,0);
    add_cnj(30,1,41,0);
    add_cnj(30,45,0,0);
    add_cnj(30,45,28,0);
    add_cnj(30,45,29,0);
    add_cnj(30,45,38,0);
    add_cnj(30,2,0,0);
    add_cnj(30,3,0,0);
    add_cnj(30,3,28,0);
    add_cnj(30,3,29,0);
    add_cnj(30,3,41,0);
    add_cnj(30,4,0,0);
    add_cnj(30,4,3,0);
    add_cnj(30,4,28,0);
    add_cnj(30,4,29,0);
    add_cnj(30,4,41,0);
    add_cnj(30,5,0,0);
    add_cnj(30,5,8,0);
    add_cnj(30,6,0,0);
    add_cnj(30,6,6,0);
    add_cnj(30,6,7,0);
    add_cnj(30,6,28,0);
    add_cnj(30,6,29,0);
    add_cnj(30,7,0,0);
    add_cnj(30,8,0,0);
    add_cnj(30,8,8,0);
    add_cnj(30,8,28,0);
    add_cnj(30,8,29,0);
    add_cnj(30,8,54,0);
    add_cnj(30,46,0,0);
    add_cnj(30,9,0,0);
    add_cnj(30,11,0,0);
    add_cnj(30,11,41,0);
    add_cnj(30,12,0,0);
    add_cnj(30,12,41,0);
    add_cnj(30,13,0,0);
    add_cnj(30,13,41,0);
    add_cnj(30,14,0,0);
    add_cnj(30,14,41,0);
    add_cnj(30,15,0,0);
    add_cnj(30,15,15,0);
    add_cnj(30,15,28,0);
    add_cnj(30,15,29,0);
    add_cnj(30,16,0,0);
    add_cnj(30,16,16,0);
    add_cnj(30,16,27,0);
    add_cnj(30,16,28,0);
    add_cnj(30,16,29,0);
    add_cnj(30,16,41,0);
    add_cnj(30,17,0,0);
    add_cnj(30,17,28,0);
    add_cnj(30,17,29,0);
    add_cnj(30,18,0,0);
    add_cnj(30,18,18,0);
    add_cnj(30,18,19,0);
    add_cnj(30,18,28,0);
    add_cnj(30,18,29,0);
    add_cnj(30,18,30,0);
    add_cnj(30,18,38,0);
    add_cnj(31,19,0,0);
    add_cnj(31,19,20,0);
    add_cnj(31,19,25,0);
    add_cnj(31,19,28,0);
    add_cnj(31,19,29,0);
    add_cnj(31,19,38,0);
    add_cnj(31,20,0,0);
    add_cnj(31,20,41,0);
    add_cnj(31,23,0,0);
    add_cnj(31,23,28,0);
    add_cnj(31,23,29,0);
    add_cnj(31,23,30,0);
    add_cnj(31,23,41,0);
    add_cnj(31,23,42,0);
    add_cnj(31,24,0,0);
    add_cnj(31,24,28,0);
    add_cnj(31,24,29,0);
    add_cnj(31,24,41,0);
    add_cnj(31,24,42,0);
    add_cnj(31,25,0,0);
    add_cnj(31,25,41,0);
    add_cnj(31,25,42,0);
    add_cnj(31,26,0,0);
    add_cnj(31,26,41,0);
    add_cnj(31,27,0,0);
    add_cnj(31,27,28,0);
    add_cnj(31,27,29,0);
    add_cnj(31,27,41,0);
    add_cnj(31,28,0,0);
    add_cnj(31,28,28,0);
    add_cnj(31,28,29,0);
    add_cnj(31,29,0,0);
    add_cnj(31,29,28,0);
    add_cnj(31,29,29,0);
    add_cnj(31,30,0,0);
    add_cnj(31,30,28,0);
    add_cnj(31,30,29,0);
    add_cnj(31,35,0,0);
    add_cnj(31,35,28,0);
    add_cnj(31,35,35,0);
    add_cnj(31,35,13,0);
    add_cnj(31,35,41,0);
    add_cnj(31,38,0,0);
    add_cnj(31,38,28,0);
    add_cnj(31,38,29,0);
    add_cnj(31,38,41,0);
    add_cnj(31,38,42,0);
    add_cnj(31,39,0,0);
    add_cnj(31,39,25,0);
    add_cnj(31,39,28,0);
    add_cnj(31,39,29,0);
    add_cnj(31,39,38,0);
    add_cnj(31,40,0,0);
    add_cnj(31,40,1,0);
    add_cnj(31,40,11,0);
    add_cnj(31,40,15,0);
    add_cnj(31,40,28,0);
    add_cnj(31,40,29,0);
    add_cnj(31,41,0,0);
    add_cnj(31,41,11,0);
    add_cnj(31,41,17,0);
    add_cnj(31,42,0,0);
    add_cnj(32,32,0,0);
    add_cnj(33,33,0,0);
    add_cnj(34,1,0,0);
    add_cnj(34,2,0,0);
    add_cnj(34,3,0,0);
    add_cnj(34,4,0,0);
    add_cnj(34,5,0,0);
    add_cnj(34,6,0,0);
    add_cnj(34,7,0,0);
    add_cnj(34,8,0,0);
    add_cnj(34,10,0,0);
    add_cnj(34,11,0,0);
    add_cnj(34,12,0,0);
    add_cnj(34,13,0,0);
    add_cnj(34,15,0,0);
    add_cnj(34,16,0,0);
    add_cnj(34,17,0,0);
    add_cnj(34,18,0,0);
    add_cnj(34,19,0,0);
    add_cnj(34,20,0,0);
    add_cnj(34,23,0,0);
    add_cnj(34,25,0,0);
    add_cnj(34,26,0,0);
    add_cnj(34,27,0,0);
    add_cnj(34,28,0,0);
    add_cnj(34,29,0,0);
    add_cnj(34,30,0,0);
    add_cnj(34,34,0,0);
    add_cnj(34,34,28,0);
    add_cnj(34,34,29,0);
    add_cnj(34,35,0,0);
    add_cnj(34,36,0,0);
    add_cnj(34,38,0,0);
    add_cnj(34,39,0,0);
    add_cnj(34,40,0,0);
    add_cnj(34,41,0,0);
    add_cnj(34,42,0,0);
    add_cnj(35,1,0,0);
    add_cnj(35,1,41,0);
    add_cnj(35,3,0,0);
    add_cnj(35,6,0,0);
    add_cnj(35,7,0,0);
    add_cnj(35,8,0,0);
    add_cnj(35,9,0,0);
    add_cnj(35,11,0,0);
    add_cnj(35,11,30,0);
    add_cnj(35,12,0,0);
    add_cnj(35,13,0,0);
    add_cnj(35,16,0,0);
    add_cnj(35,17,0,0);
    add_cnj(35,18,0,0);
    add_cnj(35,18,38,0);
    add_cnj(35,18,25,0);
    add_cnj(35,20,0,0);
    add_cnj(35,23,0,0);
    add_cnj(35,23,28,0);
    add_cnj(35,23,29,0);
    add_cnj(35,24,0,0);
    add_cnj(35,25,0,0);
    add_cnj(35,26,0,0);
    add_cnj(35,26,28,0);
    add_cnj(35,26,29,0);
    add_cnj(35,26,30,0);
    add_cnj(35,27,0,0);
    add_cnj(35,27,28,0);
    add_cnj(35,27,29,0);
    add_cnj(35,28,0,0);
    add_cnj(35,29,0,0);
    add_cnj(35,34,0,0);
    add_cnj(35,35,0,0);
    add_cnj(35,35,28,0);
    add_cnj(35,35,29,0);
    add_cnj(35,38,0,0);
    add_cnj(35,41,0,0);
    add_cnj(35,42,0,0);
    add_cnj(35,42,28,0);
    add_cnj(35,42,29,0);
    add_cnj(36,1,0,0);
    add_cnj(36,2,0,0);
    add_cnj(36,3,0,0);
    add_cnj(36,4,0,0);
    add_cnj(36,5,0,0);
    add_cnj(36,6,0,0);
    add_cnj(36,7,0,0);
    add_cnj(36,8,0,0);
    add_cnj(36,11,0,0);
    add_cnj(36,12,0,0);
    add_cnj(36,13,0,0);
    add_cnj(36,15,0,0);
    add_cnj(36,16,0,0);
    add_cnj(36,17,0,0);
    add_cnj(36,18,0,0);
    add_cnj(36,19,0,0);
    add_cnj(36,20,0,0);
    add_cnj(36,23,0,0);
    add_cnj(36,25,0,0);
    add_cnj(36,26,0,0);
    add_cnj(36,27,0,0);
    add_cnj(36,28,0,0);
    add_cnj(36,29,0,0);
    add_cnj(36,34,0,0);
    add_cnj(36,35,0,0);
    add_cnj(36,36,0,0);
    add_cnj(36,36,28,0);
    add_cnj(36,36,29,0);
    add_cnj(36,38,0,0);
    add_cnj(36,39,0,0);
    add_cnj(36,40,0,0);
    add_cnj(36,41,0,0);
    add_cnj(36,42,0,0);
    add_cnj(37,1,0,0);
    add_cnj(37,2,0,0);
    add_cnj(37,3,0,0);
    add_cnj(37,4,0,0);
    add_cnj(37,5,0,0);
    add_cnj(37,6,0,0);
    add_cnj(37,7,0,0);
    add_cnj(37,8,0,0);
    add_cnj(37,10,0,0);
    add_cnj(37,11,0,0);
    add_cnj(37,12,0,0);
    add_cnj(37,13,0,0);
    add_cnj(37,15,0,0);
    add_cnj(37,16,0,0);
    add_cnj(37,16,16,0);
    add_cnj(37,18,0,0);
    add_cnj(37,19,0,0);
    add_cnj(37,20,0,0);
    add_cnj(37,23,0,0);
    add_cnj(37,25,0,0);
    add_cnj(37,26,0,0);
    add_cnj(37,27,0,0);
    add_cnj(37,28,0,0);
    add_cnj(37,29,0,0);
    add_cnj(37,37,0,0);
    add_cnj(37,38,0,0);
    add_cnj(37,39,0,0);
    add_cnj(37,40,0,0);
    add_cnj(37,41,0,0);
    add_cnj(37,42,0,0);
    add_cnj(38,1,0,0);
    add_cnj(38,16,0,0);
    add_cnj(38,20,0,0);
    add_cnj(38,28,0,0);
    add_cnj(38,29,0,0);
    add_cnj(38,30,0,0);
    add_cnj(38,34,0,0);
    add_cnj(38,35,0,0);
    add_cnj(38,36,0,0);
    add_cnj(38,38,0,0);
    add_cnj(38,38,28,0);
    add_cnj(38,38,29,0);
    add_cnj(38,42,0,0);
    add_cnj(39,1,0,0);
    add_cnj(39,1,25,0);
    add_cnj(39,1,28,0);
    add_cnj(39,1,29,0);
    add_cnj(39,1,30,0);
    add_cnj(39,1,38,0);
    add_cnj(39,2,0,0);
    add_cnj(39,6,0,0);
    add_cnj(39,6,28,0);
    add_cnj(39,6,29,0);
    add_cnj(39,7,0,0);
    add_cnj(39,10,0,0);
    add_cnj(39,16,0,0);
    add_cnj(39,20,0,0);
    add_cnj(39,25,0,0);
    add_cnj(39,25,28,0);
    add_cnj(39,25,29,0);
    add_cnj(39,27,0,0);
    add_cnj(39,28,0,0);
    add_cnj(39,29,0,0);
    add_cnj(39,30,0,0);
    add_cnj(39,30,28,0);
    add_cnj(39,30,29,0);
    add_cnj(39,34,0,0);
    add_cnj(39,34,28,0);
    add_cnj(39,34,29,0);
    add_cnj(39,35,0,0);
    add_cnj(39,38,0,0);
    add_cnj(39,38,28,0);
    add_cnj(39,38,29,0);
    add_cnj(39,36,0,0);
    add_cnj(39,39,0,0);
    add_cnj(39,39,23,0);
    add_cnj(39,39,25,0);
    add_cnj(39,39,28,0);
    add_cnj(39,39,29,0);
    add_cnj(40,1,0,0);
    add_cnj(40,1,25,0);
    add_cnj(40,1,28,0);
    add_cnj(40,1,29,0);
    add_cnj(40,1,30,0);
    add_cnj(40,1,38,0);
    add_cnj(40,11,0,0);
    add_cnj(40,11,28,0);
    add_cnj(40,11,29,0);
    add_cnj(40,11,30,0);
    add_cnj(40,11,38,0);
    add_cnj(40,12,0,0);
    add_cnj(40,12,28,0);
    add_cnj(40,12,29,0);
    add_cnj(40,12,30,0);
    add_cnj(40,12,38,0);
    add_cnj(40,15,0,0);
    add_cnj(40,15,28,0);
    add_cnj(40,15,29,0);
    add_cnj(40,20,0,0);
    add_cnj(40,23,0,0);
    add_cnj(40,23,30,0);
    add_cnj(40,24,0,0);
    add_cnj(40,25,0,0);
    add_cnj(40,25,28,0);
    add_cnj(40,25,29,0);
    add_cnj(40,27,0,0);
    add_cnj(40,27,28,0);
    add_cnj(40,27,29,0);
    add_cnj(40,28,0,0);
    add_cnj(40,29,0,0);
    add_cnj(40,30,0,0);
    add_cnj(40,34,0,0);
    add_cnj(40,34,28,0);
    add_cnj(40,34,29,0);
    add_cnj(40,36,0,0);
    add_cnj(40,38,0,0);
    add_cnj(40,38,28,0);
    add_cnj(40,38,29,0);
    add_cnj(40,40,0,0);
    add_cnj(40,40,28,0);
    add_cnj(40,40,29,0);
    add_cnj(41,1,0,0);
    add_cnj(41,1,25,0);
    add_cnj(41,1,28,0);
    add_cnj(41,1,29,0);
    add_cnj(41,1,30,0);
    add_cnj(41,1,34,0);
    add_cnj(41,1,38,0);
    add_cnj(41,2,0,0);
    add_cnj(41,8,0,0);
    add_cnj(41,11,0,0);
    add_cnj(41,11,28,0);
    add_cnj(41,11,29,0);
    add_cnj(41,11,30,0);
    add_cnj(41,12,0,0);
    add_cnj(41,16,0,0);
    add_cnj(41,16,28,0);
    add_cnj(41,16,29,0);
    add_cnj(41,16,30,0);
    add_cnj(41,16,38,0);
    add_cnj(41,17,0,0);
    add_cnj(41,17,28,0);
    add_cnj(41,17,29,0);
    add_cnj(41,20,0,0);
    add_cnj(41,20,28,0);
    add_cnj(41,20,29,0);
    add_cnj(41,23,0,0);
    add_cnj(41,23,30,0);
    add_cnj(41,24,0,0);
    add_cnj(41,25,0,0);
    add_cnj(41,25,28,0);
    add_cnj(41,25,29,0);
    add_cnj(41,27,0,0);
    add_cnj(41,28,0,0);
    add_cnj(41,29,0,0);
    add_cnj(41,30,0,0);
    add_cnj(41,34,0,0);
    add_cnj(41,34,28,0);
    add_cnj(41,34,29,0);
    add_cnj(41,35,0,0);
    add_cnj(41,36,0,0);
    add_cnj(41,38,0,0);
    add_cnj(41,41,0,0);
    add_cnj(41,41,17,0);
    add_cnj(41,41,28,0);
    add_cnj(41,41,29,0);
    add_cnj(41,41,38,0);
    add_cnj(42,1,0,0);
    add_cnj(42,15,0,0);
    add_cnj(42,16,0,0);
    add_cnj(42,20,0,0);
    add_cnj(42,20,28,0);
    add_cnj(42,20,29,0);
    add_cnj(42,25,0,0);
    add_cnj(42,27,0,0);
    add_cnj(42,28,0,0);
    add_cnj(42,29,0,0);
    add_cnj(42,30,0,0);
    add_cnj(42,34,0,0);
    add_cnj(42,35,0,0);
    add_cnj(42,36,0,0);
    add_cnj(42,38,0,0);
    add_cnj(42,42,0,0);
    add_cnj(42,42,28,0);
    add_cnj(42,42,29,0);
    add_cnj(43,28,0,0);
    add_cnj(43,29,0,0);
    add_cnj(44,28,0,0);
    add_cnj(44,29,0,0);
    add_cnj(45,45,0,0);
    add_cnj(45,15,0,0);
    add_cnj(45,16,0,0);
    add_cnj(45,20,0,0);
    add_cnj(45,23,0,0);
    add_cnj(45,27,0,0);
    add_cnj(45,28,0,0);
    add_cnj(45,29,0,0);
    add_cnj(45,34,0,0);
    add_cnj(45,35,0,0);
    add_cnj(45,38,0,0);
    add_cnj(46,28,0,0);
    add_cnj(46,29,0,0);
    add_cnj(47,47,0,0);
    add_cnj(48,48,0,0);
    add_cnj(49,49,0,0);
    add_cnj(50,50,0,0);
    add_cnj(51,51,0,0);
    add_cnj(52,52,0,0);
    add_cnj(53,1,0,0);
    add_cnj(53,6,0,0);
    add_cnj(53,11,0,0);
    add_cnj(53,16,0,0);
    add_cnj(53,23,0,0);
    add_cnj(53,28,0,0);
    add_cnj(53,29,0,0);
    add_cnj(53,30,0,0);
    add_cnj(53,35,0,0);
    add_cnj(53,38,0,0);
    add_cnj(53,39,0,0);
    add_cnj(53,40,0,0);
    add_cnj(53,41,0,0);
    add_cnj(53,42,0,0);
    add_cnj(54,54,0,0);
}

/* Conjunct lookup */
static syl_t get_conjunct_code(int consonant, int c2, int c3, int c4) {
    unsigned int value = c2 | (c3<<8) | (c4<<16);
    cnj_entry_t *e = &conjunct_table[consonant];
    int i;
    for(i=1; i<=e->count; i++) {
        if(e->conjuncts[i] == value) return SYLLABLE(consonant, i, 0);
    }
    return SYL_INVALID;
}

/* Check if syllable is full, decompose conjuncts */
static int is_syl_full(syl_t l, byte_t *c1, byte_t *c2, byte_t *c3, byte_t *c4) {
    byte_t c = CONSONANT(l);
    byte_t cnj = CONJUNCT(l);
    *c1 = c;
    cnj_entry_t *e = &conjunct_table[c];
    unsigned int value = e->conjuncts[cnj];
    *c2 = value & 0xFF;
    *c3 = (value & 0x0000FF00) >> 8;
    *c4 = (value & 0x00FF0000) >> 16;
    return (*c2 && *c3 && *c4) ? 1 : 0;
}

/* Unicode range tables */
typedef struct { uint32_t base; uint32_t end; imli_script_t script; } unicode_range_t;
static const unicode_range_t unicode_ranges[] = {
    {0x0900,0x097F,SCRIPT_HINDI}, {0x0980,0x09FF,SCRIPT_BANGLA},
    {0x0A00,0x0A7F,SCRIPT_PUNJABI}, {0x0A80,0x0AFF,SCRIPT_GUJARATI},
    {0x0B00,0x0B7F,SCRIPT_ORIYA}, {0x0B80,0x0BFF,SCRIPT_TAMIL},
    {0x0C00,0x0C7F,SCRIPT_TELUGU}, {0x0C80,0x0CFF,SCRIPT_KANNADA},
    {0x0D00,0x0D7F,SCRIPT_MALAYALAM},
};
#define NUM_URANGES (sizeof(unicode_ranges)/sizeof(unicode_ranges[0]))

static imli_script_t script_from_codepoint(uint32_t cp) {
    int i; for(i=0;i<(int)NUM_URANGES;i++) if(cp>=unicode_ranges[i].base && cp<=unicode_ranges[i].end) return unicode_ranges[i].script;
    if(cp<0x80) return SCRIPT_ASCII;
    return SCRIPT_UNSUPPORTED;
}
static uint32_t unicode_base_for_script(imli_script_t s) {
    int i; for(i=0;i<(int)NUM_URANGES;i++) if(unicode_ranges[i].script==s) return unicode_ranges[i].base;
    return 0;
}
static byte_t iscii_code_from_script(imli_script_t s) { 
    if(s==SCRIPT_ASCII) return 0x41;
    return (s>=0&&s<=12)?lang_switch_codes[s]:0xFF; 
}
static imli_script_t script_from_iscii_code(byte_t c) {
    if(c==0x41) return SCRIPT_ASCII;
    int i; for(i=0;i<(int)(sizeof(lang_switch_codes)/sizeof(lang_switch_codes[0]));i++) if(c==lang_switch_codes[i]) return (imli_script_t)i;
    return SCRIPT_UNSUPPORTED;
}

/* ============================================================
 * UTF-8 decode/encode
 * ============================================================ */
static int decode_utf8(const byte_t *in, int len, uint32_t *out, int *out_len) {
    int i=0, n=0;
    while(i<len) {
        uint32_t cp; byte_t b=in[i];
        if(b<=0x7F) { cp=b; i+=1; }
        else if((b&0xE0)==0xC0) { cp=(b&0x1F)<<6; if(i+1>=len) break; cp|=in[i+1]&0x3F; i+=2; }
        else if((b&0xF0)==0xE0) { cp=(b&0x0F)<<12; if(i+2>=len) break; cp|=(in[i+1]&0x3F)<<6; cp|=in[i+2]&0x3F; i+=3; }
        else if((b&0xF8)==0xF0) { cp=(b&0x07)<<18; if(i+3>=len) break; cp|=(in[i+1]&0x3F)<<12; cp|=(in[i+2]&0x3F)<<6; cp|=in[i+3]&0x3F; i+=4; }
        else { i++; continue; }
        out[n++]=cp;
    }
    *out_len=n; return 0;
}

static int encode_cp_to_utf8(uint32_t cp, unsigned char *out) {
    if(cp<=0x7F){out[0]=(unsigned char)cp;return 1;}
    else if(cp<=0x7FF){out[0]=(unsigned char)(0xC0|(cp>>6));out[1]=(unsigned char)(0x80|(cp&0x3F));return 2;}
    else if(cp<=0xFFFF){out[0]=(unsigned char)(0xE0|(cp>>12));out[1]=(unsigned char)(0x80|((cp>>6)&0x3F));out[2]=(unsigned char)(0x80|(cp&0x3F));return 3;}
    else if(cp<=0x10FFFF){out[0]=(unsigned char)(0xF0|(cp>>18));out[1]=(unsigned char)(0x80|((cp>>12)&0x3F));out[2]=(unsigned char)(0x80|((cp>>6)&0x3F));out[3]=(unsigned char)(0x80|(cp&0x3F));return 4;}
    return 0;
}

/* ============================================================
 * Unicode <-> ISCII conversion (BIS IS 13194:1991 standard)
 * ISCII uses SAME byte layout for all scripts; Unicode blocks differ.
 * We use explicit lookup tables for correct mapping.
 * ============================================================ */

/* ISCII byte -> Unicode offset within script block.
   Index: iscii_byte - 0xA0.  Value: unicode offset from script base.
   0xFFFF means no mapping. */
static const uint16_t iscii_to_unicode_offset[] = {
    /* 0xA0 */ 0xFFFF,
    /* 0xA1 chandrabindu */ 0x01,
    /* 0xA2 anusvara */     0x02,
    /* 0xA3 visarga */      0x03,
    /* 0xA4 a */            0x05,
    /* 0xA5 aa */           0x06,
    /* 0xA6 i */            0x07,
    /* 0xA7 ii */           0x08,
    /* 0xA8 u */            0x09,
    /* 0xA9 uu */           0x0A,
    /* 0xAA vocalic r */    0x0B,
    /* 0xAB short e */      0x0E,
    /* 0xAC e */            0x0F,
    /* 0xAD ai */           0x10,
    /* 0xAE candra o */     0x11,
    /* 0xAF short o */      0x12,
    /* 0xB0 o */            0x13,
    /* 0xB1 au */           0x14,
    /* 0xB2 vocalic rr */   0x60,
    /* 0xB3 ka */           0x15,
    /* 0xB4 kha */          0x16,
    /* 0xB5 ga */           0x17,
    /* 0xB6 gha */          0x18,
    /* 0xB7 nga */          0x19,
    /* 0xB8 cha */          0x1A,
    /* 0xB9 chha */         0x1B,
    /* 0xBA ja */           0x1C,
    /* 0xBB jha */          0x1D,
    /* 0xBC nya */          0x1E,
    /* 0xBD Ta */            0x1F,
    /* 0xBE Tha */           0x20,
    /* 0xBF Da */            0x21,
    /* 0xC0 Dha */           0x22,
    /* 0xC1 Na */            0x23,
    /* 0xC2 ta */            0x24,
    /* 0xC3 tha */           0x25,
    /* 0xC4 da */            0x26,
    /* 0xC5 dha */           0x27,
    /* 0xC6 na */            0x28,
    /* 0xC7 nnna */          0x29,
    /* 0xC8 pa */            0x2A,
    /* 0xC9 pha */           0x2B,
    /* 0xCA ba */            0x2C,
    /* 0xCB bha */           0x2D,
    /* 0xCC ma */            0x2E,
    /* 0xCD ya */            0x2F,
    /* 0xCE yya */           0x5F,
    /* 0xCF ra */            0x30,
    /* 0xD0 rra */           0x31,
    /* 0xD1 la */            0x32,
    /* 0xD2 lla */           0x33,
    /* 0xD3 llla */          0x34,
    /* 0xD4 va */            0x35,
    /* 0xD5 sha */           0x36,
    /* 0xD6 Sha */           0x37,
    /* 0xD7 sa */            0x38,
    /* 0xD8 ha */            0x39,
    /* 0xD9 */ 0xFFFF,
    /* 0xDA aa matra */      0x3E,
    /* 0xDB i matra */       0x3F,
    /* 0xDC ii matra */      0x40,
    /* 0xDD u matra */       0x41,
    /* 0xDE uu matra */      0x42,
    /* 0xDF r matra */       0x43,
    /* 0xE0 short e matra */ 0x46,
    /* 0xE1 e matra */       0x47,
    /* 0xE2 ai matra */      0x48,
    /* 0xE3 short o matra */ 0x4A,
    /* 0xE4 o matra */       0x4B,
    /* 0xE5 au matra */      0x4C,
    /* 0xE6 danda */          0x64,
    /* 0xE7 double danda */   0x65,
    /* 0xE8 halant */        0x4D,
    /* 0xE9 nukta */         0x3C,
    /* 0xEA avagraha */      0x3D,
};
#define ISCII_TABLE_SIZE (sizeof(iscii_to_unicode_offset)/sizeof(iscii_to_unicode_offset[0]))

/* Reverse table: unicode offset -> ISCII byte. Built at init time. */
static byte_t unicode_offset_to_iscii[0x80];

static void build_unicode_iscii_tables(void) {
    int i;
    memset(unicode_offset_to_iscii, 0, sizeof(unicode_offset_to_iscii));
    for(i=0; i<(int)ISCII_TABLE_SIZE; i++) {
        uint16_t off = iscii_to_unicode_offset[i];
        if(off != 0xFFFF && off < 0x80) {
            unicode_offset_to_iscii[off] = (byte_t)(0xA0 + i);
        }
    }
    /* Danda and double-danda are shared across scripts at U+0964/U+0965
       but in our per-script offset scheme they only exist in Devanagari block.
       For other scripts, we handle them in unicode_to_iscii directly. */
}

static int unicode_to_iscii(const uint32_t *cps, int ncp, byte_t *out, int *out_len) {
    int i, n=0;
    imli_script_t cur = SCRIPT_UNSUPPORTED;
    for(i=0;i<ncp;i++) {
        uint32_t cp = cps[i];
        if(cp < 0x80) { 
            if(cur != SCRIPT_ASCII && cur != SCRIPT_UNSUPPORTED) { out[n++]=0xEF; out[n++]=0x41; cur=SCRIPT_ASCII; }
            out[n++] = (byte_t)cp; continue; 
        }
        /* Danda and double-danda are shared across all scripts (in Devanagari block) */
        if(cp == 0x0964 || cp == 0x0965) {
            if(cur == SCRIPT_ASCII || cur == SCRIPT_UNSUPPORTED) {
                out[n++]=0xEF; out[n++]=iscii_code_from_script(SCRIPT_HINDI); cur=SCRIPT_HINDI;
            }
            out[n++] = (cp == 0x0964) ? 0xE6 : 0xE7;
            continue;
        }
        imli_script_t s = script_from_codepoint(cp);
        if(s == SCRIPT_UNSUPPORTED) {
            if(cur != SCRIPT_ASCII) { out[n++]=0xEF; out[n++]=0x41; cur=SCRIPT_ASCII; }
            int nb = encode_cp_to_utf8(cp, out+n);
            n += nb;
            continue;
        }
        uint32_t base = unicode_base_for_script(s);
        uint32_t offset = cp - base;
        if(offset < 0x80 && unicode_offset_to_iscii[offset]) {
            if(s != cur) { out[n++]=0xEF; out[n++]=iscii_code_from_script(s); cur=s; }
            out[n++] = unicode_offset_to_iscii[offset];
        } else {
            if(cur != SCRIPT_ASCII) { out[n++]=0xEF; out[n++]=0x41; cur=SCRIPT_ASCII; }
            int nb = encode_cp_to_utf8(cp, out+n);
            n += nb;
        }
    }
    *out_len = n; return 0;
}

static int iscii_to_unicode(const byte_t *in, int len, uint32_t *out, int *out_len) {
    int i, n=0;
    imli_script_t cur = SCRIPT_HINDI;
    for(i=0;i<len;i++) {
        byte_t b = in[i];
        if(b == 0xEF && i+1<len) { imli_script_t s=script_from_iscii_code(in[i+1]); if(s!=SCRIPT_UNSUPPORTED) cur=s; i++; continue; }
        if(b < 0x80) { out[n++] = b; continue; }
        if(cur == SCRIPT_ASCII && b >= 0x80) {
            uint32_t cp = 0; int nb = 0, j, valid = 1;
            if((b & 0xE0) == 0xC0) { cp = b & 0x1F; nb = 1; }
            else if((b & 0xF0) == 0xE0) { cp = b & 0x0F; nb = 2; }
            else if((b & 0xF8) == 0xF0) { cp = b & 0x07; nb = 3; }
            else valid = 0;
            if(valid) {
                for(j=1; j<=nb; j++) {
                    if(i+j < len && (in[i+j] & 0xC0) == 0x80) cp = (cp << 6) | (in[i+j] & 0x3F);
                    else { valid = 0; break; }
                }
            }
            if(valid) { out[n++] = cp; i += nb; }
            else out[n++] = b;
            continue;
        }
        /* Danda and double-danda always map to U+0964/U+0965 */
        if(b == 0xE6) { out[n++] = 0x0964; continue; }
        if(b == 0xE7) { out[n++] = 0x0965; continue; }
        if(b >= 0xA0) {
            int idx = b - 0xA0;
            if(idx < (int)ISCII_TABLE_SIZE && iscii_to_unicode_offset[idx] != 0xFFFF) {
                uint32_t base = unicode_base_for_script(cur);
                out[n++] = base + iscii_to_unicode_offset[idx];
            }
        }
    }
    *out_len = n; return 0;
}

/* ============================================================
 * Syllable composition (from im.c)
 * ============================================================ */
static imli_script_t active_script = SCRIPT_UNSUPPORTED;

static syl_t syl_compose(byte_t input, imli_char_type_t type,
    imli_script_t script_prev, syl_t prev_syl, imli_compose_flag_t flag, int *del_prev) {
    byte_t c,v,cons1,cons2,cons3,cons4;
    syl_t syl, temp;
    *del_prev = 0;
    syl = SYL_INVALID;

    switch(type) {
    case TYPE_INVALID: break;
    case TYPE_SPECIAL: syl = (SPECIAL_START | input); break;
    case TYPE_VOWEL:
        syl = (input & VOWEL_MASK);
        if(prev_syl==SYL_INVALID) return syl;
        if(active_script != script_prev) return syl;
        if(IS_SYL_ASCII(prev_syl) || IS_SYL_SPECIAL(prev_syl)) return syl;
        c = CONSONANT(prev_syl); v = VOWEL(prev_syl);
        if(flag == FLAG_STANDALONE_VOWEL) {
            if(v==HALANT_V && c==0) *del_prev=1;
            return syl;
        }
        if(v==HALANT_V && input==HALANT_V && c!=0) return syl;
        if(c && v==0) { syl = prev_syl | input; *del_prev=1; }
        return syl;
    case TYPE_CONSONANT:
        syl = SYLLABLE(input, 0, 0);
        if(active_script != script_prev) return syl;
        if(prev_syl==SYL_INVALID || IS_SYL_ASCII(prev_syl) || IS_SYL_SPECIAL(prev_syl)) return syl;
        v = VOWEL(prev_syl); c = CONSONANT(prev_syl);
        if(flag==FLAG_INSERT_HALANT && v==0) v=HALANT_V;
        if(input==NUKHTA_C && v==0) v=HALANT_V;
        if(v != HALANT_V) return syl;
        if(c==0) { *del_prev=1; return syl; }
        is_syl_full(prev_syl, &cons1, &cons2, &cons3, &cons4);
        if(cons1 && cons2 && cons3 && cons4) return syl;
        if(cons2==0) {
            *del_prev=1;
            if(c==KA_C && input==SHA_C) return SYLLABLE(KSHA_C,0,0);
            if(c==JA_C && input==NJA_C) return SYLLABLE(JNYA_C,0,0);
            if(c==NA_C) {
                temp=get_conjunct_code(c,input,0,0);
                if(temp!=SYL_INVALID) return temp;
                c=NEX_C;
                temp=get_conjunct_code(c,input,0,0);
                if(temp!=SYL_INVALID) return temp;
                *del_prev=0; return syl;
            }
            if(c==RA_C) {
                temp=get_conjunct_code(c,input,0,0);
                if(temp!=SYL_INVALID) return temp;
                temp=get_conjunct_code(REX_C,input,0,0);
                if(temp!=SYL_INVALID) return temp;
                temp=get_conjunct_code(RXL_C,input,0,0);
                if(temp!=SYL_INVALID) return temp;
                c=RXXL_C;
            }
            temp=get_conjunct_code(c,input,0,0);
            if(temp!=SYL_INVALID) return temp;
            *del_prev=0; return syl;
        }
        if(cons3==0) {
            temp=get_conjunct_code(c,cons2,input,0);
            if(temp!=SYL_INVALID){*del_prev=1;return temp;}
            *del_prev=0; return syl;
        }
        if(cons4==0) {
            temp=get_conjunct_code(c,cons2,cons3,input);
            if(temp!=SYL_INVALID){*del_prev=1;return temp;}
        }
        *del_prev=0; return syl;
    default: break;
    }
    return syl;
}

/* ============================================================
 * ISCII -> Acharya syllables (construct_syllables, from aci.cpp)
 * ============================================================ */
static int construct_syllables(const byte_t *iscii, int len, syl_t *syls, int *nsyls,
                               imli_script_t *scripts, int *nscripts) {
    int i, ns=0, nsc=0;
    syl_t prev_syl = SYL_INVALID;
    imli_script_t prev_script = SCRIPT_HINDI;
    
    /* Peek at first byte to determine initial script */
    imli_script_t cur_script = SCRIPT_HINDI;
    if (len > 0) {
        if (iscii[0] < 128 || isspace(iscii[0])) cur_script = SCRIPT_ASCII;
        else if (iscii[0] == 0xEF && len > 1) {
            imli_script_t sc = script_from_iscii_code(iscii[1]);
            if (sc != SCRIPT_UNSUPPORTED) cur_script = sc;
        }
    }
    active_script = cur_script;
    
    /* Inject initial script token for Acharya byte stream */
    syls[ns++] = SWITCH_CODE | (byte_t)cur_script;
    scripts[nsc++] = cur_script;

    for(i=0;i<len;i++) {
        byte_t b = iscii[i];

        /* Whitespace and punctuation */
        if(isspace(b)||b==';'||b=='!'||b=='\''||b=='/'||b==','||b=='?'||b=='.'||b=='-') {
            if(b!=13) syls[ns++] = ASCII_START + b;
            prev_syl = SYL_INVALID;
            continue;
        }

        /* ASCII and UTF-8 Passthrough in SCRIPT_ASCII mode */
        if(b < 128 || (cur_script == SCRIPT_ASCII && b != 0xEF)) {
            syls[ns++] = ASCII_START + b;
            prev_syl = SYL_INVALID;
            continue;
        }

        /* Marathi short-a */
        if(i+1<len && cur_script==SCRIPT_MARATHI && b==0xAE && iscii[i+1]==0xE9) {
            syls[ns++]=SYLLABLE(63,0,15); prev_syl=SYL_INVALID; i++; continue;
        }

        /* Specials */
        if(cur_script!=SCRIPT_ASCII && (b==0x2C||b==0x2D||b==0xA1||b==0xAE||b==0xEA)) {
            byte_t iv=iscii_imli_map[b].value;
            imli_char_type_t it=iscii_imli_map[b].type;
            if(it!=TYPE_INVALID) syls[ns++]=SYLLABLE(63,0,iv);
            prev_syl=SYL_INVALID; continue;
        }

        /* Numbers in Indic script */
        if(b>='0'&&b<='9'&&cur_script!=SCRIPT_ASCII) {
            syls[ns++]=SYLLABLE(63,3,b); prev_syl=SYL_INVALID; continue;
        }

        /* Local language numbers */
        if(b>=0xF1&&b<=0xFA&&cur_script!=SCRIPT_ASCII) {
            syls[ns++]=SYLLABLE(63,3,b-0xF1); prev_syl=SYL_INVALID; continue;
        }

        /* Danda (0xE6) and double-danda (0xE7) - store as special markers */
        /* Danda (0xE6) and double-danda (0xE7) - store as special markers */
        if(b==0xE6 || b==0xE7) {
            syls[ns++] = SPECIAL_START | b;
            prev_syl = SYL_INVALID;
            continue;
        }

        /* Language switch */
        if(b==0xEF) {
            if(i+1<len) {
                i++;
                imli_script_t sc=script_from_iscii_code(iscii[i]);
                if(sc!=SCRIPT_UNSUPPORTED && sc!=cur_script && sc!=SCRIPT_SANSKRIT) {
                    /* Insert switch syl */
                    syls[ns++] = SWITCH_CODE | (byte_t)sc;
                    scripts[nsc++] = sc;
                    prev_script=cur_script; cur_script=sc;
                    active_script=cur_script; prev_syl=SYL_INVALID;
                }
            }
            continue;
        }

        /* Promoted consonants and nukhta */
        {
            imli_compose_flag_t fl=FLAG_NONE;
            imli_char_type_t tp=TYPE_INVALID;
            byte_t imli_val=0;

            if(i+1<len && iscii[i+1]==0xE9) {
                tp=TYPE_CONSONANT; fl=FLAG_NONE; i++;
                if(b==0xBF) imli_val=DDRA_C;
                else if(b==0xC0) imli_val=DDRHA_C;
                else { i--; tp=TYPE_INVALID; }
            }
            if(tp==TYPE_INVALID && i+2<len) {
                tp=TYPE_CONSONANT; i+=2;
                if(b==0xB3&&iscii[i-1]==0xE8&&iscii[i]==0xD6) imli_val=KSHA_C;
                else if(b==0xBA&&iscii[i-1]==0xE8&&iscii[i]==0xBC) imli_val=JNYA_C;
                else { i-=2; tp=TYPE_INVALID; }
            }
            if(tp!=TYPE_INVALID) {
                int dp=0;
                syl_t ns2=syl_compose(imli_val,tp,cur_script,prev_syl,fl,&dp);
                if(ns2!=SYL_INVALID) {
                    if(dp && ns>0) syls[ns-1]=ns2; else syls[ns++]=ns2;
                    prev_syl=ns2;
                }
                continue;
            }
        }

        /* Regular ISCII byte */
        {
            byte_t imli_val=0;
            imli_char_type_t tp=TYPE_INVALID;
            imli_compose_flag_t fl=FLAG_NONE;

            if(b==0xE9) { tp=TYPE_CONSONANT; imli_val=NUKHTA_C; fl=FLAG_NONE; }
            else {
                imli_val=iscii_imli_map[b].value;
                tp=iscii_imli_map[b].type;
                fl=iscii_imli_map[b].flags;
            }

            int dp=0;
            syl_t ns2=syl_compose(imli_val,tp,cur_script,prev_syl,fl,&dp);
            if(ns2!=SYL_INVALID) {
                if(dp && ns>0) syls[ns-1]=ns2; else syls[ns++]=ns2;
                prev_syl=ns2;
            }
        }
    }
    *nsyls=ns; *nscripts=nsc; return 0;
}

/* ============================================================
 * Acharya -> ISCII (expand_syllables, from iscii.c:imli_to_iscii)
 * ============================================================ */
static void emit_consonant_iscii(byte_t C, byte_t **pp) {
    byte_t *p = *pp;
    if(C==KSHA_C){*p++=0xB3;*p++=0xE8;*p++=0xD6;}
    else if(C==JNYA_C){*p++=0xBA;*p++=0xE8;*p++=0xBC;}
    else if(C==DDRA_C){*p++=0xBF;*p++=0xE9;}
    else if(C==DDRHA_C){*p++=0xC0;*p++=0xE9;}
    else if(C==NEX_C){*p++=0xC6;}
    else if(C==REX_C||C==RXL_C||C==RXXL_C){*p++=0xCF;}
    else {*p++=imli_to_iscii_con[C];}
    *pp = p;
}

static int expand_syllables(const syl_t *syls, int nsyls, byte_t *out, int *out_len) {
    byte_t *p = out;
    byte_t cur_lang = 0xFF;
    int i;
    for(i=0;i<nsyls;i++) {
        syl_t lch = syls[i];
        if(lch==SYL_TERMINATOR) break;
        byte_t C=CONSONANT(lch), V=VOWEL(lch), Cj=CONJUNCT(lch);

        if(IS_SYL_ASCII(lch)){*p++=(byte_t)(lch&0xFF);continue;}
        if(IS_SYL_SPECIAL(lch)){
            byte_t low = lch & 0xFF;
            /* Danda/double-danda stored as SPECIAL_START | byte */
            if(low == 0xE6 || low == 0xE7) { *p++ = low; }
            else if(Cj==0){
                if(V==15){if(cur_lang==SCRIPT_BANGLA||cur_lang==SCRIPT_KANNADA||cur_lang==SCRIPT_HINDI||cur_lang==SCRIPT_ASAMIYA||cur_lang==SCRIPT_MARATHI)*p++=0xAE;if(cur_lang==SCRIPT_MARATHI)*p++=0xE9;}
                else if(low < sizeof(imli_to_iscii_spl)/sizeof(imli_to_iscii_spl[0])) *p++=imli_to_iscii_spl[low];
            } else if(Cj==3) *p++='0'+V;
            continue;
        }
        if(IS_SYL_SWITCH(lch)){
            byte_t lang=lch&LANGCODE_MASK;
            if(lang!=cur_lang&&(lang<=12||lang==SCRIPT_ASCII)){*p++=0xEF;*p++=iscii_code_from_script(lang);cur_lang=lang;}
            continue;
        }

        if(C==0){*p++=imli_to_iscii_vow[V];continue;}
        emit_consonant_iscii(C, &p);

        if(Cj) {
            byte_t c1,cj1,cj2,cj3;
            is_syl_full(lch,&c1,&cj1,&cj2,&cj3);
            if(cj1==NUKHTA_C){*p++=iscii_nukhta;}
            else{*p++=iscii_halant;emit_consonant_iscii(cj1,&p);}
            if(cj2){
                if(cj2==NUKHTA_C)*p++=iscii_nukhta;
                else{*p++=iscii_halant;emit_consonant_iscii(cj2,&p);}
            }
            if(cj3){
                if(cj3==NUKHTA_C)*p++=iscii_nukhta;
                else{*p++=iscii_halant;emit_consonant_iscii(cj3,&p);}
            }
        }
        if(V>0&&V!=15)*p++=imli_to_iscii_vow_matras[V];
        else if(V==15){*p++=iscii_halant;}
    }
    *out_len=(int)(p-out); return 0;
}

/* ============================================================
 * Print helpers
 * ============================================================ */
static void print_hex_bytes(const byte_t *data, int len) {
    int i; for(i=0;i<len;i++) { if(i>0) printf(" "); printf("%02X",data[i]); }
}

static void print_codepoints(const uint32_t *cps, int n) {
    int i; for(i=0;i<n;i++) { if(i>0)printf(" "); printf("U+%04X",cps[i]); }
}

static void print_syllables(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        if(IS_SYL_ASCII(syls[i])) printf("{%02X}",(unsigned)(syls[i]&0xFF));
        else if(IS_SYL_SWITCH(syls[i])) printf("{SWITCH:%d}",(int)(syls[i]&0xFF));
        else printf("{%04X}",(unsigned)syls[i]);
    }
}

static void print_acharya_bytes(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        byte_t hi=(syls[i]>>8)&0xFF, lo=syls[i]&0xFF;
        printf("%02X%02X",hi,lo);
    }
}

/* ============================================================
 * Full pipeline
 * ============================================================ */
static int run_pipeline(const byte_t *utf8_in, int utf8_len) {
    /* Step 1: Print input UTF-8 */
    printf("INPUT (UTF-8):\n  "); print_hex_bytes(utf8_in, utf8_len); printf("\n\n");

    /* Step 2: Decode to Unicode code points */
    uint32_t *cps = (uint32_t*)malloc(utf8_len * sizeof(uint32_t));
    int ncp=0;
    decode_utf8(utf8_in, utf8_len, cps, &ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps, ncp); printf("\n\n");

    /* Step 3: Unicode -> ISCII */
    byte_t *iscii1 = (byte_t*)malloc(ncp * 4 + 256);
    int iscii1_len=0;
    unicode_to_iscii(cps, ncp, iscii1, &iscii1_len);
    printf("ISCII BYTE STREAM (with script switches injected):\n  ");
    print_hex_bytes(iscii1, iscii1_len); printf("\n\n");

    /* Step 4: ISCII -> Acharya syllables */
    syl_t *syls = (syl_t*)malloc((iscii1_len+256) * sizeof(syl_t));
    imli_script_t *scripts = (imli_script_t*)malloc((iscii1_len+256)*sizeof(imli_script_t));
    int nsyls=0, nscripts=0;
    construct_syllables(iscii1, iscii1_len, syls, &nsyls, scripts, &nscripts);
    printf("SYLLABLE STRUCTS (construct_syllables):\n  "); print_syllables(syls, nsyls); printf("\n\n");
    printf("ACHARYA 2-BYTE CODES (internal script tokens inserted):\n  ");
    print_acharya_bytes(syls, nsyls); printf("\n\n");

    /* Step 5: Acharya -> ISCII (decode/expand) */
    byte_t *iscii2 = (byte_t*)malloc(nsyls * 16 + 256);
    int iscii2_len=0;
    expand_syllables(syls, nsyls, iscii2, &iscii2_len);
    printf("EXPANDED ISCII (expand_syllables):\n  ");
    print_hex_bytes(iscii2, iscii2_len); printf("\n\n");

    /* Step 6: ISCII -> Unicode */
    uint32_t *cps2 = (uint32_t*)malloc(iscii2_len * sizeof(uint32_t));
    int ncp2=0;
    iscii_to_unicode(iscii2, iscii2_len, cps2, &ncp2);
    printf("BACK TO UNICODE:\n  "); print_codepoints(cps2, ncp2); printf("\n\n");

    /* Step 7: Unicode -> UTF-8 */
    byte_t *utf8_out = (byte_t*)malloc(ncp2 * 4 + 16);
    int utf8_out_len=0;
    int j; for(j=0;j<ncp2;j++) { int nb=encode_cp_to_utf8(cps2[j],utf8_out+utf8_out_len); utf8_out_len+=nb; }
    printf("OUTPUT (UTF-8):\n  "); print_hex_bytes(utf8_out, utf8_out_len); printf("\n\n");

    /* Step 8: Verify */
    int ok = (utf8_len==utf8_out_len && memcmp(utf8_in, utf8_out, utf8_len)==0);
    printf("VERIFICATION: %s\n", ok ? "\xe2\x9c\x94 SUCCESS" : "\xe2\x9c\x98 FAILURE");
    if(!ok) { printf("  Input  len=%d\n  Output len=%d\n", utf8_len, utf8_out_len); }

    printf("\nSummary: UTF-8 (%d bytes) -> Unicode (%d cps) -> ISCII (%d bytes) -> Acharya (%d bytes) -> ISCII (%d bytes) -> Unicode (%d cps) -> UTF-8 (%d bytes).\n", utf8_len, ncp, iscii1_len, nsyls * 2, iscii2_len, ncp2, utf8_out_len);

    free(cps); free(iscii1); free(syls); free(scripts); free(iscii2); free(cps2); free(utf8_out);
    return ok ? 0 : 1;
}

/* ============================================================
 * Conversion functions
 * ============================================================ */
static void func_utf8_to_acharya(const byte_t *utf8, int len) {
    printf("UTF-8 INPUT:\n  "); print_hex_bytes(utf8,len); printf("\n\n");
    uint32_t *cps=(uint32_t*)malloc(len*sizeof(uint32_t)); int ncp=0;
    decode_utf8(utf8,len,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(ncp*4+256); int ilen=0;
    unicode_to_iscii(cps,ncp,iscii,&ilen);
    printf("ISCII BYTE STREAM:\n  "); print_hex_bytes(iscii,ilen); printf("\n\n");
    syl_t *syls=(syl_t*)malloc((ilen+256)*sizeof(syl_t));
    imli_script_t *sc=(imli_script_t*)malloc((len+256)*sizeof(imli_script_t));
    int nsyls=0,nsc=0;
    construct_syllables(iscii,ilen,syls,&nsyls,sc,&nsc);
    printf("ACHARYA 2-BYTE:\n  "); print_acharya_bytes(syls,nsyls); printf("\n");
    printf("\nSummary: UTF-8 (%d bytes) -> Unicode (%d cps) -> ISCII (%d bytes) -> Acharya (%d bytes).\n", len, ncp, ilen, nsyls * 2);
    free(cps);free(iscii);free(syls);free(sc);
}

static void func_acharya_to_utf8(const syl_t *syls, int nsyls) {
    printf("ACHARYA INPUT:\n  "); print_acharya_bytes(syls,nsyls); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(nsyls*16+256); int ilen=0;
    expand_syllables(syls,nsyls,iscii,&ilen);
    printf("ISCII BYTE STREAM:\n  "); print_hex_bytes(iscii,ilen); printf("\n\n");
    uint32_t *cps=(uint32_t*)malloc(ilen*sizeof(uint32_t)); int ncp=0;
    iscii_to_unicode(iscii,ilen,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *utf8=(byte_t*)malloc(ncp*4+16); int ulen=0;
    int j; for(j=0;j<ncp;j++){int nb=encode_cp_to_utf8(cps[j],utf8+ulen);ulen+=nb;}
    printf("UTF-8 OUTPUT:\n  "); print_hex_bytes(utf8,ulen); printf("\n");
    printf("\nSummary: Acharya (%d bytes) -> ISCII (%d bytes) -> Unicode (%d cps) -> UTF-8 (%d bytes).\n", nsyls * 2, ilen, ncp, ulen);
    free(iscii);free(cps);free(utf8);
}

static void func_acharya_to_iscii(const syl_t *syls, int nsyls) {
    printf("ACHARYA INPUT:\n  "); print_acharya_bytes(syls,nsyls); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(nsyls*16+256); int ilen=0;
    expand_syllables(syls,nsyls,iscii,&ilen);
    printf("ISCII OUTPUT:\n  "); print_hex_bytes(iscii,ilen); printf("\n");
    printf("\nSummary: Acharya (%d bytes) -> ISCII (%d bytes).\n", nsyls * 2, ilen);
    free(iscii);
}

static void func_iscii_to_acharya(const byte_t *iscii, int len) {
    printf("ISCII INPUT:\n  "); print_hex_bytes(iscii,len); printf("\n\n");
    syl_t *syls=(syl_t*)malloc((len+256)*sizeof(syl_t));
    imli_script_t *sc=(imli_script_t*)malloc(256*sizeof(imli_script_t));
    int nsyls=0,nsc=0;
    construct_syllables(iscii,len,syls,&nsyls,sc,&nsc);
    printf("ACHARYA 2-BYTE OUTPUT:\n  "); print_acharya_bytes(syls,nsyls); printf("\n");
    printf("\nSummary: ISCII (%d bytes) -> Acharya (%d bytes).\n", len, nsyls * 2);
    free(syls);free(sc);
}

static void func_utf8_to_iscii(const byte_t *utf8, int len) {
    printf("UTF-8 INPUT:\n  "); print_hex_bytes(utf8,len); printf("\n\n");
    uint32_t *cps=(uint32_t*)malloc(len*sizeof(uint32_t)); int ncp=0;
    decode_utf8(utf8,len,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(ncp*4+256); int ilen=0;
    unicode_to_iscii(cps,ncp,iscii,&ilen);
    printf("ISCII OUTPUT:\n  "); print_hex_bytes(iscii,ilen); printf("\n");
    printf("\nSummary: UTF-8 (%d bytes) -> Unicode (%d cps) -> ISCII (%d bytes).\n", len, ncp, ilen);
    free(cps);free(iscii);
}

static void func_iscii_to_utf8(const byte_t *iscii, int len) {
    printf("ISCII INPUT:\n  "); print_hex_bytes(iscii,len); printf("\n\n");
    uint32_t *cps=(uint32_t*)malloc(len*sizeof(uint32_t)); int ncp=0;
    iscii_to_unicode(iscii,len,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *utf8=(byte_t*)malloc(ncp*4+16); int ulen=0;
    int j; for(j=0;j<ncp;j++){int nb=encode_cp_to_utf8(cps[j],utf8+ulen);ulen+=nb;}
    printf("UTF-8 OUTPUT:\n  "); print_hex_bytes(utf8,ulen); printf("\n");
    printf("\nSummary: ISCII (%d bytes) -> Unicode (%d cps) -> UTF-8 (%d bytes).\n", len, ncp, ulen);
    free(cps);free(utf8);
}

/* Parse hex string to bytes */
static int parse_hex_bytes(const char *s, byte_t *out) {
    int n=0; unsigned int v;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(sscanf(s,"%x",&v)==1) { out[n++]=(byte_t)v; while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++; }
        else break;
    }
    return n;
}

static int parse_hex_syls(const char *s, syl_t *out) {
    int n=0; unsigned int v;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(sscanf(s,"%x",&v)==1) { out[n++]=(syl_t)v; while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++; }
        else break;
    }
    return n;
}

static byte_t* read_file(const char *path, int *len) {
    FILE *f=fopen(path,"rb");
    if(!f){fprintf(stderr,"Cannot open %s\n",path);return NULL;}
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    /* Skip BOM if present */
    byte_t bom[3]; int skip=0;
    if(sz>=3){fread(bom,1,3,f);if(bom[0]==0xEF&&bom[1]==0xBB&&bom[2]==0xBF)skip=3;else fseek(f,0,SEEK_SET);}
    /* Strip trailing newline */
    byte_t *buf=(byte_t*)malloc(sz+1);
    int rd=(int)fread(buf,1,sz-skip,f); fclose(f);
    while(rd>0&&(buf[rd-1]=='\n'||buf[rd-1]=='\r')) rd--;
    *len=rd; return buf;
}

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s pipeline <file.txt>\n", prog);
    printf("  %s utf8_to_acharya <file.txt>\n", prog);
    printf("  %s acharya_to_utf8 <hex_syls>\n", prog);
    printf("    // Example - %s acharya_to_utf8 \"FA01 B400 41D2 7000\"\n", prog);
    printf("  %s acharya_to_iscii <hex_syls>\n", prog);
    printf("    // Example - %s acharya_to_iscii \"FA01 B400 41D2 7000\"\n", prog);
    printf("  %s iscii_to_acharya <hex_bytes>\n", prog);
    printf("    // Example - %s iscii_to_acharya \"EF 42 B3 E8 D6 C2 E8 CF DB CD\"\n", prog);
    printf("  %s utf8_to_iscii <file.txt>\n", prog);
    printf("  %s iscii_to_utf8 <hex_bytes>\n", prog);
    printf("    // Example - %s iscii_to_utf8 \"EF 42 B3 E8 D6 C2 E8 CF DB CD\"\n", prog);
}

int main(int argc, char **argv) {
    /* Initialize tables */
    setup_iscii_imli_table();
    build_unicode_iscii_tables();
    init_conjunct_tables();

    if(argc < 3) { print_usage(argv[0]); return 1; }

    const char *cmd = argv[1];
    const char *arg = argv[2];

    if(strcmp(cmd,"pipeline")==0) {
        int len; byte_t *data=read_file(arg,&len);
        if(!data) return 1;
        int ret=run_pipeline(data,len);
        free(data); return ret;
    }
    else if(strcmp(cmd,"utf8_to_acharya")==0) {
        int len; byte_t *data=read_file(arg,&len);
        if(!data) return 1;
        func_utf8_to_acharya(data,len); free(data);
    }
    else if(strcmp(cmd,"utf8_to_iscii")==0) {
        int len; byte_t *data=read_file(arg,&len);
        if(!data) return 1;
        func_utf8_to_iscii(data,len); free(data);
    }
    else if(strcmp(cmd,"acharya_to_utf8")==0) {
        syl_t syls[4096]; int n=parse_hex_syls(arg,syls);
        func_acharya_to_utf8(syls,n);
    }
    else if(strcmp(cmd,"acharya_to_iscii")==0) {
        syl_t syls[4096]; int n=parse_hex_syls(arg,syls);
        func_acharya_to_iscii(syls,n);
    }
    else if(strcmp(cmd,"iscii_to_acharya")==0) {
        byte_t bytes[4096]; int n=parse_hex_bytes(arg,bytes);
        func_iscii_to_acharya(bytes,n);
    }
    else if(strcmp(cmd,"iscii_to_utf8")==0) {
        byte_t bytes[4096]; int n=parse_hex_bytes(arg,bytes);
        func_iscii_to_utf8(bytes,n);
    }
    else { print_usage(argv[0]); return 1; }

    return 0;
}

