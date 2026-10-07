// tests/test_match.c —— 两个机器人随机对打 100 场:拿到炸弹后随机等 0–2 秒再扔,随机断线。
// 每场都要两边都到结束页、比分镜像;先手(第 1 回合炸弹给谁)两边都轮得到。
#include "duo.h"

#include <assert.h>
#include <stdio.h>

static uint32_t s_x = 777;
static uint32_t rnd(void) {
    s_x = s_x * 1664525u + 1013904223u;
    return s_x >> 8;
}

int main(void) {
    int first_a = 0, drops = 0, rounds = 0;
    for (int match = 0; match < 100; match++) {
        duo_seed(1000u + (uint32_t)match);
        duo_init();
        duo_connect();
        flow();
        duo_pick((uint8_t)(match % BF_COUNT), (uint8_t)(match / BF_COUNT % BF_COUNT));
        if (D.a.holding) first_a++;
        int64_t wait[2] = { -1, -1 };  // 拿到炸弹后打算等多久再扔(-1 = 还没拿到)
        for (int step = 0; step < 100000 && !(D.a.phase == BP_OVER && D.b.phase == BP_OVER); step++) {
            bomb_tick(&D.a, 10);
            bomb_tick(&D.b, 10);
            bomb_t *bot[2] = { &D.a, &D.b };
            for (int i = 0; i < 2; i++) {
                if (!bot[i]->holding || bot[i]->phase != BP_PLAY) {
                    wait[i] = -1;
                    continue;
                }
                if (wait[i] < 0) wait[i] = BOMB_CATCH_MS + rnd() % 2000;
                if (bot[i]->hold_ms >= (uint32_t)wait[i] && bomb_press(bot[i])) wait[i] = -1;
            }
            if (rnd() % 3000 == 0) {
                duo_drop();
                duo_connect();
                drops++;
            }
            flow();
            uint8_t ev;
            while (bomb_event(&D.a, &ev)) rounds += ev == BE_BOOM_ME || ev == BE_BOOM_PEER;
            while (bomb_event(&D.b, &ev)) {}
        }
        assert(D.a.phase == BP_OVER && D.b.phase == BP_OVER);
        mirror();
        assert(bomb_over(&D.a));
    }
    printf("  100 场,%d 个回合,断线 %d 次,第 1 回合炸弹先给主动方 %d 次\n", rounds, drops, first_a);
    assert(first_a > 20 && first_a < 80);
    puts("test_match: ok");
    return 0;
}
