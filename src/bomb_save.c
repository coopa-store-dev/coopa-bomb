// src/bomb_save.c —— 见 bomb_save.h。
#include "bomb_save.h"

#include <string.h>

_Static_assert(sizeof(bomb_save_t) <= 2048, "save blob is at most 2 KB");

void bomb_save_reset(bomb_save_t *s) {
    memset(s, 0, sizeof(*s));
    s->magic = BOMB_MAGIC;
    s->ver = BOMB_SAVE_VER;
}

bool bomb_save_valid(const void *blob) {
    const bomb_save_t *s = blob;
    return s->magic == BOMB_MAGIC && s->ver == BOMB_SAVE_VER && s->wins <= s->games;
}

bool bomb_save_round(bomb_save_t *s) {
    if (s->rounds == 0xFFFFFFFFu) return false;
    s->rounds++;
    return true;
}

bool bomb_save_over(bomb_save_t *s, uint32_t match, bool won) {
    if (!match || s->counted == match || s->games == 0xFFFFu) return false;
    s->counted = match;
    s->games++;
    if (won) s->wins++;
    return true;
}

uint8_t bomb_save_milestones(const bomb_save_t *s) {
    return (uint8_t)((s->rounds >= 1 ? 1u : 0u) | (s->wins >= 1 ? 2u : 0u) | (s->wins >= 10 ? 4u : 0u));
}

#define FIGHTERS 4  // = BF_COUNT(存档不依赖规则头文件)

uint8_t bomb_save_pick(const bomb_save_t *s) { return s->pad[0] < FIGHTERS ? s->pad[0] : 0; }

void bomb_save_set_pick(bomb_save_t *s, uint8_t fighter) { s->pad[0] = fighter < FIGHTERS ? fighter : 0; }

bool bomb_save_crown(const bomb_save_t *s, uint8_t fighter) { return fighter < FIGHTERS && (s->pad[1] >> fighter & 1); }

bool bomb_save_set_crown(bomb_save_t *s, uint8_t fighter) {
    if (fighter >= FIGHTERS || bomb_save_crown(s, fighter)) return false;
    s->pad[1] = (uint8_t)(s->pad[1] | 1u << fighter);
    return true;
}
