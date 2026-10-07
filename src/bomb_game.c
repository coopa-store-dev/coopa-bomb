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

static void send_throw(bomb_t *g, uint8_t skill, bool perfect);

static uint8_t add_en(uint8_t en, uint8_t n) { return en + n > BOMB_EN_MAX ? BOMB_EN_MAX : (uint8_t)(en + n); }

bool bomb_over(const bomb_t *g) { return g->score_me >= BOMB_WIN || g->score_peer >= BOMB_WIN; }

static void finish(bomb_t *g) {
    g->phase = BP_OVER;
    g->holding = false;
    event(g, BE_OVER);
}

// 拿到一次炸弹就重新算的东西(扔出去 / 新回合时清掉)。
static void clear_hold(bomb_t *g) {
    g->hold_ms = 0;
    g->catch_ms = BOMB_CATCH_MS;
    g->aim_x1000 = 0;
    g->miss_ms = g->freeze_ms = g->guard_ms = 0;
    g->cage_left = g->decoy = 0;
    g->decoy_sel = 1;
    g->nag = g->fast = g->pushing = false;
}

// 两边开一回合的同一段:炸弹给谁、引信多长、要不要先倒数。
static void begin(bomb_t *g, uint8_t round, bool mine, uint16_t fuse, uint16_t delay) {
    g->round = round;
    g->decided = false;
    g->holding = mine;
    g->remain_ms = fuse;
    g->elapsed_ms = 0;
    clear_hold(g);
    g->hold_ms = BOMB_CATCH_MS;  // 开局拿到的不用等「接住」
    g->lid = g->taiji = false;
    g->glasses_left = 0;
    g->throws = 0;
    g->delay_ms = delay;
    g->next_ms = 0;
    g->phase = delay ? BP_COUNT : BP_PLAY;
    event(g, BE_ROUND);
}

static void send_round(bomb_t *g, uint8_t holder, uint16_t fuse, uint16_t delay) {
    uint8_t m[15] = { 'N' };
    put32(m + 1, g->match);
    m[5] = g->round;
    m[6] = g->score_me;    // 主动方比分
    m[7] = g->score_peer;  // 被连方比分
    m[8] = holder;         // 0 主动方 / 1 被连方 / 2 这场打完了
    put16(m + 9, fuse);
    put16(m + 11, delay);
    m[13] = g->me_char;    // 主动方角色
    m[14] = g->peer_char;  // 被连方角色
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

// 新的一场(场号已经在选人页定好):比分、能量清零,角色锁定。
static void new_match(bomb_t *g) {
    g->selecting = false;
    g->score_me = g->score_peer = 0;
    g->en_me = g->en_peer = 0;
    g->me_again = g->peer_again = false;
    start_round(g, 1, BOMB_COUNT_MS);
}

static void send_select(bomb_t *g) {
    uint8_t m[5] = { 'S' };
    put32(m + 1, g->match);
    enqueue(g, m, sizeof(m));
}

static void send_pick(bomb_t *g) {
    uint8_t m[6] = { 'P' };
    put32(m + 1, g->match);
    m[5] = g->me_picked ? g->me_char : BOMB_NOCHAR;
    enqueue(g, m, sizeof(m));
}

// 换了一场(新场号):比分、能量、选人都从头来。
static void reset_match(bomb_t *g, uint32_t match) {
    g->match = match;
    g->score_me = g->score_peer = 0;
    g->en_me = g->en_peer = 0;
    g->round = 0;
    g->decided = false;
    g->me_picked = g->peer_picked = false;
    g->peer_char = BOMB_NOCHAR;
    g->me_again = g->peer_again = false;
}

static void to_pick(bomb_t *g) {
    g->selecting = true;
    g->holding = false;
    g->phase = BP_PICK;
    event(g, BE_PICK);
}

// 主动方:定下一场的场号,进选人页。
static void enter_select(bomb_t *g) {
    uint32_t old = g->match, m;
    do m = g->rnd() | 1u;
    while (m == old);
    reset_match(g, m);
    to_pick(g);
    send_select(g);
}

static void maybe_start(bomb_t *g) {
    if (g->initiator && g->phase == BP_PICK && g->me_picked && g->peer_picked) new_match(g);
}

// 主动方:对方连上来了。没开过就开新的一场;打完了就告诉对方结果;否则重开这一回合(炸过了就开下一回合)。
static void resume(bomb_t *g) {
    if (!g->match) {
        enter_select(g);
    } else if (g->selecting) {  // 选人选到一半断的:同一个场号接着选
        to_pick(g);
        send_select(g);
        if (g->me_picked) send_pick(g);
    } else if (bomb_over(g)) {
        send_round(g, BOMB_NOBODY, 0, 0);
        finish(g);
    } else {
        start_round(g, g->decided ? (uint8_t)(g->round + 1) : g->round, BOMB_COUNT_MS);
    }
}

static void maybe_again(bomb_t *g) {
    if (g->initiator && g->phase == BP_OVER && g->me_again && g->peer_again) enter_select(g);
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

static void on_select(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 5 || g->initiator) return;
    uint32_t match = get32(m + 1);
    if (!match) return;
    if (match != g->match) reset_match(g, match);
    to_pick(g);
    if (g->me_picked) send_pick(g);  // 同一场断线重连:再告诉一次
}

static void on_pick(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 6 || g->phase != BP_PICK || get32(m + 1) != g->match) return;
    uint8_t f = m[5];
    if (f >= BF_COUNT && f != BOMB_NOCHAR) return;
    g->peer_char = f;
    g->peer_picked = f != BOMB_NOCHAR;
    event(g, BE_PEER_PICK);
    maybe_start(g);
}

static void on_round(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 15 || g->initiator) return;
    uint8_t holder = m[8];
    uint16_t fuse = get16(m + 9), delay = get16(m + 11);
    if (holder > BOMB_NOBODY || m[6] > BOMB_WIN || m[7] > BOMB_WIN || delay > BOMB_COUNT_MS || !m[5]) return;
    if (holder != BOMB_NOBODY && (fuse < BOMB_FUSE_MIN || fuse > BOMB_FUSE_MAX)) return;
    if (m[13] >= BF_COUNT || m[14] >= BF_COUNT) return;
    uint32_t match = get32(m + 1);
    if (!match) return;
    if (match != g->match) reset_match(g, match);
    g->selecting = false;
    g->peer_char = m[13];
    g->me_char = m[14];
    g->me_picked = g->peer_picked = true;
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
    if (len < 15 || get32(m + 1) != g->match || m[5] != g->round || g->holding) return;
    uint16_t left = get16(m + 8), elapsed = get16(m + 13);
    if (left > BOMB_FUSE_MAX || m[10] >= SK_COUNT || m[11] > BOMB_EN_MAX || !playing(g)) return;
    uint8_t sk = m[10];
    g->holding = true;
    g->remain_ms = left;
    clear_hold(g);
    g->throws = get16(m + 6);
    g->en_peer = m[11];
    g->elapsed_ms = elapsed;  // 对齐对方的红度(卡顿时两边的时钟会差开)
    event(g, BE_GOT);
    if (g->taiji) {  // 太极架势:停一下就推回去,对方的招作废
        g->taiji = false;
        g->pushing = true;
        g->freeze_ms = BOMB_TAIJI_MS;
        event(g, BE_TAIJI);
        return;
    }
    g->freeze_ms = bomb_skill_freeze(sk);
    g->skill_in = sk;
    if (sk == SK_SLING) g->catch_ms = BOMB_SLING_CATCH_MS;
    if (sk == SK_ROCKET) g->fast = true;
    if (sk == SK_CAGE) g->cage_left = BOMB_CAGE_HITS;
    if (sk == SK_NAG) g->nag = true;
    if (sk == SK_DECOY) g->decoy = (uint8_t)(1 + (g->rnd() & 1));
    if (bomb_skill_hits(sk)) g->en_me = add_en(g->en_me, 1);
    if (sk) event(g, BE_SKILL_IN);
}

static void on_boom(bomb_t *g, const uint8_t *m, size_t len) {
    if (len < 6 || get32(m + 1) != g->match || m[5] != g->round || g->holding || !playing(g)) return;
    if (g->score_me < BOMB_WIN) g->score_me++;
    g->en_peer = add_en(g->en_peer, BOMB_EN_LOSE);
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
    g->me_char = g->peer_char = BOMB_NOCHAR;
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
        case 'S': on_select(g, m, len); break;
        case 'P': on_pick(g, m, len); break;
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
            if (g->freeze_ms) {  // 特写 / 闪字:引信不烧;多出来的时间下一帧再算
                g->freeze_ms = g->freeze_ms > dt ? g->freeze_ms - dt : 0;
                if (!g->freeze_ms && g->pushing) send_throw(g, SK_TAIJI_PUSH, false);
                break;
            }
            g->hold_ms += dt;
            if (g->hold_ms >= g->catch_ms && !g->cage_left) {
                bomb_aim_t a = bomb_aim_now(g);
                g->aim_x1000 = (g->aim_x1000 + dt * a.speed_x1000) % 2000000u;
            }
            g->miss_ms = g->miss_ms > dt ? g->miss_ms - dt : 0;
            g->guard_ms = g->guard_ms > dt ? g->guard_ms - dt : 0;
            {
                uint32_t burn = g->fast ? 2 * dt : dt;
                if (burn < g->remain_ms) {
                    g->remain_ms -= burn;
                    break;
                }
            }
            if (g->lid) {  // 锅盖:崩回去,对方只剩 3 秒
                g->lid = false;
                g->remain_ms = BOMB_LID_MS;
                event(g, BE_LID);
                send_throw(g, SK_LID_BOUNCE, false);
                break;
            }
            g->remain_ms = 0;  // 炸在我手里:对方得一分
            g->holding = false;
            if (g->score_peer < BOMB_WIN) g->score_peer++;
            g->en_me = add_en(g->en_me, BOMB_EN_LOSE);
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

bool bomb_can_throw(const bomb_t *g) {
    return g->phase == BP_PLAY && g->holding && !g->freeze_ms && g->hold_ms >= g->catch_ms && !g->miss_ms &&
           !g->cage_left && !g->decoy && !g->guard_ms;
}

uint16_t bomb_aim_pos(const bomb_t *g) {
    uint32_t u = g->aim_x1000 / 1000u;
    return (uint16_t)(u < 1000 ? u : 2000 - u);
}

bomb_aim_t bomb_aim_now(const bomb_t *g) { return bomb_aim_params(bomb_heat(g), g->nag, g->glasses_left > 0); }

// 扔出去(瞄准扔中,或者放招):把剩下的引信、招式、扔完的能量、回合过去多久带过去。
static void send_throw(bomb_t *g, uint8_t skill, bool perfect) {
    g->holding = false;
    clear_hold(g);
    g->throws++;
    uint8_t m[15] = { 'T' };
    put32(m + 1, g->match);
    m[5] = g->round;
    put16(m + 6, g->throws);
    put16(m + 8, (uint16_t)g->remain_ms);
    m[10] = skill;
    m[11] = g->en_me;
    m[12] = perfect;
    put16(m + 13, (uint16_t)(g->elapsed_ms > 0xFFFFu ? 0xFFFFu : g->elapsed_ms));
    enqueue(g, m, sizeof(m));
    event(g, BE_THROWN);
}

static bool aim_throw(bomb_t *g) {
    bomb_aim_t a = bomb_aim_now(g);
    uint16_t pos = bomb_aim_pos(g);
    uint16_t off = pos > BOMB_AIM_MID ? pos - BOMB_AIM_MID : BOMB_AIM_MID - pos;
    if (off > a.green_half) {  // 扔歪:撞墙弹回来
        g->miss_ms = BOMB_MISS_MS;
        event(g, BE_MISS);
        return true;
    }
    bool perfect = off <= a.perfect_half;
    if (g->glasses_left) {  // 戴着老花镜扔的不加能量(免得连着完美滚雪球)
        g->glasses_left--;
    } else if (perfect) {
        g->en_me = add_en(g->en_me, BOMB_EN_PERFECT);
        event(g, BE_PERFECT);
    }
    send_throw(g, SK_NONE, perfect);
    return true;
}

bool bomb_press(bomb_t *g) { return bomb_key(g, BK_OK); }

// 现在能砸笼子 / 选影分身(冻结、接住冷却、弹回都过了)
static bool hands_free(const bomb_t *g) {
    return g->phase == BP_PLAY && g->holding && !g->freeze_ms && g->hold_ms >= g->catch_ms && !g->miss_ms;
}

static bool cast(bomb_t *g, bool super) {
    uint8_t sk = bomb_skill_of(g->me_char, super);
    if (!sk || !bomb_can_throw(g) || g->en_me < bomb_skill_cost(sk)) return false;
    g->en_me = (uint8_t)(g->en_me - bomb_skill_cost(sk));
    if (sk == SK_LID) g->lid = true;
    if (sk == SK_TAIJI) g->taiji = true;
    g->skill_out = sk;
    event(g, BE_SKILL_OUT);
    send_throw(g, sk, false);
    if (sk == SK_GLASSES) g->glasses_left = BOMB_GLASSES;  // 扔完才戴上:这一扔不算
    return true;
}

bool bomb_key(bomb_t *g, uint8_t key) {
    if (g->cage_left && g->phase == BP_PLAY && g->holding && !g->freeze_ms) {  // 砸笼子不用等「接住」
        if (key != BK_OK) return false;
        g->cage_left--;
        event(g, BE_CAGE_HIT);
        if (!g->cage_left) {
            g->guard_ms = BOMB_GUARD_MS;
            event(g, BE_CAGE_OPEN);
        }
        return true;
    }
    if (g->decoy && hands_free(g)) {
        if (key != BK_OK) {
            g->decoy_sel = key == BK_UP ? 1 : 2;
            return true;
        }
        bool real = g->decoy_sel == g->decoy;
        g->decoy = 0;
        if (!real) {
            g->miss_ms = BOMB_DECOY_MS;
            event(g, BE_DECOY_PUFF);
            return true;
        }
        // 选对了:照常瞄准(下面)
    }
    if (key != BK_OK) return cast(g, key == BK_DOWN);
    if (bomb_can_throw(g)) return aim_throw(g);
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

bool bomb_pick(bomb_t *g, uint8_t fighter) {
    if (g->phase != BP_PICK || g->me_picked || fighter >= BF_COUNT) return false;
    g->me_char = fighter;
    g->me_picked = true;
    send_pick(g);
    maybe_start(g);
    return true;
}

bool bomb_unpick(bomb_t *g) {
    if (g->phase != BP_PICK || !g->me_picked) return false;
    g->me_picked = false;
    send_pick(g);
    return true;
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
