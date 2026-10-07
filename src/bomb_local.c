// src/bomb_local.c —— 见 bomb_local.h。
#include "bomb_local.h"

#include <string.h>

static bool pipe_send(void *ctx, const void *m, size_t len) {
    bomb_pipe_t *p = ctx;
    if (p->n == BOMB_LOCAL_Q || len > sizeof(p->q[0])) return false;  // 满了:bomb_pump 下次再发
    uint8_t i = (uint8_t)((p->head + p->n) % BOMB_LOCAL_Q);
    memcpy(p->q[i], m, len);
    p->len[i] = (uint8_t)len;
    p->n++;
    return true;
}

static bool deliver(bomb_pipe_t *p, bomb_t *to) {
    if (!p->n) return false;
    uint8_t m[16], len = p->len[p->head];
    memcpy(m, p->q[p->head], len);
    p->head = (uint8_t)((p->head + 1) % BOMB_LOCAL_Q);
    p->n--;
    bomb_on_msg(to, m, len);
    return true;
}

void bomb_local_init(bomb_local_t *l, uint32_t (*rnd)(void)) {
    memset(l, 0, sizeof(*l));
    bomb_init(&l->me, pipe_send, &l->to_cpu, rnd);
    bomb_init(&l->cpu, pipe_send, &l->to_me, rnd);
    bomb_connected(&l->me, true);
    bomb_connected(&l->cpu, false);
}

void bomb_local_flow(bomb_local_t *l) {
    for (bool any = true; any;) {
        any = false;
        bomb_pump(&l->me);
        bomb_pump(&l->cpu);
        while (deliver(&l->to_cpu, &l->cpu)) any = true;
        while (deliver(&l->to_me, &l->me)) any = true;
    }
}
