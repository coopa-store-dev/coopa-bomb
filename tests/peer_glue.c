// tests/peer_glue.c —— Mac 冒充对手(外壳的 tools/link_peer.py bomb)用的 C 接口:一个 bomb_t + 一个电脑(普通)。
// Mac 永远是主动连的一方;时间由 Python 推(peer_tick)。选人、扔、放招、砸笼子都由电脑在 peer_tick 里自己做,
// 所以 peer_holding() 一直是 0(link_peer.py 不用再自己按)。
// link_peer.py 只编 src/bomb_game.c 和这个文件,所以其余的规则源文件在这里直接包含进来
// (tests/Makefile 编 test_peer_glue 时就不再单独列它们)。发出去的消息先进这里的队列,由 Python 取走。
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bomb_game.h"
#include "bomb_bot.h"

#include "../src/bomb_bot.c"
#include "../src/bomb_fighters.c"

#define PEER_Q 32
static bomb_t s_g;
static bomb_bot_t s_bot;
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
    bomb_bot_init(&s_bot, BOT_NORMAL, rnd);
}

void peer_connected(void) {
    s_head = s_cnt = 0;
    bomb_connected(&s_g, true);
}

void peer_lost(void) { bomb_lost(&s_g); }
void peer_msg(const uint8_t *m, size_t len) { bomb_on_msg(&s_g, m, len); }
void peer_tick(uint32_t dt) {
    if (s_g.phase == BP_PICK && !s_g.me_picked) bomb_pick(&s_g, (uint8_t)(rnd() % BF_COUNT));
    if (s_g.phase == BP_OVER && !s_g.me_again) bomb_press(&s_g);  // 打完了:直接要「再来一场」
    bomb_tick(&s_g, dt);
    bomb_seen_t s;
    bomb_seen(&s_g, &s);
    int k = bomb_bot_step(&s_bot, &s, dt);
    if (k >= 0) bomb_key(&s_g, (uint8_t)k);
}
int peer_press(void) { return 0; }  // 电脑自己按(见 peer_tick)
int peer_holding(void) { return 0; }
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
    return snprintf(buf, len, "phase=%u hold=%u me=%u peer=%u round=%u match=%08lx char=%u/%u en=%u/%u", (unsigned)s_g.phase,
                    (unsigned)s_g.holding, (unsigned)s_g.score_me, (unsigned)s_g.score_peer, (unsigned)s_g.round,
                    (unsigned long)s_g.match, (unsigned)s_g.me_char, (unsigned)s_g.peer_char,
                    (unsigned)s_g.en_me, (unsigned)s_g.en_peer);
}
