// tests/test_local.c —— 和电脑打:两个 bomb_t 在内存里接起来,电脑那边收到招一样冻结,两个电脑能打完一场。
#include "bomb_bot.h"
#include "bomb_local.h"

#include <assert.h>
#include <stdio.h>

static uint32_t s_x = 5;
static uint32_t rnd(void) {
    s_x = s_x * 1664525u + 1013904223u;
    return s_x >> 8;
}

static bomb_local_t L;

static void tick(uint32_t dt) {
    bomb_tick(&L.me, dt);
    bomb_tick(&L.cpu, dt);
    bomb_local_flow(&L);
}

static void begin(uint8_t me, uint8_t cpu) {
    bomb_local_init(&L, rnd);
    bomb_local_flow(&L);
    assert(L.me.phase == BP_PICK && L.cpu.phase == BP_PICK && L.me.match == L.cpu.match);
    assert(bomb_pick(&L.me, me) && bomb_pick(&L.cpu, cpu));
    bomb_local_flow(&L);
    assert(L.me.phase == BP_COUNT && L.cpu.phase == BP_COUNT);
    for (int t = 0; t < BOMB_COUNT_MS + 20; t += 10) tick(10);
}

static void test_cpu_frozen_by_skill(void) {
    for (;;) {
        begin(BF_DAD, BF_KID);
        if (L.me.holding) break;
    }
    L.me.en_me = 4;
    while (!bomb_can_throw(&L.me)) tick(10);
    assert(bomb_key(&L.me, BK_DOWN));  // 铁笼
    bomb_local_flow(&L);
    assert(L.cpu.holding && L.cpu.freeze_ms == 1200 && L.cpu.cage_left == BOMB_CAGE_HITS);
    uint32_t left = L.cpu.remain_ms;
    for (int t = 0; t < 1200; t += 10) tick(10);
    assert(L.cpu.remain_ms == left);
}

static void test_bots_finish(void) {
    begin(BF_GRANNY, BF_GRANDPA);
    bomb_bot_t a, b;
    bomb_bot_init(&a, BOT_NORMAL, rnd);
    bomb_bot_init(&b, BOT_HARD, rnd);
    for (int t = 0; t < 600000 && !(L.me.phase == BP_OVER && L.cpu.phase == BP_OVER); t += 10) {
        bomb_seen_t s;
        bomb_seen(&L.me, &s);
        int k = bomb_bot_step(&a, &s, 10);
        if (k >= 0) bomb_key(&L.me, (uint8_t)k);
        bomb_seen(&L.cpu, &s);
        k = bomb_bot_step(&b, &s, 10);
        if (k >= 0) bomb_key(&L.cpu, (uint8_t)k);
        tick(10);
    }
    assert(L.me.phase == BP_OVER && L.cpu.phase == BP_OVER && bomb_over(&L.me));
    assert(L.me.score_me == L.cpu.score_peer && L.me.score_peer == L.cpu.score_me);
}

int main(void) {
    test_cpu_frozen_by_skill();
    test_bots_finish();
    puts("test_local: ok");
    return 0;
}
