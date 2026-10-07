// tests/test_fighters.c —— 角色和招式表、瞄准参数(任何叠加下窗口都按得中)。
#include "bomb_fighters.h"

#include <assert.h>
#include <stdio.h>

static void test_skills(void) {
    assert(bomb_skill_of(BF_KID, false) == SK_SLING && bomb_skill_of(BF_KID, true) == SK_DECOY);
    assert(bomb_skill_of(BF_DAD, false) == SK_ROCKET && bomb_skill_of(BF_DAD, true) == SK_CAGE);
    assert(bomb_skill_of(BF_GRANNY, false) == SK_NAG && bomb_skill_of(BF_GRANNY, true) == SK_LID);
    assert(bomb_skill_of(BF_GRANDPA, false) == SK_GLASSES && bomb_skill_of(BF_GRANDPA, true) == SK_TAIJI);
    assert(bomb_skill_of(BF_COUNT, false) == SK_NONE && bomb_skill_of(200, true) == SK_NONE);
    for (uint8_t f = 0; f < BF_COUNT; f++) {
        uint8_t sp = bomb_skill_of(f, false), su = bomb_skill_of(f, true);
        assert(!bomb_skill_super(sp) && bomb_skill_super(su));
        assert(bomb_skill_cost(sp) == 2 && bomb_skill_cost(su) == 4);
        assert(bomb_skill_freeze(sp) == 600 && bomb_skill_freeze(su) == 1200);
    }
    assert(bomb_skill_cost(SK_NONE) == 0 && bomb_skill_cost(SK_LID_BOUNCE) == 0 && bomb_skill_cost(SK_TAIJI_PUSH) == 0);
    assert(bomb_skill_freeze(SK_NONE) == 0 && bomb_skill_freeze(SK_LID_BOUNCE) == 400 &&
           bomb_skill_freeze(SK_TAIJI_PUSH) == 400 && bomb_skill_freeze(SK_COUNT) == 0);
    int hits = 0;
    for (uint8_t s = 0; s < SK_COUNT; s++) hits += bomb_skill_hits(s);
    assert(hits == 5 && bomb_skill_hits(SK_SLING) && bomb_skill_hits(SK_DECOY) && bomb_skill_hits(SK_ROCKET) &&
           bomb_skill_hits(SK_CAGE) && bomb_skill_hits(SK_NAG));
}

static uint32_t cross_ms(uint16_t half, uint16_t speed_x1000) { return (uint32_t)half * 2u * 1000u / speed_x1000; }

static void test_aim(void) {
    bomb_aim_t cold = bomb_aim_params(0, false, false), hot = bomb_aim_params(255, false, false);
    assert(cold.speed_x1000 == 1000 && hot.speed_x1000 == 1666);  // 来回 2000 → 1200 ms,单程 1000 个单位
    assert(cold.green_half == 150 && cold.perfect_half == 40 && hot.green_half == 150);
    bomb_aim_t g = bomb_aim_params(0, false, true);
    assert(g.speed_x1000 == 500 && g.green_half == 225 && g.perfect_half == 60);
    bomb_aim_t n = bomb_aim_params(255, true, false);
    assert(n.speed_x1000 == 2499 && n.green_half >= 125);  // 0.7 倍是 105,不够 100 ms 被放大
    for (int heat = 0; heat <= 255; heat++) {
        for (int k = 0; k < 4; k++) {
            bomb_aim_t a = bomb_aim_params((uint8_t)heat, k & 1, k & 2);
            assert(cross_ms(a.green_half, a.speed_x1000) >= 100);
            assert(cross_ms(a.perfect_half, a.speed_x1000) >= 30);
            assert(a.perfect_half < a.green_half && a.green_half < 500);
        }
    }
}

int main(void) {
    test_skills();
    test_aim();
    puts("test_fighters: ok");
    return 0;
}
