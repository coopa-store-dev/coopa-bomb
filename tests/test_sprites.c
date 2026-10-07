// tests/test_sprites.c —— 像素画:每张都能用外壳的调色板解码;四张热血头像按 bomb_fighter_t 排。
#include "bomb_fighters.h"
#include "bomb_sprites.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t buf[16 * 16 * 4];
    for (int i = 0; i < BS_COUNT; i++) {
        const spr_art_t *a = bomb_art((bs_spr_t)i);
        assert(a && a->w <= 16 && a->h <= 16);
        assert(spr_decode(a, buf));
    }
    assert(!bomb_art(BS_COUNT));
    assert(BS_HOT_DAD == BS_HOT_KID + BF_DAD && BS_HOT_GRANDPA == BS_HOT_KID + BF_GRANDPA);
    assert(bomb_art(BS_CAGE)->w == 16 && bomb_art(BS_CROWN)->w == 8 && BOMB_SPR_IMAGES == BS_COUNT + 1);
    puts("test_sprites: ok");
    return 0;
}
