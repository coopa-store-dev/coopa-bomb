// src/bomb_bot.h —— 电脑对手的脑子:只看 bomb_seen_t(玩家看得见的),不看引信还剩多少。
// 接到炸弹先「反应」一会儿,然后决定放招还是瞄准;瞄准时对准中点按,按早按晚有个随机误差(难度越高越准)。
// 和电脑打时它驱动对面那个 bomb_t;宿主测试拿它来调数值;Mac 对手(tests/peer_glue.c)也用它。
#pragma once

#include <stdint.h>

#include "bomb_game.h"

typedef enum { BOT_EASY = 0, BOT_NORMAL, BOT_HARD, BOT_LEVELS } bomb_bot_level_t;

typedef struct {
    uint8_t level;
    uint32_t (*rnd)(void);
    bool ready;        // 这次拿着已经反应完了
    int32_t wait_ms;   // 反应 / 砸笼子的间隔还剩多久
    int32_t err_ms;    // 这一下打算按早(+)还是按晚(-)多少毫秒
    bool decoy_done;   // 影分身已经选过了
    uint32_t casts[2]; // 统计:放了几次必杀 / 超必杀
    uint32_t presses;  // 统计:瞄准按了几次
} bomb_bot_t;

void bomb_bot_init(bomb_bot_t *b, uint8_t level, uint32_t (*rnd)(void));
// 每帧调一次;返回要按的键(bomb_key_t),-1 = 这帧不按。
int bomb_bot_step(bomb_bot_t *b, const bomb_seen_t *s, uint32_t dt);
