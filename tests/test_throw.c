// tests/test_throw.c —— 瞄准扔:箭头怎么走、扔中 / 完美 / 扔歪弹回、能量怎么加、两边的炸弹红度对齐。
#include "duo.h"

#include <assert.h>
#include <stdio.h>

static void start(void) {
    duo_begin();
    run(BOMB_COUNT_MS + 20);
    assert(D.a.phase == BP_PLAY && holder());
}

static int count_ev(bomb_t *g, uint8_t want) {
    int n = 0;
    uint8_t ev;
    while (bomb_event(g, &ev)) n += ev == want;
    return n;
}

static void test_arrow_moves(void) {
    start();
    bomb_t *h = holder();
    uint16_t p0 = bomb_aim_pos(h);  // 开打后走了 20 ms
    assert(p0 <= 30);
    for (int i = 0; i < 30; i++) bomb_tick(h, 10);  // 凉的时候每毫秒 1 个单位(热度涨得很慢)
    assert(bomb_aim_pos(h) >= p0 + 290 && bomb_aim_pos(h) <= p0 + 305);
    for (int i = 0; i < 100; i++) bomb_tick(h, 10);  // 到头往回走
    assert(bomb_aim_pos(h) >= 650 - p0 && bomb_aim_pos(h) <= 720 - p0);
}

static void test_miss_bounces_back(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    count_ev(h, 0);
    assert(bomb_aim_pos(h) < BOMB_AIM_MID - BOMB_GREEN_HALF);  // 刚开打,箭头还在左边
    assert(bomb_press(h));  // 箭头在最左边:扔歪
    assert(h->holding && count_ev(h, BE_MISS) == 1);
    assert(h->out_n == 0);  // 什么都没发
    assert(!bomb_can_throw(h));
    for (int i = 0; i < 59; i++) bomb_tick(h, 10);
    assert(!bomb_can_throw(h) && !bomb_press(h));  // 弹回的 0.6 秒里按了也没用
    bomb_tick(h, 10);
    assert(bomb_can_throw(h));
    throw_hit(h);
    flow();
    assert(o->holding);
}

static void test_hit_no_energy_perfect_one(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    throw_hit(h);
    flow();
    assert(h->en_me == 0 && o->en_peer == 0);
    count_ev(o, 0);
    throw_perfect(o);
    assert(count_ev(o, BE_PERFECT) == 1);
    flow();
    assert(o->en_me == 1 && h->en_peer == 1);
}

static void test_loser_gets_two(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    h->en_me = o->en_peer = 3;
    run(h->remain_ms + 20);
    assert(h->phase == BP_BOOM && h->loser_me);
    assert(h->en_me == BOMB_EN_MAX && o->en_peer == BOMB_EN_MAX);  // 封顶
    assert(o->en_me == 0 && h->en_peer == 0);
}

static void test_elapsed_aligned(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    for (int i = 0; i < 200; i++) bomb_tick(h, 10);  // 只有 h 走了 2 秒(那边卡了)
    aim_solo(h, HIT_LO, HIT_HI);
    assert(bomb_press(h));
    uint32_t e = h->elapsed_ms;
    flow();
    assert(o->holding && o->elapsed_ms == e && bomb_heat(o) == bomb_heat(h));
}

static void test_hot_arrow_faster(void) {
    start();
    bomb_t *h = holder();
    h->elapsed_ms = BOMB_HOT_MS;
    bomb_aim_t a = bomb_aim_now(h);
    assert(a.speed_x1000 == 1666);
}

int main(void) {
    test_arrow_moves();
    test_miss_bounces_back();
    test_hit_no_energy_perfect_one();
    test_loser_gets_two();
    test_elapsed_aligned();
    test_hot_arrow_faster();
    puts("test_throw: ok");
    return 0;
}
