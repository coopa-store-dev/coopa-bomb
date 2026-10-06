// src/bomb_game.c —— 见 bomb_game.h。
#include "bomb_game.h"

#include <string.h>

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void enqueue(bomb_t *g, const uint8_t *m, size_t len) {
    if (g->out_n == BOMB_OUTQ) return;  // 满了(正常打不满):丢掉,断线重连时以主动方为准
    uint8_t i = (uint8_t)((g->out_head + g->out_n) % BOMB_OUTQ);
    memcpy(g->out[i], m, len);
    g->out_len[i] = (uint8_t)len;
    g->out_n++;
}

static void event(bomb_t *g, uint8_t e) {
    if (g->ev_n == BOMB_EVQ) {  // 满了丢最旧的:只影响声音和动画
        g->ev_head = (uint8_t)((g->ev_head + 1) % BOMB_EVQ);
        g->ev_n--;
    }
    g->ev[(g->ev_head + g->ev_n) % BOMB_EVQ] = e;
    g->ev_n++;
}

bool bomb_over(const bomb_t *g) { return g->score_me >= BOMB_WIN || g->score_peer >= BOMB_WIN; }

static void finish(bomb_t *g) {
    g->phase = BP_OVER;
    g->holding = false;
    event(g, BE_OVER);
}

// 两边开一回合的同一段:炸弹给谁、引信多长、要不要先倒数。
static void begin(bomb_t *g, uint8_t round, bool mine, uint16_t fuse, uint16_t delay) {
    g->round = round;
    g->decided = false;
    g->holding = mine;
    g->remain_ms = fuse;
    g->elapsed_ms = 0;
    g->hold_ms = BOMB_CATCH_MS;  // 开局拿到的不用等「接住」
    g->throws = 0;
    g->delay_ms = delay;
    g->next_ms = 0;
    g->phase = delay ? BP_COUNT : BP_PLAY;
    event(g, BE_ROUND);
}

static void send_round(bomb_t *g, uint8_t holder, uint16_t fuse, uint16_t delay) {
    uint8_t m[13] = { 'N' };
    put32(m + 1, g->match);
    m[5] = g->round;
    m[6] = g->score_me;    // 主动方比分
    m[7] = g->score_peer;  // 被连方比分
    m[8] = holder;         // 0 主动方 / 1 被连方 / 2 这场打完了
    put16(m + 9, fuse);
    put16(m + 11, delay);
    enqueue(g, m, sizeof(m));
}

// 主动方:随机定引信和先给谁,发出去,自己也开。
static void start_round(bomb_t *g, uint8_t round, uint16_t delay) {
    uint32_t r = g->rnd();
    uint16_t fuse = (uint16_t)(BOMB_FUSE_MIN + r % (BOMB_FUSE_MAX - BOMB_FUSE_MIN + 1));
    uint8_t holder = (uint8_t)((r >> 20) & 1);
    g->round = round;
    send_round(g, holder, fuse, delay);
    begin(g, round, holder == 0, fuse, delay);
}

static void new_match(bomb_t *g) {
    g->match = g->rnd() | 1u;
    g->score_me = g->score_peer = 0;
    g->me_again = g->peer_again = false;
    start_round(g, 1, BOMB_COUNT_MS);
}

// 主动方:对方连上来了。没开过就开新的一场;打完了就告诉对方结果;否则重开这一回合(炸过了就开下一回合)。
static void resume(bomb_t *g) {
    if (!g->match) {
        new_match(g);
    } else if (bomb_over(g)) {
        send_round(g, BOMB_NOBODY, 0, 0);
        finish(g);
    } else {
        start_round(g, g->decided ? (uint8_t)(g->round + 1) : g->round, BOMB_COUNT_MS);
    }
}

static void maybe_again(bomb_t *g) {
    if (g->initiator && g->phase == BP_OVER && g->me_again && g->peer_again) new_match(g);
}

static void on_hello(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 2) return;
    if (m[1] != BOMB_PROTO) {
        g->bad_ver = true;
        event(g, BE_BAD_VER);
        return;
    }
    if (g->initiator) resume(g);
}

static void on_round(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 13 || g->initiator) return;
    uint8_t holder = m[8];
    uint16_t fuse = get16(m + 9), delay = get16(m + 11);
    if (holder > BOMB_NOBODY || m[6] > BOMB_WIN || m[7] > BOMB_WIN || delay > BOMB_COUNT_MS || !m[5]) return;
    if (holder != BOMB_NOBODY && (fuse < BOMB_FUSE_MIN || fuse > BOMB_FUSE_MAX)) return;
    uint32_t match = get32(m + 1);
    if (!match) return;
    if (match != g->match) g->me_again = g->peer_again = false;
    g->match = match;
    g->score_me = m[7];
    g->score_peer = m[6];
    g->round = m[5];
    if (holder == BOMB_NOBODY) {
        if (bomb_over(g)) finish(g);
        else g->phase = BP_WAIT;
        return;
    }
    begin(g, m[5], holder == 1, fuse, delay);
}

// 对方扔过来 / 炸了,说明它的倒数已经完了;这边的倒数可能因为卡顿慢一点,直接结束(否则消息被丢、回合卡住)。
static bool playing(bomb_t *g) {
    if (g->phase == BP_COUNT) {
        g->delay_ms = 0;
        g->phase = BP_PLAY;
    }
    return g->phase == BP_PLAY;
}

static void on_throw(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 10 || get32(m + 1) != g->match || m[5] != g->round || g->holding) return;
    uint16_t left = get16(m + 8);
    if (left > BOMB_FUSE_MAX || !playing(g)) return;
    g->holding = true;
    g->remain_ms = left;
    g->hold_ms = 0;
    g->throws = get16(m + 6);
    event(g, BE_GOT);
}

static void on_boom(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 6 || get32(m + 1) != g->match || m[5] != g->round || g->holding || !playing(g)) return;
    if (g->score_me < BOMB_WIN) g->score_me++;
    g->decided = true;
    g->loser_me = false;
    g->phase = BP_BOOM;
    g->next_ms = BOMB_NEXT_MS;
    event(g, BE_BOOM_PEER);
}

static void on_again(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 5 || get32(m + 1) != g->match) return;
    g->peer_again = true;
    maybe_again(g);
}

void bomb_init(bomb_t *g, bomb_send_fn send, void *ctx, bomb_rnd_fn rnd) {
    memset(g, 0, sizeof(*g));
    g->send = send;
    g->ctx = ctx;
    g->rnd = rnd;
}

static void clear_link(bomb_t *g) {
    if (g->phase != BP_OVER) g->phase = BP_WAIT;
    g->holding = false;
    g->me_again = g->peer_again = false;
    g->out_n = g->out_head = 0;
}

void bomb_connected(bomb_t *g, bool initiator) {
    clear_link(g);
    g->phase = BP_WAIT;
    g->initiator = initiator;
    g->bad_ver = false;
    uint8_t m[2] = { 'H', BOMB_PROTO };
    enqueue(g, m, sizeof(m));
}

void bomb_lost(bomb_t *g) { clear_link(g); }

void bomb_on_msg(bomb_t *g, const uint8_t *m, size_t len) {
    if (!len) return;
    switch (m[0]) {
        case 'H': on_hello(g, m, len); break;
        case 'N': on_round(g, m, len); break;
        case 'T': on_throw(g, m, len); break;
        case 'B': on_boom(g, m, len); break;
        case 'R': on_again(g, m, len); break;
        default: break;
    }
}

void bomb_tick(bomb_t *g, uint32_t dt) {
    switch (g->phase) {
        case BP_COUNT:
            if (dt < g->delay_ms) {
                g->delay_ms -= dt;
            } else {
                g->delay_ms = 0;
                g->phase = BP_PLAY;
            }
            break;
        case BP_PLAY:
            g->elapsed_ms += dt;
            if (!g->holding) break;
            g->hold_ms += dt;
            if (dt < g->remain_ms) {
                g->remain_ms -= dt;
                break;
            }
            g->remain_ms = 0;  // 炸在我手里:对方得一分
            g->holding = false;
            if (g->score_peer < BOMB_WIN) g->score_peer++;
            g->decided = true;
            g->loser_me = true;
            g->phase = BP_BOOM;
            g->next_ms = BOMB_NEXT_MS;
            {
                uint8_t m[6] = { 'B' };
                put32(m + 1, g->match);
                m[5] = g->round;
                enqueue(g, m, sizeof(m));
            }
            event(g, BE_BOOM_ME);
            break;
        case BP_BOOM:
            if (!g->next_ms) break;  // 被连方:等主动方的 ROUND
            if (dt < g->next_ms) {
                g->next_ms -= dt;
                break;
            }
            g->next_ms = 0;
            if (!g->initiator) break;  // 被连方:等主动方的 ROUND(打完了也由它说,BOOM 可能没送到)
            if (bomb_over(g)) {
                send_round(g, BOMB_NOBODY, 0, 0);
                finish(g);
            } else {
                start_round(g, (uint8_t)(g->round + 1), 0);
            }
            break;
        default: break;
    }
}

bool bomb_press(bomb_t *g) {
    if (g->phase == BP_PLAY && g->holding && g->hold_ms >= BOMB_CATCH_MS) {
        g->holding = false;
        g->throws++;
        uint8_t m[10] = { 'T' };
        put32(m + 1, g->match);
        m[5] = g->round;
        put16(m + 6, g->throws);
        put16(m + 8, (uint16_t)g->remain_ms);
        enqueue(g, m, sizeof(m));
        event(g, BE_THROWN);
        return true;
    }
    if (g->phase == BP_OVER && !g->me_again) {
        g->me_again = true;
        uint8_t m[5] = { 'R' };
        put32(m + 1, g->match);
        enqueue(g, m, sizeof(m));
        maybe_again(g);
        return true;
    }
    return false;
}

void bomb_pump(bomb_t *g) {
    while (g->out_n && g->send(g->ctx, g->out[g->out_head], g->out_len[g->out_head])) {
        g->out_head = (uint8_t)((g->out_head + 1) % BOMB_OUTQ);
        g->out_n--;
    }
}

bool bomb_event(bomb_t *g, uint8_t *ev) {
    if (!g->ev_n) return false;
    *ev = g->ev[g->ev_head];
    g->ev_head = (uint8_t)((g->ev_head + 1) % BOMB_EVQ);
    g->ev_n--;
    return true;
}

uint8_t bomb_heat(const bomb_t *g) {
    uint32_t e = g->elapsed_ms > BOMB_HOT_MS ? BOMB_HOT_MS : g->elapsed_ms;
    return (uint8_t)(e * 255u / BOMB_HOT_MS);
}

uint32_t bomb_tick_gap(const bomb_t *g) { return 900u - bomb_heat(g) * 780u / 255u; }
