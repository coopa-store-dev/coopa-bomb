// src/bomb_fighters.c —— 见 bomb_fighters.h。
#include "bomb_fighters.h"

static const uint8_t SKILLS[BF_COUNT][2] = {
    [BF_KID] = { SK_SLING, SK_DECOY },
    [BF_DAD] = { SK_ROCKET, SK_CAGE },
    [BF_GRANNY] = { SK_NAG, SK_LID },
    [BF_GRANDPA] = { SK_GLASSES, SK_TAIJI },
};

uint8_t bomb_skill_of(uint8_t fighter, bool super) { return fighter < BF_COUNT ? SKILLS[fighter][super] : SK_NONE; }

bool bomb_skill_super(uint8_t sk) { return sk == SK_DECOY || sk == SK_CAGE || sk == SK_LID || sk == SK_TAIJI; }

static bool player_skill(uint8_t sk) { return sk >= SK_SLING && sk <= SK_TAIJI; }

uint8_t bomb_skill_cost(uint8_t sk) { return player_skill(sk) ? (bomb_skill_super(sk) ? 4 : 2) : 0; }

bool bomb_skill_hits(uint8_t sk) {
    return sk == SK_SLING || sk == SK_DECOY || sk == SK_ROCKET || sk == SK_CAGE || sk == SK_NAG;
}

uint32_t bomb_skill_freeze(uint8_t sk) {
    if (player_skill(sk)) return bomb_skill_super(sk) ? 1200 : 600;
    return sk == SK_LID_BOUNCE || sk == SK_TAIJI_PUSH ? 400 : 0;
}

// 扫过 2*half 个单位要 ≥ ms 毫秒:half ≥ ms*speed/2000(向上取整)
static uint16_t at_least(uint16_t half, uint32_t ms, uint32_t speed_x1000) {
    uint32_t min = (ms * speed_x1000 + 1999u) / 2000u;
    return half < min ? (uint16_t)min : half;
}

bomb_aim_t bomb_aim_params(uint8_t heat, bool nag, bool glasses) {
    uint32_t period = BOMB_AIM_COLD - (uint32_t)(BOMB_AIM_COLD - BOMB_AIM_HOT) * heat / 255u;
    uint32_t speed = 2000u * 1000u / period;
    uint32_t green = BOMB_GREEN_HALF, perfect = BOMB_PERFECT_HALF;
    if (glasses) {
        speed /= 2;
        green = green * 3 / 2;
        perfect = perfect * 3 / 2;
    }
    if (nag) {
        speed = speed * 3 / 2;
        green = green * 7 / 10;
        perfect = perfect * 7 / 10;
    }
    bomb_aim_t a = { (uint16_t)speed, (uint16_t)green, (uint16_t)perfect };
    a.green_half = at_least(a.green_half, BOMB_GREEN_MIN_MS, speed);
    a.perfect_half = at_least(a.perfect_half, BOMB_PERFECT_MIN_MS, speed);
    return a;
}
