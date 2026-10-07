// src/bomb_local.h —— 和电脑打:卡上开两个 bomb_t(me 是主动方、cpu 是被连方),用内存里的管道接起来。
// 规则、招式、冻结和联机完全是同一套代码;只是消息不走蓝牙。管道和 kit_link 一样:不丢、不乱。
#pragma once

#include "bomb_game.h"

#define BOMB_LOCAL_Q 16

typedef struct {
    uint8_t q[BOMB_LOCAL_Q][16];
    uint8_t len[BOMB_LOCAL_Q];
    uint8_t head, n;
} bomb_pipe_t;

typedef struct {
    bomb_t me, cpu;
    bomb_pipe_t to_cpu, to_me;
} bomb_local_t;

void bomb_local_init(bomb_local_t *l, uint32_t (*rnd)(void));  // 两个 bomb_t 接好、连上(还要 flow 一下)
void bomb_local_flow(bomb_local_t *l);                          // 把两边排着的消息互相送到,直到没有
