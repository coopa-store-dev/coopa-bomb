// tests/test_peer_glue.c —— Mac 对端的 C 接口(peer_glue.c)当主动方,和 duo.c 的 b 打完一场。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "duo.h"

void peer_init(uint32_t seed);
void peer_connected(void);
void peer_msg(const uint8_t *m, size_t len);
void peer_tick(uint32_t dt);
int peer_press(void);
int peer_holding(void);
int peer_over(void);
size_t peer_take(uint8_t *buf);
int peer_status(char *buf, size_t len);

int main(void) {
    duo_init();
    D.ba.up = true;
    peer_init(7);
    peer_connected();
    bomb_connected(&D.b, false);
    for (int step = 0; step < 200000 && !(peer_over() && D.b.phase == BP_OVER); step++) {
        peer_tick(10);
        bomb_tick(&D.b, 10);
        if (D.b.phase == BP_PICK && !D.b.me_picked) bomb_pick(&D.b, BF_GRANDPA);
        if (peer_holding() && step % 97 == 0) peer_press();
        if (D.b.holding && D.b.hold_ms >= 900) bomb_press(&D.b);
        uint8_t m[BOMB_MSG_MAX];
        size_t n;
        while ((n = peer_take(m)) > 0) bomb_on_msg(&D.b, m, n);
        bomb_pump(&D.b);
        while (D.ba.n) {
            int i = D.ba.head;
            D.ba.head = (D.ba.head + 1) % PIPE_N;
            D.ba.n--;
            peer_msg(D.ba.q[i], D.ba.len[i]);
        }
    }
    char st[160];
    peer_status(st, sizeof(st));
    printf("  %s\n", st);
    assert(peer_over() && D.b.phase == BP_OVER && bomb_over(&D.b));
    puts("test_peer_glue: ok");
    return 0;
}
