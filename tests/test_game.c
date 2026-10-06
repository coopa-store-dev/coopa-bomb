// tests/test_game.c —— 一场比赛:开场、扔和接住冷却、爆炸记分、打到 3 分、再来一场、断线作废和续上、
// 两边重启、版本不同、乱消息、「不能露馅」(热度只看过去多久)。
#include "duo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void start(void) {
    duo_init();
    duo_connect();
    flow();
    run(BOMB_COUNT_MS + 20);
    assert(D.a.phase == BP_PLAY && D.b.phase == BP_PLAY);
}

static void test_start(void) {
    duo_init();
    duo_connect();
    flow();
    assert(D.a.phase == BP_COUNT && D.b.phase == BP_COUNT);
    assert(D.a.match && D.a.match == D.b.match && D.a.round == 1 && D.b.round == 1);
    assert(D.a.holding != D.b.holding);
    bomb_t *h = holder();
    assert(h->remain_ms >= BOMB_FUSE_MIN && h->remain_ms <= BOMB_FUSE_MAX);
    run(BOMB_COUNT_MS + 20);
    assert(D.a.phase == BP_PLAY && D.b.phase == BP_PLAY);
    mirror();
}

static void test_throw_and_catch(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    uint32_t left = h->remain_ms;
    assert(bomb_press(h));
    assert(!h->holding);
    flow();
    assert(o->holding && o->remain_ms == left && o->throws == 1);
    assert(!bomb_press(o));  // 刚接到:0.5 秒内扔不回去
    run(BOMB_CATCH_MS);
    assert(bomb_press(o));
    flow();
    assert(h->holding && h->throws == 2);
}

static void test_press_only_when_allowed(void) {  // Review Focus 3
    duo_init();
    duo_connect();
    flow();
    assert(!bomb_press(&D.a) && !bomb_press(&D.b));  // 倒数时
    run(BOMB_COUNT_MS + 20);
    bomb_t *h = holder(), *o = other(h);
    uint8_t before = o->out_n;
    assert(!bomb_press(o));                          // 没拿炸弹
    assert(o->out_n == before);                      // 也没发出任何东西
    assert(bomb_press(h));
    flow();
    assert(!bomb_press(h));                          // 扔出去了就没了
}

static void test_boom_scores(void) {
    start();
    bomb_t *h = holder(), *o = other(h);
    run(h->remain_ms + 20);
    assert(h->phase == BP_BOOM && o->phase == BP_BOOM);
    assert(h->score_peer == 1 && o->score_me == 1 && h->loser_me && !o->loser_me);
    mirror();
    run(BOMB_NEXT_MS + 20);
    assert(D.a.round == 2 && D.b.round == 2);
    assert(D.a.phase == BP_PLAY && D.b.phase == BP_PLAY);  // 下一回合不倒数
    mirror();
}

static void finish_round(void) {
    bomb_t *h = holder();
    assert(h);
    run(h->remain_ms + BOMB_NEXT_MS + 40);
}

static void test_match_to_three_and_again(void) {
    start();
    while (D.a.phase != BP_OVER) finish_round();
    assert(D.b.phase == BP_OVER && bomb_over(&D.a) && bomb_over(&D.b));
    assert(D.a.score_me == BOMB_WIN || D.a.score_peer == BOMB_WIN);
    mirror();
    uint32_t old = D.a.match;
    assert(bomb_press(&D.b));
    flow();
    assert(D.a.phase == BP_OVER);  // a 还没按
    assert(bomb_press(&D.a));
    flow();
    assert(D.a.match != old && D.a.match == D.b.match && D.a.round == 1);
    assert(D.a.score_me == 0 && D.a.score_peer == 0 && D.b.phase == BP_COUNT);
}

static void test_drop_mid_round(void) {
    start();
    bomb_t *h = holder();
    assert(bomb_press(h));  // 扔出去了,但没送到
    duo_drop();
    assert(D.a.phase == BP_WAIT && D.b.phase == BP_WAIT && !holder());
    duo_connect();
    flow();
    assert(D.a.round == 1 && D.b.round == 1 && D.a.phase == BP_COUNT && D.b.phase == BP_COUNT);
    mirror();
    assert(D.a.score_me == 0 && D.a.score_peer == 0);
}

static void test_drop_after_boom(void) {  // Review Focus 1
    for (uint32_t seed = 1; seed < 40; seed++) {  // 换着种子,两种情况(主动方炸 / 被连方炸)都碰到
        duo_seed(seed);
        start();
        bomb_t *h = holder();
        uint32_t left = h->remain_ms;
        for (uint32_t t = 0; t < left + 10; t += 10) {  // 只走时间不送消息:BOOM 留在队列里
            bomb_tick(&D.a, 10);
            bomb_tick(&D.b, 10);
        }
        duo_drop();
        duo_connect();
        flow();
        mirror();
        assert(D.a.score_me + D.a.score_peer <= 1);
        // 主动方炸的:它记了这一分,开第 2 回合;被连方炸的:主动方不知道,重开第 1 回合、0:0
        assert(D.a.round == (h == &D.a ? 2 : 1) && D.b.round == D.a.round);
    }
}

static void test_peer_restart_gets_scores(void) {  // Review Focus 2
    start();
    finish_round();
    mirror();
    uint8_t me = D.a.score_me, peer = D.a.score_peer;
    duo_drop();
    bomb_init(&D.b, duo_send, &D.ba, duo_rnd);  // 被连方重启
    duo_connect();
    flow();
    assert(D.b.score_me == peer && D.b.score_peer == me && D.b.round == 2);
}

static void test_initiator_restart_new_match(void) {
    start();
    finish_round();
    uint32_t old = D.a.match;
    duo_drop();
    bomb_init(&D.a, duo_send, &D.ab, duo_rnd);  // 主动方重启:比分没了,开新的一场
    duo_connect();
    flow();
    assert(D.a.match != old && D.b.match == D.a.match && D.b.score_me == 0 && D.b.score_peer == 0);
}

static void test_over_survives_reconnect(void) {
    start();
    while (D.a.phase != BP_OVER) finish_round();
    duo_drop();
    duo_connect();
    flow();
    assert(D.a.phase == BP_OVER && D.b.phase == BP_OVER);
    mirror();
}

static void test_version_mismatch(void) {
    duo_init();
    duo_connect();
    uint8_t h[2] = { 'H', BOMB_PROTO + 1 };
    bomb_on_msg(&D.b, h, sizeof(h));
    uint8_t ev = 0, last = 0;
    while (bomb_event(&D.b, &ev)) last = ev;
    assert(D.b.bad_ver && last == BE_BAD_VER);
    bomb_connected(&D.b, false);
    assert(!D.b.bad_ver);
}

static void test_garbage_ignored(void) {  // Review Focus 4
    start();
    static const uint8_t TYPES[] = { 'H', 'N', 'T', 'B', 'R', 0, 0xFF };
    uint8_t m[BOMB_MSG_MAX];
    uint32_t x = 9;
    for (size_t t = 0; t < sizeof(TYPES); t++) {
        for (size_t len = 0; len <= BOMB_MSG_MAX; len++) {
            for (int r = 0; r < 20; r++) {
                for (size_t k = 0; k < sizeof(m); k++) {
                    x = x * 1103515245u + 12345u;
                    m[k] = (uint8_t)(x >> 16);
                }
                m[0] = TYPES[t];
                bomb_on_msg(&D.b, m, len);
                assert(D.b.score_me <= BOMB_WIN && D.b.score_peer <= BOMB_WIN && D.b.phase <= BP_OVER);
                assert(!D.b.holding || D.b.remain_ms <= BOMB_FUSE_MAX);
            }
        }
    }
    duo_drop();
    duo_connect();
    flow();
    mirror();  // 连回来以主动方为准
}

static void test_heat_only_elapsed(void) {
    bomb_t g;
    memset(&g, 0, sizeof(g));
    g.phase = BP_PLAY;
    g.elapsed_ms = 5000;
    g.remain_ms = 100;
    uint8_t h1 = bomb_heat(&g);
    uint32_t t1 = bomb_tick_gap(&g);
    g.remain_ms = 15000;  // 剩多少不影响
    assert(bomb_heat(&g) == h1 && bomb_tick_gap(&g) == t1);
    g.elapsed_ms = 0;
    assert(bomb_heat(&g) < h1 && bomb_tick_gap(&g) > t1);
    g.elapsed_ms = 60000;
    assert(bomb_heat(&g) == 255 && bomb_tick_gap(&g) >= 120);
}

int main(void) {
    test_start();
    test_throw_and_catch();
    test_press_only_when_allowed();
    test_boom_scores();
    test_match_to_three_and_again();
    test_drop_mid_round();
    test_drop_after_boom();
    test_peer_restart_gets_scores();
    test_initiator_restart_new_match();
    test_over_survives_reconnect();
    test_version_mismatch();
    test_garbage_ignored();
    test_heat_only_elapsed();
    puts("test_game: ok");
    return 0;
}
