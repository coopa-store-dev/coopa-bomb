// tests/test_skills.c —— 八个招式、锅盖崩回、太极推回、特写冻结,以及它们叠在一起。
#include "duo.h"

#include <assert.h>
#include <stdio.h>

// 两边选好人开打,炸弹先在 a(a_holds)或 b 手里。
static bomb_t *start_with(uint8_t ca, uint8_t cb, bool a_holds) {
    for (uint32_t seed = 1;; seed++) {
        duo_seed(seed);
        duo_init();
        duo_connect();
        flow();
        duo_pick(ca, cb);
        run(BOMB_COUNT_MS + 20);
        if ((holder() == &D.a) == a_holds) return holder();
    }
}

static void give(bomb_t *g, uint8_t en) {
    g->en_me = en;
    other(g)->en_peer = en;
}

static void ready(bomb_t *g) { aim_at(g, 0, 1000); }

static int count_ev(bomb_t *g, uint8_t want) {
    int n = 0;
    uint8_t ev;
    while (bomb_event(g, &ev)) n += ev == want;
    return n;
}

// 收到招:冻结期间引信不烧、不能扔;冻结完再等 catch 毫秒。
static void check_frozen(bomb_t *o, uint32_t freeze, uint32_t catch_ms) {
    assert(o->holding && o->freeze_ms == freeze && o->catch_ms == catch_ms);
    uint32_t left = o->remain_ms;
    run(freeze);
    assert(o->remain_ms == left && o->freeze_ms == 0 && !bomb_can_throw(o));
    run(catch_ms - 10);
    assert(!bomb_can_throw(o));
    run(10);
}

static void test_sling(void) {
    bomb_t *h = start_with(BF_KID, BF_DAD, true), *o = other(h);
    give(h, 2);
    ready(h);
    count_ev(o, 0);
    assert(bomb_key(h, BK_UP) && !h->holding && h->en_me == 0 && h->skill_out == SK_SLING);
    flow();
    assert(count_ev(o, BE_SKILL_IN) == 1 && o->skill_in == SK_SLING);
    assert(o->en_me == 1 && o->en_peer == 0 && h->en_peer == 1);  // 被打中 +1,放招的一方也跟着记
    check_frozen(o, 600, 1500);
    assert(bomb_can_throw(o));
}

static void test_not_enough_energy(void) {
    bomb_t *h = start_with(BF_KID, BF_DAD, true);
    give(h, 3);
    ready(h);
    assert(!bomb_key(h, BK_DOWN) && h->holding && h->out_n == 0);  // 超必杀要 4 格
    give(h, 4);
    assert(bomb_key(h, BK_DOWN) && h->en_me == 0);
}

static void test_cast_timing(void) {
    bomb_t *h = start_with(BF_KID, BF_DAD, true), *o = other(h);
    throw_hit(h);
    flow();
    give(o, 4);
    assert(!bomb_key(o, BK_UP) && !bomb_key(o, BK_DOWN));  // 接住冷却里
    ready(o);
    aim_at(o, 0, 50);
    assert(bomb_press(o) && o->miss_ms);                     // 扔歪
    assert(!bomb_key(o, BK_UP) && o->en_me == 4);            // 弹回时也不能放
    run(BOMB_MISS_MS);
    assert(bomb_key(o, BK_UP) && o->en_me == 2);
}

static void test_decoy(void) {
    for (int right = 0; right < 2; right++) {
        bomb_t *h = start_with(BF_KID, BF_GRANNY, true), *o = other(h);
        give(h, 4);
        ready(h);
        assert(bomb_key(h, BK_DOWN));
        flow();
        assert(o->decoy == 1 || o->decoy == 2);
        check_frozen(o, 1200, BOMB_CATCH_MS);
        give(o, 4);
        uint8_t real = o->decoy, fake = (uint8_t)(3 - real);
        uint8_t pick = right ? real : fake;
        assert(bomb_key(o, pick == 1 ? BK_UP : BK_DOWN) && o->decoy_sel == pick && o->en_me == 4);  // ▲▼ 是选,不放招
        count_ev(o, 0);
        if (!right) {
            assert(bomb_press(o) && o->holding && !o->decoy && count_ev(o, BE_DECOY_PUFF) == 1);
            assert(o->miss_ms == 1200 && !bomb_can_throw(o));
            run(1200);
        } else {
            assert(!bomb_can_throw(o));  // 还有两个炸弹:● 是「扔选中的那个」,箭头照走
            while (bomb_aim_pos(o) < HIT_LO || bomb_aim_pos(o) > HIT_HI) run(10);
            assert(bomb_press(o) && !o->holding && !o->decoy);  // 选对了:照常瞄准扔
        }
    }
}

static void test_rocket(void) {
    bomb_t *h = start_with(BF_DAD, BF_KID, true), *o = other(h);
    give(h, 2);
    ready(h);
    assert(bomb_key(h, BK_UP));
    flow();
    assert(o->fast);
    run(600);
    uint32_t left = o->remain_ms;
    run(100);
    assert(left - o->remain_ms == 200);  // 烧两倍快
    throw_hit(o);
    assert(!o->fast);
}

static void test_cage(void) {
    bomb_t *h = start_with(BF_DAD, BF_KID, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN));
    flow();
    assert(o->cage_left == BOMB_CAGE_HITS && BOMB_CAGE_HITS == 8);
    run(1200);
    give(o, 4);
    assert(!bomb_can_throw(o) && !bomb_key(o, BK_UP));  // 笼子里不能扔、不能放招
    count_ev(o, 0);
    for (int i = 0; i < BOMB_CAGE_HITS; i++) assert(bomb_press(o));
    assert(count_ev(o, BE_CAGE_HIT) == BOMB_CAGE_HITS && !o->cage_left && o->holding);
    assert(!bomb_press(o) && o->miss_ms == 0);  // 砸开后 0.3 秒:多按的那下不算扔歪
    run(BOMB_GUARD_MS);
    assert(!o->guard_ms);
    run(BOMB_CATCH_MS);  // 「接住」的 0.5 秒和砸笼子一起算,这里砸得太快还没过
    assert(bomb_can_throw(o));
}

static void test_nag(void) {
    bomb_t *h = start_with(BF_GRANNY, BF_KID, true), *o = other(h);
    give(h, 2);
    ready(h);
    assert(bomb_key(h, BK_UP));
    flow();
    assert(o->nag);
    bomb_aim_t plain = bomb_aim_params(bomb_heat(o), false, false), now = bomb_aim_now(o);
    assert(now.speed_x1000 == plain.speed_x1000 * 3 / 2 && now.green_half < plain.green_half);
    run(600);
    throw_hit(o);
    assert(!o->nag);
}

static void test_glasses(void) {
    bomb_t *h = start_with(BF_GRANDPA, BF_KID, true), *o = other(h);
    give(h, 2);
    ready(h);
    assert(bomb_key(h, BK_UP) && h->glasses_left == 3);
    flow();
    assert(o->freeze_ms == 600 && o->en_me == 0);  // 自己身上的招:对方不加能量
    run(600);
    throw_hit(o);
    flow();
    bomb_aim_t a = bomb_aim_now(h), plain = bomb_aim_params(bomb_heat(h), false, false);
    assert(a.speed_x1000 == plain.speed_x1000 / 2 && a.green_half > plain.green_half);
    throw_perfect(h);
    assert(h->en_me == 0 && h->glasses_left == 2);  // 戴着老花镜:完美也不加
}

static void test_lid(void) {
    bomb_t *h = start_with(BF_GRANNY, BF_KID, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN) && h->lid);
    flow();
    run(1200);
    throw_hit(o);
    flow();
    assert(h->holding);
    count_ev(h, 0);
    run(h->remain_ms + 10);  // 炸在奶奶手里
    assert(!h->lid && !h->holding && h->score_peer == 0 && count_ev(h, BE_LID) == 1);
    assert(o->holding && o->skill_in == SK_LID_BOUNCE && o->freeze_ms > 0);
    assert(o->remain_ms == BOMB_LID_MS);
    run(o->freeze_ms + o->remain_ms + 10);
    assert(o->phase == BP_BOOM && o->loser_me && h->score_me == 1);
}

static void test_lid_only_next_hold(void) {  // 锅盖只顶放完以后的下一次拿炸弹
    bomb_t *h = start_with(BF_GRANNY, BF_KID, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN) && h->lid);
    flow();
    run(1200);
    throw_hit(o);
    flow();
    assert(h->lid);
    throw_hit(h);
    assert(!h->lid);
}

static void test_taiji_voids_skill(void) {
    bomb_t *h = start_with(BF_GRANDPA, BF_DAD, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN) && h->taiji);
    flow();
    run(1200);
    give(o, 4);
    ready(o);
    assert(bomb_key(o, BK_DOWN));  // 铁笼
    flow();
    uint32_t left = h->remain_ms;
    assert(h->holding && !h->taiji && h->cage_left == 0 && h->en_me == 0 && h->freeze_ms == BOMB_TAIJI_MS);
    assert(count_ev(h, BE_TAIJI) == 1);
    run(BOMB_TAIJI_MS);
    assert(!h->holding && o->holding && o->skill_in == SK_TAIJI_PUSH && o->remain_ms == left);
    assert(o->cage_left == 0);
}

static void test_taiji_vs_taiji(void) {
    bomb_t *h = start_with(BF_GRANDPA, BF_GRANDPA, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN));
    flow();
    run(1200);
    give(o, 4);
    ready(o);
    assert(bomb_key(o, BK_DOWN) && o->taiji);
    flow();
    run(BOMB_TAIJI_MS * 3);  // h 推回去,o 的架势再推回来,停
    assert(h->holding && !o->holding && !h->taiji && !o->taiji);
}

static void test_lid_into_taiji(void) {
    bomb_t *h = start_with(BF_GRANNY, BF_GRANDPA, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN));
    flow();
    run(1200);
    give(o, 4);
    ready(o);
    assert(bomb_key(o, BK_DOWN));
    flow();
    run(1200);
    while (h->holding && h->phase == BP_PLAY) run(10);  // 炸在奶奶手里 → 锅盖崩回 → 爷爷太极推回
    run(BOMB_TAIJI_MS + 20);
    assert(h->holding && !h->lid && !o->taiji && h->score_peer == 0);
    run(h->freeze_ms + h->remain_ms + 20);
    assert(h->loser_me && o->score_me == 1);  // 锅盖用掉了,这次真炸
}

static void test_round_clears(void) {
    bomb_t *h = start_with(BF_GRANNY, BF_DAD, true), *o = other(h);
    give(h, 4);
    ready(h);
    assert(bomb_key(h, BK_DOWN) && h->lid);
    flow();
    o->en_me = 4;
    run(1200);
    ready(o);
    assert(bomb_key(o, BK_DOWN));
    flow();
    for (int t = 0; D.a.round != 2 || D.b.round != 2; t += 10) {  // 笼子里不砸,炸了 → 锅盖崩回 → o 炸了 → 下一回合
        assert(t < 60000);
        run(10);
    }
    assert(o->score_peer == 1 && h->score_me == 1);
    assert(D.a.round == 2 && D.b.round == 2);
    assert(!h->lid && !h->cage_left && !h->nag && !h->fast && !h->decoy && !h->glasses_left && !h->taiji);
    assert(!o->cage_left && !o->lid);
}

static void test_bad_throw_ignored(void) {
    bomb_t *h = start_with(BF_KID, BF_DAD, true), *o = other(h);
    throw_hit(h);
    for (int bad = 0; bad < 2; bad++) {
        uint8_t m[15];
        for (int i = 0; i < 15; i++) m[i] = h->out[h->out_head][i];
        if (bad) m[11] = BOMB_EN_MAX + 1;
        else m[10] = SK_COUNT;
        bomb_on_msg(o, m, sizeof(m));
        assert(!o->holding);
    }
    flow();
    assert(o->holding);
}

int main(void) {
    test_sling();
    test_not_enough_energy();
    test_cast_timing();
    test_decoy();
    test_rocket();
    test_cage();
    test_nag();
    test_glasses();
    test_lid();
    test_lid_only_next_hold();
    test_taiji_voids_skill();
    test_taiji_vs_taiji();
    test_lid_into_taiji();
    test_round_clears();
    test_bad_throw_ignored();
    puts("test_skills: ok");
    return 0;
}
