// tests/test_game.c —— 一场比赛:开场、扔和接住冷却、爆炸记分、打到 3 分、再来一场、断线作废和续上、
// 两边重启、版本不同、乱消息、「不能露馅」(热度只看过去多久)。
#include "duo.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void start(void) {
    duo_begin();
    run(BOMB_COUNT_MS + 20);
    assert(D.a.phase == BP_PLAY && D.b.phase == BP_PLAY);
}

static void test_start(void) {
    duo_begin();
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
    aim_at(h, HIT_LO, HIT_HI);
    uint32_t left = h->remain_ms;
    assert(bomb_press(h));
    assert(!h->holding);
    flow();
    assert(o->holding && o->remain_ms == left && o->throws == 1);
    assert(!bomb_press(o) && !bomb_can_throw(o));  // 刚接到:0.5 秒内扔不回去
    run(BOMB_CATCH_MS - 10);
    assert(!bomb_can_throw(o));
    throw_hit(o);
    flow();
    assert(h->holding && h->throws == 2);
}

static void test_press_only_when_allowed(void) {  // Review Focus 3
    duo_begin();
    assert(!bomb_press(&D.a) && !bomb_press(&D.b));  // 倒数时
    run(BOMB_COUNT_MS + 20);
    bomb_t *h = holder(), *o = other(h);
    uint8_t before = o->out_n;
    assert(!bomb_press(o));                          // 没拿炸弹
    assert(o->out_n == before);                      // 也没发出任何东西
    throw_hit(h);
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
    D.a.en_me = D.b.en_peer = 3;
    assert(bomb_press(&D.a));
    flow();
    // 两边都按了:回选人页,新的场号,比分、能量清零;再选一次才开打
    assert(D.a.phase == BP_PICK && D.b.phase == BP_PICK);
    assert(D.a.match != old && D.a.match == D.b.match);
    assert(D.a.score_me == 0 && D.a.score_peer == 0 && D.b.score_me == 0 && D.a.en_me == 0 && D.b.en_peer == 0);
    assert(!D.a.me_picked && !D.b.me_picked && !D.a.peer_picked);
    duo_pick(2, 3);
    assert(D.a.round == 1 && D.b.phase == BP_COUNT && D.a.me_char == 2 && D.b.peer_char == 2 && D.a.peer_char == 3);
}

static void test_drop_mid_round(void) {
    start();
    bomb_t *h = holder();
    throw_hit(h);  // 扔出去了,但没送到
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
    assert(D.a.phase == BP_PICK && D.b.phase == BP_PICK && !D.b.me_picked);  // 新的一场:重新选人
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
    static const uint8_t TYPES[] = { 'H', 'S', 'P', 'N', 'T', 'B', 'R', 0, 0xFF };
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
                assert(D.b.score_me <= BOMB_WIN && D.b.score_peer <= BOMB_WIN && D.b.phase <= BP_PICK);
                assert((D.b.me_char < BF_COUNT || D.b.me_char == BOMB_NOCHAR) && (D.b.peer_char < BF_COUNT || D.b.peer_char == BOMB_NOCHAR));
                assert(D.b.en_me <= BOMB_EN_MAX && D.b.en_peer <= BOMB_EN_MAX);
                assert(!D.b.holding || D.b.remain_ms <= BOMB_FUSE_MAX);
            }
        }
    }
    duo_drop();
    duo_connect();
    flow();
    mirror();  // 连回来以主动方为准
}

// 外壳每帧 dt 最多 100 ms:卡一顿(截图、写 Flash),这边的倒数就比对方慢。对方倒数完扔过来 / 炸了,
// 这边还在倒数也要接住 / 记分,不然两边都不拿炸弹,这一回合永远卡住(真卡上碰到过)。
static void lag_count(bomb_t *h) {
    for (uint32_t t = 0; t < BOMB_COUNT_MS + 20; t += 10) bomb_tick(h, 10);
    assert(other(h)->phase == BP_COUNT);
    assert(h->phase == BP_PLAY && other(h)->phase == BP_COUNT);
}

static void test_throw_while_peer_counting(void) {
    duo_begin();
    bomb_t *h = holder(), *o = other(h);
    lag_count(h);
    aim_solo(h, HIT_LO, HIT_HI);
    uint32_t left = h->remain_ms;
    assert(bomb_press(h));
    flow();
    assert(o->phase == BP_PLAY && o->holding && o->remain_ms == left && o->throws == 1);
}

static void test_boom_while_peer_counting(void) {
    duo_begin();
    bomb_t *h = holder(), *o = other(h);
    lag_count(h);
    while (h->phase == BP_PLAY) bomb_tick(h, 10);
    flow();
    assert(o->phase == BP_BOOM && o->score_me == 1 && h->score_peer == 1);
}

// 赛点炸在被连方手里,BOOM 还没送到就断线:被连方不能自己判这场打完(不然记一场输、放结束音,连回来又接着打)。
static void test_responder_never_ends_match_alone(void) {  // Review Focus 1 + 5
    for (uint32_t seed = 1;; seed++) {
        duo_seed(seed);
        start();
        if (holder() == &D.b) break;
    }
    D.a.score_me = D.a.score_peer = D.b.score_me = D.b.score_peer = 2;
    uint8_t ev;
    while (bomb_event(&D.b, &ev)) {}
    uint32_t left = D.b.remain_ms;
    for (uint32_t t = 0; t < left + BOMB_NEXT_MS + 1000; t += 10) {  // 只走时间不送消息
        bomb_tick(&D.a, 10);
        bomb_tick(&D.b, 10);
    }
    while (bomb_event(&D.b, &ev)) assert(ev != BE_OVER);
    assert(D.b.phase != BP_OVER);
    duo_drop();
    duo_connect();
    flow();
    mirror();
    assert(D.a.score_me == 2 && D.a.score_peer == 2 && D.b.phase == BP_COUNT);
}

// 赛点正常炸了:被连方等主动方说「打完了」才结束,两边都只结束一次。
static void test_match_point_ends_once(void) {
    for (uint32_t seed = 1;; seed++) {
        duo_seed(seed);
        start();
        if (holder() == &D.b) break;
    }
    D.a.score_me = D.a.score_peer = D.b.score_me = D.b.score_peer = 2;
    uint8_t ev;
    while (bomb_event(&D.a, &ev)) {}
    while (bomb_event(&D.b, &ev)) {}
    run(D.b.remain_ms + BOMB_NEXT_MS + 500);
    int over_a = 0, over_b = 0;
    while (bomb_event(&D.a, &ev)) over_a += ev == BE_OVER;
    while (bomb_event(&D.b, &ev)) over_b += ev == BE_OVER;
    assert(over_a == 1 && over_b == 1);
    assert(D.a.phase == BP_OVER && D.b.phase == BP_OVER && D.a.score_me == 3 && D.b.score_peer == 3);
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


static void test_pick_flow(void) {
    duo_init();
    duo_connect();
    flow();
    assert(D.a.phase == BP_PICK && D.b.phase == BP_PICK);
    assert(D.a.match && D.a.match == D.b.match);
    assert(!bomb_pick(&D.a, BF_COUNT) && !bomb_pick(&D.a, BOMB_NOCHAR));
    assert(bomb_pick(&D.a, BF_GRANNY));
    flow();
    assert(D.a.phase == BP_PICK && D.b.peer_picked && D.b.peer_char == BF_GRANNY);  // 只一边选了:不开
    assert(bomb_pick(&D.b, BF_GRANNY));                                            // 可以选同一个人
    flow();
    assert(D.a.phase == BP_COUNT && D.b.phase == BP_COUNT);
    assert(D.a.me_char == BF_GRANNY && D.a.peer_char == BF_GRANNY && D.b.me_char == BF_GRANNY);
    assert(!bomb_pick(&D.a, BF_KID));  // 开打了不能再选
}

static void test_unpick(void) {
    duo_init();
    duo_connect();
    flow();
    assert(!bomb_unpick(&D.b));
    assert(bomb_pick(&D.b, BF_KID));
    flow();
    assert(D.a.peer_picked);
    assert(bomb_unpick(&D.b));
    flow();
    assert(!D.a.peer_picked && D.a.peer_char == BOMB_NOCHAR);
    assert(bomb_pick(&D.a, BF_DAD));
    flow();
    assert(D.a.phase == BP_PICK);  // 对方撤回了:不开
    assert(bomb_pick(&D.b, BF_GRANDPA));
    flow();
    assert(D.a.phase == BP_COUNT && D.a.peer_char == BF_GRANDPA);
}

static void test_pick_survives_drop(void) {
    duo_init();
    duo_connect();
    flow();
    uint32_t m = D.a.match;
    assert(bomb_pick(&D.a, BF_DAD) && bomb_pick(&D.b, BF_KID));  // 都按了,但消息没送到
    duo_drop();
    assert(D.a.phase == BP_WAIT && D.b.phase == BP_WAIT);
    duo_connect();
    flow();
    // 连回来:同一个场号,两边重发自己的选择,直接开打
    assert(D.a.match == m && D.b.match == m && D.a.phase == BP_COUNT && D.b.phase == BP_COUNT);
    assert(D.a.peer_char == BF_KID && D.b.peer_char == BF_DAD);
}

static void test_peer_swapped_while_picking(void) {  // 选人页断线,对方重启(或换了张卡):不能沿用它上次的选择
    duo_init();
    duo_connect();
    flow();
    assert(bomb_pick(&D.b, BF_DAD));
    flow();
    assert(D.a.peer_picked);
    duo_drop();
    bomb_init(&D.b, duo_send, &D.ba, duo_rnd);
    duo_connect();
    flow();
    assert(D.a.phase == BP_PICK && !D.a.peer_picked && D.a.peer_char == BOMB_NOCHAR);
    assert(bomb_pick(&D.a, BF_KID));
    flow();
    assert(D.a.phase == BP_PICK);  // 对方还没选:不开打
}

static void test_stale_pick_ignored(void) {  // 比赛中、或者场号不对的 PICK 不换人
    start();
    uint8_t m[6] = { 'P' };
    for (int i = 0; i < 4; i++) m[1 + i] = (uint8_t)(D.b.match >> (8 * i));
    m[5] = BF_GRANDPA;
    bomb_on_msg(&D.b, m, sizeof(m));
    assert(D.b.peer_char == BF_KID && D.b.phase == BP_PLAY);
}

static void test_responder_restart_gets_chars(void) {  // 被连方重启丢了角色:ROUND 里带着
    start();
    duo_drop();
    bomb_init(&D.b, duo_send, &D.ba, duo_rnd);
    duo_connect();
    flow();
    assert(D.b.phase == BP_COUNT && D.b.me_char == BF_DAD && D.b.peer_char == BF_KID);
}

static void test_old_version(void) {
    duo_init();
    duo_connect();
    uint8_t h[2] = { 'H', 1 };
    bomb_on_msg(&D.b, h, sizeof(h));
    assert(D.b.bad_ver);
}

int main(void) {
    test_responder_never_ends_match_alone();
    test_match_point_ends_once();
    test_throw_while_peer_counting();
    test_boom_while_peer_counting();
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
    test_pick_flow();
    test_unpick();
    test_pick_survives_drop();
    test_peer_swapped_while_picking();
    test_stale_pick_ignored();
    test_responder_restart_gets_chars();
    test_old_version();
    puts("test_game: ok");
    return 0;
}
