// tests/duo.c —— 见 duo.h。
#include "duo.h"

#include <assert.h>
#include <string.h>

duo_t D;
static uint32_t s_rng = 1;

uint32_t duo_rnd(void) {
    s_rng = s_rng * 1103515245u + 12345u;
    return s_rng >> 8;
}

void duo_seed(uint32_t s) { s_rng = s; }

bool duo_send(void *ctx, const void *m, size_t len) {
    pipe_t *p = ctx;
    assert(len >= 1 && len <= BOMB_MSG_MAX);
    if (!p->up || p->n >= p->cap) return false;
    int i = (p->head + p->n) % PIPE_N;
    memcpy(p->q[i], m, len);
    p->len[i] = len;
    p->n++;
    return true;
}

void duo_init(void) {
    memset(&D, 0, sizeof(D));
    bomb_init(&D.a, duo_send, &D.ab, duo_rnd);
    bomb_init(&D.b, duo_send, &D.ba, duo_rnd);
    D.ab.cap = D.ba.cap = 8;
}

void duo_connect(void) {
    D.ab.up = D.ba.up = true;
    bomb_connected(&D.a, true);
    bomb_connected(&D.b, false);
}

void duo_drop(void) {
    D.ab.up = D.ba.up = false;
    D.ab.n = D.ba.n = 0;
    bomb_lost(&D.a);
    bomb_lost(&D.b);
}

void duo_pick(uint8_t a, uint8_t b) {
    assert(bomb_pick(&D.a, a));
    assert(bomb_pick(&D.b, b));
    flow();
}

void duo_begin(void) {
    duo_init();
    duo_connect();
    flow();
    duo_pick(0, 1);
    assert(D.a.phase == BP_COUNT && D.b.phase == BP_COUNT);
}

static bool deliver(pipe_t *p, bomb_t *to) {
    if (!p->n) return false;
    uint8_t m[BOMB_MSG_MAX];
    size_t len = p->len[p->head];
    memcpy(m, p->q[p->head], len);
    p->head = (p->head + 1) % PIPE_N;
    p->n--;
    bomb_on_msg(to, m, len);
    return true;
}

void flow(void) {
    for (bool any = true; any;) {
        any = false;
        bomb_pump(&D.a);
        bomb_pump(&D.b);
        while (deliver(&D.ab, &D.b)) any = true;
        while (deliver(&D.ba, &D.a)) any = true;
    }
}

void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 10) {
        bomb_tick(&D.a, 10);
        bomb_tick(&D.b, 10);
        flow();
    }
}

void mirror(void) {
    assert(D.a.match == D.b.match);
    assert(D.a.score_me == D.b.score_peer && D.a.score_peer == D.b.score_me);
}

bomb_t *holder(void) { return D.a.holding ? &D.a : D.b.holding ? &D.b : NULL; }
bomb_t *other(bomb_t *g) { return g == &D.a ? &D.b : &D.a; }

static bool aimed(const bomb_t *g, uint16_t lo, uint16_t hi) {
    uint16_t p = bomb_aim_pos(g);
    return bomb_can_throw(g) && p >= lo && p <= hi;
}

void aim_at(bomb_t *g, uint16_t lo, uint16_t hi) {
    for (int t = 0; !aimed(g, lo, hi); t += 10) {
        assert(t < 5000);
        bomb_tick(&D.a, 10);
        bomb_tick(&D.b, 10);
        flow();
    }
}

void aim_solo(bomb_t *g, uint16_t lo, uint16_t hi) {
    for (int t = 0; !aimed(g, lo, hi); t += 10) {
        assert(t < 5000);
        bomb_tick(g, 10);
    }
}

void throw_hit(bomb_t *g) {
    aim_at(g, HIT_LO, HIT_HI);
    assert(bomb_press(g) && !g->holding);
}

void throw_perfect(bomb_t *g) {
    aim_at(g, PERFECT_LO, PERFECT_HI);
    assert(bomb_press(g) && !g->holding);
}
