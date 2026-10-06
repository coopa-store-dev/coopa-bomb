// src/bomb_game.h —— 库巴传炸弹的一场比赛:回合、扔、接住冷却、倒计时、爆炸、比分、断线,和对方之间的消息。
// 纯 C:发送和随机数走回调(卡上是 kit_link_send / esp_random),宿主测试把两个实例接在一起(tests/duo.c)。
//
// 消息(第一字节是类型,多字节小端):
//   'H' HELLO  版本                                                              连上后双方各发一次
//   'N' ROUND  场号(4) 回合 主动方比分 被连方比分 炸弹给谁 引信(2) 倒数(2)          主动方:开一回合(给谁 = 2:这场打完了)
//   'T' THROW  场号(4) 回合 第几扔(2) 剩余毫秒(2)                                  扔的一方
//   'B' BOOM   场号(4) 回合                                                       炸在手里的一方
//   'R' AGAIN  场号(4)                                                            结束页按了 ●
// 主动方说了算:开回合、比分(ROUND 里带着)。只有拿着炸弹的一方能扔、能判爆炸,所以不会两边同时拿着。
// 断线:这一回合作废;连回来主动方重开(这一回合已经炸过就开下一回合)。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOMB_PROTO 1
#define BOMB_MSG_MAX 64
#define BOMB_WIN 3
#define BOMB_CATCH_MS 500
#define BOMB_FUSE_MIN 8000
#define BOMB_FUSE_MAX 20000
#define BOMB_NEXT_MS 2000
#define BOMB_COUNT_MS 3000
#define BOMB_HOT_MS 20000  // 过去这么久就最红、最快(只看过去多久)
#define BOMB_OUTQ 8
#define BOMB_EVQ 8
#define BOMB_NOBODY 2      // ROUND 里「炸弹给谁」= 这场打完了

typedef enum { BP_WAIT = 0, BP_COUNT, BP_PLAY, BP_BOOM, BP_OVER } bomb_phase_t;
typedef enum { BE_ROUND = 1, BE_GOT, BE_THROWN, BE_BOOM_ME, BE_BOOM_PEER, BE_OVER, BE_BAD_VER } bomb_ev_t;

typedef bool (*bomb_send_fn)(void *ctx, const void *msg, size_t len);  // false = 现在发不出去,过会儿再发
typedef uint32_t (*bomb_rnd_fn)(void);

typedef struct {
    bomb_send_fn send;
    void *ctx;
    bomb_rnd_fn rnd;
    bool initiator;
    bool bad_ver;
    uint8_t phase;         // bomb_phase_t
    uint32_t match;        // 场号(0 = 还没开过)
    uint8_t round;
    uint8_t score_me, score_peer;
    bool decided;          // 这一回合已经炸了
    bool holding;          // 炸弹在我这
    bool loser_me;         // 最近一次炸在我手里
    bool me_again, peer_again;
    uint16_t throws;       // 这一回合扔了几次
    uint32_t remain_ms;    // 我拿着时:引信还剩多少
    uint32_t elapsed_ms;   // 这一回合开始以后过去多久(画面、声音只看它)
    uint32_t hold_ms;      // 拿到多久了(接住冷却)
    uint32_t delay_ms;     // 倒数还剩
    uint32_t next_ms;      // 炸了以后还要等多久
    uint8_t out[BOMB_OUTQ][16];
    uint8_t out_len[BOMB_OUTQ], out_head, out_n;
    uint8_t ev[BOMB_EVQ], ev_head, ev_n;
} bomb_t;

void bomb_init(bomb_t *g, bomb_send_fn send, void *ctx, bomb_rnd_fn rnd);
void bomb_connected(bomb_t *g, bool initiator);  // 新的一次连接:发 HELLO
void bomb_lost(bomb_t *g);                       // 断了:这一回合作废,比分留着
void bomb_on_msg(bomb_t *g, const uint8_t *m, size_t len);
void bomb_tick(bomb_t *g, uint32_t dt_ms);
bool bomb_press(bomb_t *g);                      // ●:扔 / 再来一场;返回 true = 做了
void bomb_pump(bomb_t *g);                       // 把排着的消息发出去
bool bomb_event(bomb_t *g, uint8_t *ev);
bool bomb_over(const bomb_t *g);                 // 有一方赢够了
uint8_t bomb_heat(const bomb_t *g);              // 0..255:炸弹有多红、多抖
uint32_t bomb_tick_gap(const bomb_t *g);         // 嘀嗒间隔(毫秒)
