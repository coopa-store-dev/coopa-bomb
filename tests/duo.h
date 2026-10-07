// tests/duo.h —— 两个 bomb_t 用假管道接起来(a 是主动连的一方)。管道就是 kit_link:一次连接之内不丢不乱,
// 发送队列 8 条满了返回 false,断线时排着的都没了。时间由 run() 推:两边每 10 ms tick 一次,然后把消息送到。
#pragma once

#include "bomb_game.h"

#define PIPE_N 64
typedef struct {
    uint8_t q[PIPE_N][BOMB_MSG_MAX];
    size_t len[PIPE_N];
    int head, n, cap;
    bool up;
} pipe_t;

typedef struct {
    bomb_t a, b;
    pipe_t ab, ba;
} duo_t;

extern duo_t D;

uint32_t duo_rnd(void);   // 确定的伪随机(LCG)
void duo_seed(uint32_t s);
bool duo_send(void *ctx, const void *m, size_t len);
void duo_init(void);
void duo_connect(void);
void duo_drop(void);
void duo_pick(uint8_t a, uint8_t b);  // 两边在选人页确定(a 是主动方)并送达
void duo_begin(void);                 // init + 连上 + 选人(库巴 对 爸爸):两边进倒数
void flow(void);
void run(uint32_t ms);    // 两边各走 ms 毫秒(每步 10 ms,每步之后 flow)
void mirror(void);        // 两边场号一样、比分镜像(assert)
// 推时间(两边,或只推 g 自己)直到 g 能扔、箭头在 [lo, hi];5 秒内到不了就 assert
void aim_at(bomb_t *g, uint16_t lo, uint16_t hi);
void aim_solo(bomb_t *g, uint16_t lo, uint16_t hi);
void throw_hit(bomb_t *g);      // 瞄准绿区(不在完美区)按 ●,断言扔出去了(不送达)
void throw_perfect(bomb_t *g);  // 瞄准完美区按 ●
#define HIT_LO 400  // 在最小的绿区里(念叨:半宽 105)、最大的完美区外(老花镜:半宽 60)
#define HIT_HI 430
#define PERFECT_LO 485
#define PERFECT_HI 515
bomb_t *holder(void);     // 拿着炸弹的那边(没人拿返回 NULL)
bomb_t *other(bomb_t *g);
