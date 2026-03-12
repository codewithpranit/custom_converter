#!/usr/bin/env python3
"""
Generate converter.c from generic.cnj and embedded templates.
Run: python3 build_converter.py > converter.c
"""
import re, sys, os

NAMES = {
    "nil":0,"ka":1,"kha":2,"ga":3,"gha":4,"nga":5,"cha":6,"chha":7,
    "ja":8,"jha":9,"nja":10,"ta":11,"tta":12,"dda":13,"ddha":14,"nna":15,
    "tha":16,"ttha":17,"da":18,"dha":19,"na":20,"nex":21,"nas":22,"pa":23,
    "pha":24,"ba":25,"bha":26,"ma":27,"ya":28,"yab":29,"ra":30,"rex":31,
    "rxl":32,"rxxl":33,"rra":34,"la":35,"lla":36,"zha":37,"va":38,"ca":39,
    "sha":40,"sa":41,"ha":42,"ddra":43,"ddrha":44,"ksha":45,"jnya":46,
    "qa":47,"xa":48,"gga":49,"za":50,"fa":51,"cca":52,"vedic":53,"nukhta":54
}

def parse_cnj(filepath):
    lines = open(filepath,'r').readlines()
    calls = []
    for line in lines:
        line = line.strip().replace('\r','')
        if not line or line.startswith('#'): continue
        m = re.match(r'(\w+)\s*:\s*(.*)', line)
        if not m: continue
        base_name = m.group(1).strip()
        if base_name not in NAMES: continue
        base = NAMES[base_name]
        rest = m.group(2).strip().rstrip(',')
        tokens = []
        i = 0
        while i < len(rest):
            while i < len(rest) and rest[i] in ' ,\t': i += 1
            if i >= len(rest): break
            if rest[i] == '(':
                i += 1; subs = []
                while i < len(rest) and rest[i] != ')':
                    while i < len(rest) and rest[i] in ' ,\t': i += 1
                    if i >= len(rest) or rest[i] == ')': break
                    w = ''
                    while i < len(rest) and rest[i] not in ' ,)\t': w += rest[i]; i += 1
                    if w: subs.append(w)
                if i < len(rest) and rest[i] == ')': i += 1
                tokens.append(('s', subs))
            else:
                w = ''
                while i < len(rest) and rest[i] not in ' ,(\t': w += rest[i]; i += 1
                if w: tokens.append(('n', w))
        idx = 0
        while idx < len(tokens):
            if tokens[idx][0] == 'n':
                c2n = tokens[idx][1]
                if c2n not in NAMES: idx += 1; continue
                c2 = NAMES[c2n]
                calls.append((base, c2, 0, 0))
                if idx+1 < len(tokens) and tokens[idx+1][0] == 's':
                    for sn in tokens[idx+1][1]:
                        if sn in NAMES: calls.append((base, c2, NAMES[sn], 0))
                    idx += 2
                else: idx += 1
            else: idx += 1
    return calls

# Find generic.cnj
script_dir = os.path.dirname(os.path.abspath(__file__))
repo_root = os.path.dirname(script_dir)
cnj_path = os.path.join(repo_root, "libimli2-branch-cnj4", "utils", "cnj", "generic.cnj")
cnj_calls = parse_cnj(cnj_path)

# Generate conjunct init function body
cnj_lines = []
for (b, c2, c3, c4) in cnj_calls:
    cnj_lines.append(f"    add_cnj({b},{c2},{c3},{c4});")
CNJ_INIT = "\n".join(cnj_lines)

print(r'''/*************************************************************
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
''' + CNJ_INIT + r'''
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
static byte_t iscii_code_from_script(imli_script_t s) { return (s>=0&&s<=12)?lang_switch_codes[s]:0xFF; }
static imli_script_t script_from_iscii_code(byte_t c) {
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
        if(cp < 0x80) { out[n++] = (byte_t)cp; continue; }
        /* Danda and double-danda are shared across all scripts (in Devanagari block) */
        if(cp == 0x0964) { out[n++] = 0xE6; continue; }
        if(cp == 0x0965) { out[n++] = 0xE7; continue; }
        imli_script_t s = script_from_codepoint(cp);
        if(s == SCRIPT_UNSUPPORTED) {
            /* Store non-Indic non-ASCII codepoints as escape: 0xFD + 4 bytes (big-endian cp) */
            out[n++] = 0xFD;
            out[n++] = (byte_t)((cp >> 24) & 0xFF);
            out[n++] = (byte_t)((cp >> 16) & 0xFF);
            out[n++] = (byte_t)((cp >> 8) & 0xFF);
            out[n++] = (byte_t)(cp & 0xFF);
            continue;
        }
        if(s != cur) { out[n++]=0xEF; out[n++]=iscii_code_from_script(s); cur=s; }
        uint32_t base = unicode_base_for_script(s);
        uint32_t offset = cp - base;
        if(offset < 0x80 && unicode_offset_to_iscii[offset]) {
            out[n++] = unicode_offset_to_iscii[offset];
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
        /* Non-indic escape: 0xFD + 4 bytes codepoint */
        if(b == 0xFD && i+4<len) {
            uint32_t cp2 = ((uint32_t)in[i+1]<<24)|((uint32_t)in[i+2]<<16)|((uint32_t)in[i+3]<<8)|in[i+4];
            out[n++] = cp2;
            i += 4;
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
    imli_script_t cur_script = SCRIPT_ASCII;
    active_script = cur_script;

    for(i=0;i<len;i++) {
        byte_t b = iscii[i];

        /* Fix: if byte >= 0xA0 and not lang switch, but cur is ASCII, revert */
        if(b>=0xA0 && b!=0xEF && cur_script==SCRIPT_ASCII) {
            cur_script = prev_script;
            active_script = cur_script;
            prev_syl = SYL_INVALID;
        }

        /* Whitespace and punctuation */
        if(isspace(b)||b==';'||b=='!'||b=='\''||b=='/'||b==','||b=='?'||b=='.'||b=='-') {
            if(b!=13) syls[ns++] = ASCII_START + b;
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

        /* ASCII */
        if(b<127) {
            if(cur_script!=SCRIPT_ASCII) {
                prev_script=cur_script; cur_script=SCRIPT_ASCII;
                active_script=cur_script; prev_syl=SYL_INVALID;
            }
            syls[ns++]=ASCII_START+b; continue;
        }

        /* Local language numbers */
        if(b>=0xF1&&b<=0xFA&&cur_script!=SCRIPT_ASCII) {
            syls[ns++]=SYLLABLE(63,3,b-0xF1); prev_syl=SYL_INVALID; continue;
        }

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
            if(lang!=cur_lang&&lang<=12){*p++=0xEF;*p++=lang_switch_codes[lang];cur_lang=lang;}
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
    imli_script_t *scripts = (imli_script_t*)malloc(256*sizeof(imli_script_t));
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

    free(cps); free(iscii1); free(syls); free(scripts); free(iscii2); free(cps2); free(utf8_out);
    return ok ? 0 : 1;
}

/* ============================================================
 * Conversion functions
 * ============================================================ */
static void func_utf8_to_acharya(const byte_t *utf8, int len) {
    uint32_t *cps=(uint32_t*)malloc(len*sizeof(uint32_t)); int ncp=0;
    decode_utf8(utf8,len,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(ncp*4+256); int ilen=0;
    unicode_to_iscii(cps,ncp,iscii,&ilen);
    printf("ISCII BYTE STREAM:\n  "); print_hex_bytes(iscii,ilen); printf("\n\n");
    syl_t *syls=(syl_t*)malloc((ilen+256)*sizeof(syl_t));
    imli_script_t *sc=(imli_script_t*)malloc(256*sizeof(imli_script_t));
    int nsyls=0,nsc=0;
    construct_syllables(iscii,ilen,syls,&nsyls,sc,&nsc);
    printf("ACHARYA 2-BYTE:\n  "); print_acharya_bytes(syls,nsyls); printf("\n");
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
    free(iscii);free(cps);free(utf8);
}

static void func_acharya_to_iscii(const syl_t *syls, int nsyls) {
    printf("ACHARYA INPUT:\n  "); print_acharya_bytes(syls,nsyls); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(nsyls*16+256); int ilen=0;
    expand_syllables(syls,nsyls,iscii,&ilen);
    printf("ISCII OUTPUT:\n  "); print_hex_bytes(iscii,ilen); printf("\n");
    free(iscii);
}

static void func_iscii_to_acharya(const byte_t *iscii, int len) {
    printf("ISCII INPUT:\n  "); print_hex_bytes(iscii,len); printf("\n\n");
    syl_t *syls=(syl_t*)malloc((len+256)*sizeof(syl_t));
    imli_script_t *sc=(imli_script_t*)malloc(256*sizeof(imli_script_t));
    int nsyls=0,nsc=0;
    construct_syllables(iscii,len,syls,&nsyls,sc,&nsc);
    printf("ACHARYA 2-BYTE OUTPUT:\n  "); print_acharya_bytes(syls,nsyls); printf("\n");
    free(syls);free(sc);
}

static void func_utf8_to_iscii(const byte_t *utf8, int len) {
    uint32_t *cps=(uint32_t*)malloc(len*sizeof(uint32_t)); int ncp=0;
    decode_utf8(utf8,len,cps,&ncp);
    printf("UNICODE CODE POINTS:\n  "); print_codepoints(cps,ncp); printf("\n\n");
    byte_t *iscii=(byte_t*)malloc(ncp*4+256); int ilen=0;
    unicode_to_iscii(cps,ncp,iscii,&ilen);
    printf("ISCII OUTPUT:\n  "); print_hex_bytes(iscii,ilen); printf("\n");
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
    printf("  %s acharya_to_iscii <hex_syls>\n", prog);
    printf("  %s iscii_to_acharya <hex_bytes>\n", prog);
    printf("  %s utf8_to_iscii <file.txt>\n", prog);
    printf("  %s iscii_to_utf8 <hex_bytes>\n", prog);
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
''')
