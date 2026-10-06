// src/bomb_sprites.h —— 库巴传炸弹的像素画:炸弹、火花、爆炸(新画的);孩子的脸借外壳的 SPR_KID。
#pragma once

#include "kit_art.h"

typedef enum { BS_BOMB = 0, BS_SPARK, BS_BOOM, BS_COUNT } bs_spr_t;

#define BOMB_SPR_IMAGES 4  // 一局最多解码几张(三张自己的 + SPR_KID),给 sprite_budget 用

const spr_art_t *bomb_art(bs_spr_t id);  // 越界返回 NULL
