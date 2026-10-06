// tests/stub_palette.c —— 外壳调色板(main/kit_art.c)里本游戏用到的字符,颜色照抄;spr_decode 和外壳一样。
#include "kit_art.h"

#include <stddef.h>
#include <string.h>

bool spr_palette(char c, uint32_t *argb) {
    static const struct { char c; uint32_t argb; } P[] = {
        { 'k', 0xFF2B1B17 }, { 'w', 0xFFFFFFFF }, { 'g', 0xFF4A4A5A }, { 'r', 0xFFD83A3A },
        { 'y', 0xFFF2C230 }, { 'o', 0xFFE8902A }, { 'x', 0xFF262630 },
    };
    if (c == '.') {
        *argb = 0;
        return true;
    }
    for (size_t i = 0; i < sizeof(P) / sizeof(P[0]); i++) {
        if (P[i].c == c) {
            *argb = P[i].argb;
            return true;
        }
    }
    return false;
}

bool spr_decode(const spr_art_t *art, uint8_t *out) {
    for (unsigned y = 0; y < art->h; y++) {
        if (strlen(art->rows[y]) != art->w) return false;
        for (unsigned x = 0; x < art->w; x++) {
            uint32_t argb;
            if (!spr_palette(art->rows[y][x], &argb)) return false;
            uint8_t *p = out + ((size_t)y * art->w + x) * 4u;
            p[0] = (uint8_t)argb;
            p[1] = (uint8_t)(argb >> 8);
            p[2] = (uint8_t)(argb >> 16);
            p[3] = (uint8_t)(argb >> 24);
        }
    }
    return true;
}
