// src/bomb_sprites.h —— 库巴传炸弹的像素画(都是新画的):炸弹、火花、爆炸、四个人的热血头像、铁笼、太极、电火花、皇冠。
// 锅盖借外壳的 SPR_POTLID。
#pragma once

#include "kit_art.h"

typedef enum {
    BS_BOMB = 0, BS_SPARK, BS_BOOM,
    BS_HOT_KID, BS_HOT_DAD, BS_HOT_GRANNY, BS_HOT_GRANDPA,  // 顺序同 bomb_fighter_t
    BS_CAGE, BS_TAIJI, BS_ZAP, BS_CROWN,
    BS_COUNT
} bs_spr_t;

#define BOMB_SPR_IMAGES (BS_COUNT + 1)  // 一局最多解码几张(自己的全部 + SPR_POTLID),给 sprite_budget 用

const spr_art_t *bomb_art(bs_spr_t id);  // 越界返回 NULL
