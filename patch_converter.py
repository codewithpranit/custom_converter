import sys

with open('build_converter.py', 'r') as f:
    code = f.read()

# 1. Update iscii_code_from_script and script_from_iscii_code
code = code.replace(
    "static byte_t iscii_code_from_script(imli_script_t s) { return (s>=0&&s<=12)?lang_switch_codes[s]:0xFF; }\nstatic imli_script_t script_from_iscii_code(byte_t c) {\n    int i; for(i=0;i<(int)(sizeof(lang_switch_codes)/sizeof(lang_switch_codes[0]));i++) if(c==lang_switch_codes[i]) return (imli_script_t)i;\n    return SCRIPT_UNSUPPORTED;\n}",
    """static byte_t iscii_code_from_script(imli_script_t s) { 
    if(s==SCRIPT_ASCII) return 0x41;
    return (s>=0&&s<=12)?lang_switch_codes[s]:0xFF; 
}
static imli_script_t script_from_iscii_code(byte_t c) {
    if(c==0x41) return SCRIPT_ASCII;
    int i; for(i=0;i<(int)(sizeof(lang_switch_codes)/sizeof(lang_switch_codes[0]));i++) if(c==lang_switch_codes[i]) return (imli_script_t)i;
    return SCRIPT_UNSUPPORTED;
}"""
)

# 2. Update unicode_to_iscii
old_u2i = """        if(s == SCRIPT_UNSUPPORTED) {
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
        }"""
new_u2i = """        if(s == SCRIPT_UNSUPPORTED) {
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
        }"""
code = code.replace(old_u2i, new_u2i)

# 3. Update iscii_to_unicode
old_i2u = """        if(b == 0xEF && i+1<len) { imli_script_t s=script_from_iscii_code(in[i+1]); if(s!=SCRIPT_UNSUPPORTED) cur=s; i++; continue; }
        if(b < 0x80) { out[n++] = b; continue; }
        /* Non-indic escape: 0xFD + 4 bytes codepoint */
        if(b == 0xFD && i+4<len) {
            uint32_t cp2 = ((uint32_t)in[i+1]<<24)|((uint32_t)in[i+2]<<16)|((uint32_t)in[i+3]<<8)|in[i+4];
            out[n++] = cp2;
            i += 4;
            continue;
        }"""
new_i2u = """        if(b == 0xEF && i+1<len) { imli_script_t s=script_from_iscii_code(in[i+1]); if(s!=SCRIPT_UNSUPPORTED) cur=s; i++; continue; }
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
        }"""
code = code.replace(old_i2u, new_i2u)

# 4. Remove 0xFD from construct_syllables and add initial switch code
old_cs = """    int i, ns=0, nsc=0;
    syl_t prev_syl = SYL_INVALID;
    imli_script_t prev_script = SCRIPT_HINDI;
    imli_script_t cur_script = SCRIPT_HINDI;
    active_script = cur_script;

    for(i=0;i<len;i++) {"""
new_cs = """    int i, ns=0, nsc=0;
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

    for(i=0;i<len;i++) {"""
code = code.replace(old_cs, new_cs)

old_cs_fd = """        /* Non-indic Escape (0xFD) + 4 bytes codepoint */
        if(b==0xFD && i+4<len) {
            syls[ns++] = SPECIAL_START | b;
            syls[ns++] = (iscii[i+1] << 8) | iscii[i+2];
            syls[ns++] = (iscii[i+3] << 8) | iscii[i+4];
            i += 4;
            prev_syl = SYL_INVALID;
            continue;
        }\n\n"""
code = code.replace(old_cs_fd, "")

# 5. Remove 0xFD from expand_syllables
old_es = """            /* Non-indic Escape */
            else if(low == 0xFD && i+2<nsyls) {
                syl_t s1 = syls[++i], s2 = syls[++i];
                *p++ = 0xFD;
                *p++ = (s1 >> 8) & 0xFF;
                *p++ = s1 & 0xFF;
                *p++ = (s2 >> 8) & 0xFF;
                *p++ = s2 & 0xFF;
            }\n"""
code = code.replace(old_es, "")

# 6. Update print_usage to show updated examples
old_usage = """static void print_usage(const char *prog) {
    printf("Usage:\\n");
    printf("  %s pipeline <file.txt>\\n", prog);
    printf("  %s utf8_to_acharya <file.txt>\\n", prog);
    printf("  %s acharya_to_utf8 <hex_syls>\\n", prog);
    printf("    // Example - %s acharya_to_utf8 \\"FA01 B400 41D2 7000\\"\\n", prog);
    printf("  %s acharya_to_iscii <hex_syls>\\n", prog);
    printf("    // Example - %s acharya_to_iscii \\"FA01 B400 41D2 7000\\"\\n", prog);
    printf("  %s iscii_to_acharya <hex_bytes>\\n", prog);
    printf("    // Example - %s iscii_to_acharya \\"EF 42 B3 E8 D6 C2 E8 CF DB CD\\"\\n", prog);
    printf("  %s utf8_to_iscii <file.txt>\\n", prog);
    printf("  %s iscii_to_utf8 <hex_bytes>\\n", prog);
    printf("    // Example - %s iscii_to_utf8 \\"EF 42 B3 E8 D6 C2 E8 CF DB CD\\"\\n", prog);
}"""
new_usage = """static void print_usage(const char *prog) {
    printf("Usage:\\n");
    printf("  %s pipeline <file.txt>\\n", prog);
    printf("  %s utf8_to_acharya <file.txt>\\n", prog);
    printf("  %s acharya_to_utf8 <hex_syls>\\n", prog);
    printf("    // Example - %s acharya_to_utf8 \\"0xFA 0x01 0xB4 0x00 0x41 0xD2 0x70 0x00\\"\\n", prog);
    printf("  %s acharya_to_iscii <hex_syls>\\n", prog);
    printf("    // Example - %s acharya_to_iscii \\"0xFA 0x01 0xB4 0x00 0x41 0xD2 0x70 0x00\\"\\n", prog);
    printf("  %s iscii_to_acharya <hex_bytes>\\n", prog);
    printf("    // Example - %s iscii_to_acharya \\"0xEF 0x42 0xB3 0xE8 0xD6 0xC2 0xE8 0xCF 0xDB 0xCD\\"\\n", prog);
    printf("  %s utf8_to_iscii <file.txt>\\n", prog);
    printf("  %s iscii_to_utf8 <hex_bytes>\\n", prog);
    printf("    // Example - %s iscii_to_utf8 \\"0xEF 0x42 0xB3 0xE8 0xD6 0xC2 0xE8 0xCF 0xDB 0xCD\\"\\n", prog);
}"""
code = code.replace(old_usage, new_usage)

# 7. Update func summaries
code = code.replace(
    'printf("ACHARYA 2-BYTE:\\n  "); print_acharya_bytes(syls,nsyls); printf("\\n");\n    free(cps);free(iscii);free(syls);free(sc);',
    'printf("ACHARYA 2-BYTE:\\n  "); print_acharya_bytes(syls,nsyls); printf("\\n");\n    printf("\\nSummary: UTF-8 (%d bytes) -> Unicode (%d cps) -> ISCII (%d bytes) -> Acharya (%d syls).\\n", len, ncp, ilen, nsyls);\n    free(cps);free(iscii);free(syls);free(sc);'
)

code = code.replace(
    'printf("UTF-8 OUTPUT:\\n  "); print_hex_bytes(utf8,ulen); printf("\\n");\n    free(iscii);free(cps);free(utf8);',
    'printf("UTF-8 OUTPUT:\\n  "); print_hex_bytes(utf8,ulen); printf("\\n");\n    printf("\\nSummary: Acharya (%d syls) -> ISCII (%d bytes) -> Unicode (%d cps) -> UTF-8 (%d bytes).\\n", nsyls, ilen, ncp, ulen);\n    free(iscii);free(cps);free(utf8);'
)

code = code.replace(
    'printf("ISCII OUTPUT:\\n  "); print_hex_bytes(iscii,ilen); printf("\\n");\n    free(iscii);',
    'printf("ISCII OUTPUT:\\n  "); print_hex_bytes(iscii,ilen); printf("\\n");\n    printf("\\nSummary: Acharya (%d syls) -> ISCII (%d bytes).\\n", nsyls, ilen);\n    free(iscii);'
)

code = code.replace(
    'printf("ACHARYA 2-BYTE OUTPUT:\\n  "); print_acharya_bytes(syls,nsyls); printf("\\n");\n    free(syls);free(sc);',
    'printf("ACHARYA 2-BYTE OUTPUT:\\n  "); print_acharya_bytes(syls,nsyls); printf("\\n");\n    printf("\\nSummary: ISCII (%d bytes) -> Acharya (%d syls).\\n", len, nsyls);\n    free(syls);free(sc);'
)

code = code.replace(
    'printf("ISCII OUTPUT:\\n  "); print_hex_bytes(iscii,ilen); printf("\\n");\n    free(cps);free(iscii);',
    'printf("ISCII OUTPUT:\\n  "); print_hex_bytes(iscii,ilen); printf("\\n");\n    printf("\\nSummary: UTF-8 (%d bytes) -> Unicode (%d cps) -> ISCII (%d bytes).\\n", len, ncp, ilen);\n    free(cps);free(iscii);'
)

# 8. Add UTF-8 INPUT header to utf8 related funcs
if 'printf("UTF-8 INPUT:\\n  ");' not in code:
    code = code.replace(
        'static void func_utf8_to_acharya(const byte_t *utf8, int len) {',
        'static void func_utf8_to_acharya(const byte_t *utf8, int len) {\n    printf("UTF-8 INPUT:\\n  "); print_hex_bytes(utf8,len); printf("\\n\\n");'
    )

    code = code.replace(
        'static void func_utf8_to_iscii(const byte_t *utf8, int len) {',
        'static void func_utf8_to_iscii(const byte_t *utf8, int len) {\n    printf("UTF-8 INPUT:\\n  "); print_hex_bytes(utf8,len); printf("\\n\\n");'
    )

# 9. Update printing and parsing for 0x format
old_print_hex = """static void print_hex_bytes(const byte_t *data, int len) {
    int i; for(i=0;i<len;i++) { if(i>0) printf(" "); printf("%02X",data[i]); }
}"""
new_print_hex = """static void print_hex_bytes(const byte_t *data, int len) {
    int i; for(i=0;i<len;i++) { if(i>0) printf(" "); printf("0x%02X",data[i]); }
}"""
code = code.replace(old_print_hex, new_print_hex)

old_print_syls = """static void print_syllables(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        if(IS_SYL_ASCII(syls[i])) printf("{%02X}",(unsigned)(syls[i]&0xFF));
        else if(IS_SYL_SWITCH(syls[i])) printf("{SWITCH:%d}",(int)(syls[i]&0xFF));
        else printf("{%04X}",(unsigned)syls[i]);
    }
}"""
new_print_syls = """static void print_syllables(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        if(IS_SYL_ASCII(syls[i])) printf("{0x%02X}",(unsigned)(syls[i]&0xFF));
        else if(IS_SYL_SWITCH(syls[i])) printf("{SWITCH:%d}",(int)(syls[i]&0xFF));
        else printf("{0x%04X}",(unsigned)syls[i]);
    }
}"""
code = code.replace(old_print_syls, new_print_syls)

old_print_acharya = """static void print_acharya_bytes(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        byte_t hi=(syls[i]>>8)&0xFF, lo=syls[i]&0xFF;
        printf("%02X%02X",hi,lo);
    }
}"""
new_print_acharya = """static void print_acharya_bytes(const syl_t *syls, int n) {
    int i; for(i=0;i<n;i++) {
        if(i>0) printf(" ");
        byte_t hi=(syls[i]>>8)&0xFF, lo=syls[i]&0xFF;
        printf("0x%02X 0x%02X",hi,lo);
    }
}"""
code = code.replace(old_print_acharya, new_print_acharya)

old_parse_hex = """static int parse_hex_bytes(const char *s, byte_t *out) {
    int n=0; unsigned int v;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(sscanf(s,"%x",&v)==1) { out[n++]=(byte_t)v; while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++; }
        else break;
    }
    return n;
}"""
new_parse_hex = """static int parse_hex_bytes(const char *s, byte_t *out) {
    int n=0; unsigned int v;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(strncmp(s, "0x", 2) == 0) s += 2;
        if(sscanf(s,"%2x",&v)==1) { out[n++]=(byte_t)v; while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++; }
        else break;
    }
    return n;
}"""
code = code.replace(old_parse_hex, new_parse_hex)

old_parse_syls = """static int parse_hex_syls(const char *s, syl_t *out) {
    int n=0; unsigned int v;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(sscanf(s,"%x",&v)==1) { out[n++]=(syl_t)v; while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++; }
        else break;
    }
    return n;
}"""
new_parse_syls = """static int parse_hex_syls(const char *s, syl_t *out) {
    int n=0; unsigned int v1, v2;
    while(*s) {
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(strncmp(s, "0x", 2) == 0) s += 2;
        if(sscanf(s, "%2x", &v1) != 1) break;
        while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++;
        while(*s&&(isspace(*s)||*s==','||*s=='|')) s++;
        if(!*s) break;
        if(strncmp(s, "0x", 2) == 0) s += 2;
        if(sscanf(s, "%2x", &v2) != 1) break;
        out[n++] = (syl_t)((v1 << 8) | v2);
        while(*s&&!isspace(*s)&&*s!=','&&*s!='|') s++;
    }
    return n;
}"""
code = code.replace(old_parse_syls, new_parse_syls)

with open('build_converter.py', 'w') as f:
    f.write(code)
