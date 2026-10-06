// src/bomb_sprites.c —— 见 bomb_sprites.h。原创像素画,只用外壳调色板里的颜色。
#include "bomb_sprites.h"

#include <stddef.h>

static const char *const BOMB[] = {
    "...........yo...",
    "..........oy....",
    ".........kk.....",
    "........kgk.....",
    "......kkkkkk....",
    "....kkxxxxxxkk..",
    "...kxxxxxxxxxxk.",
    "..kxxwwxxxxxxxxk",
    "..kxwwxxxxxxxxxk",
    ".kxxwxxxxxxxxxxk",
    ".kxxxxxxxxxxxxxk",
    ".kxxxxxxxxxxxxxk",
    "..kxxxxxxxxxxxk.",
    "..kxxxxxxxxxxxk.",
    "...kxxxxxxxxxk..",
    "....kkkkkkkkk...",
};

static const char *const SPARK[] = {
    "...y....",
    ".y.y.y..",
    "..yoy...",
    "yyorryy.",
    "..yoy...",
    ".y.y.y..",
    "...y....",
    "........",
};

static const char *const BOOM[] = {
    ".......y........",
    "...y...yy...y...",
    "....y.yooy.y....",
    ".....yoooooy....",
    "..yyyoorrooyyy..",
    "...yoorrrrooy...",
    "..yoorrwwrrooy..",
    "yyoorrwwwwrrooyy",
    "..yoorrwwrrooy..",
    "...yoorrrrooy...",
    "..yyyoorrooyyy..",
    ".....yoooooy....",
    "....y.yooy.y....",
    "...y...yy...y...",
    ".......y........",
    "................",
};

#define ART(a, w) { w, (uint8_t)(sizeof(a) / sizeof(a[0])), a }

static const spr_art_t ARTS[BS_COUNT] = { ART(BOMB, 16), ART(SPARK, 8), ART(BOOM, 16) };

const spr_art_t *bomb_art(bs_spr_t id) { return (unsigned)id < BS_COUNT ? &ARTS[id] : NULL; }
