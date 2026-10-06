// tests/peer_glue.c —— Mac 冒充对手(外壳的 tools/link_peer.py bomb)用的 C 接口:一个 bomb_t。
// Mac 永远是主动连的一方;时间由 Python 推(peer_tick)。发出去的消息先进这里的队列,由 Python 取走。
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bomb_game.h"

#define PEER_Q 32
static bomb_t s_g;
static uint8_t s_q[PEER_Q][BOMB_MSG_MAX];
static size_t s_len[PEER_Q];
static int s_head, s_cnt;
static uint32_t s_rng = 4242;

static bool to_py(void *ctx, const void *m, size_t len) {
    (void)ctx;
    if (s_cnt == PEER_Q) return false;
    int i = (s_head + s_cnt) % PEER_Q;
    memcpy(s_q[i], m, len);
    s_len[i] = len;
    s_cnt++;
    return true;
}

static uint32_t rnd(void) {
    s_rng = s_rng * 1664525u + 1013904223u;
    return s_rng >> 8;
}

void peer_init(uint32_t seed) {
    s_rng = seed ? seed : 4242;
    bomb_init(&s_g, to_py, NULL, rnd);
}

void peer_connected(void) {
    s_head = s_cnt = 0;
    bomb_connected(&s_g, true);
}

void peer_lost(void) { bomb_lost(&s_g); }
void peer_msg(const uint8_t *m, size_t len) { bomb_on_msg(&s_g, m, len); }
void peer_tick(uint32_t dt) { bomb_tick(&s_g, dt); }
int peer_press(void) { return bomb_press(&s_g); }
int peer_holding(void) { return s_g.holding && s_g.hold_ms >= BOMB_CATCH_MS; }
int peer_over(void) { return s_g.phase == BP_OVER; }

size_t peer_take(uint8_t *buf) {
    bomb_pump(&s_g);
    if (!s_cnt) return 0;
    size_t len = s_len[s_head];
    memcpy(buf, s_q[s_head], len);
    s_head = (s_head + 1) % PEER_Q;
    s_cnt--;
    return len;
}

int peer_status(char *buf, size_t len) {
    return snprintf(buf, len, "phase=%u hold=%u me=%u peer=%u round=%u match=%08lx", (unsigned)s_g.phase,
                    (unsigned)s_g.holding, (unsigned)s_g.score_me, (unsigned)s_g.score_peer, (unsigned)s_g.round,
                    (unsigned long)s_g.match);
}
