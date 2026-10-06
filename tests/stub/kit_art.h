// tests/stub/kit_art.h —— 宿主测试用的 kit_art.h 替身:只有 bomb_sprites.c 用到的类型和调色板函数。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t w;
    uint8_t h;
    const char *const *rows;
} spr_art_t;

bool spr_palette(char c, uint32_t *argb);
bool spr_decode(const spr_art_t *art, uint8_t *out);
