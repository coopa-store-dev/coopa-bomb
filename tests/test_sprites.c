// tests/test_sprites.c —— 像素画:每张都能用外壳的调色板解码,左上角透明。
#include "bomb_sprites.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t buf[16 * 16 * 4];
    for (int i = 0; i < BS_COUNT; i++) {
        const spr_art_t *a = bomb_art((bs_spr_t)i);
        assert(a && a->w <= 16 && a->h <= 16);
        assert(spr_decode(a, buf));
        assert(buf[3] == 0);
    }
    assert(!bomb_art(BS_COUNT));
    puts("test_sprites: ok");
    return 0;
}
