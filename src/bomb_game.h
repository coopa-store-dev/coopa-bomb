// src/bomb_game.h —— 库巴传炸弹的一场比赛:回合、扔、接住冷却、倒计时、爆炸、比分、断线,和对方之间的消息。
// 纯 C:发送和随机数走回调(卡上是 kit_link_send / esp_random),宿主测试把两个实例接在一起(tests/duo.c)。
//
// 消息(第一字节是类型,多字节小端):
//   'H' HELLO  版本                                                              连上后双方各发一次
//   'S' SELECT 场号(4)                                                           主动方:进选人页(新的一场的场号)
//   'P' PICK   场号(4) 角色(0xFF = 撤回)                                          选人页确定 / 撤回;连回来重发
//   'N' ROUND  场号(4) 回合 主动方比分 被连方比分 炸弹给谁 引信(2) 倒数(2) 主动方角色 被连方角色
//                                                                                主动方:开一回合(给谁 = 2:这场打完了)
//   'T' THROW  场号(4) 回合 第几扔(2) 剩余毫秒(2) 招式 扔完的能量 完美 回合过去多久(2)  扔的一方
//   'B' BOOM   场号(4) 回合                                                       炸在手里的一方
//   'R' AGAIN  场号(4)                                                            结束页按了 ●
// 主动方说了算:选人页的场号、什么时候开打、开回合、比分(ROUND 里带着)。角色一场之内锁定(开打后不收 PICK)。只有拿着炸弹的一方能扔、能判爆炸,所以不会两边同时拿着。
// 断线:这一回合作废;连回来主动方重开(这一回合已经炸过就开下一回合)。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bomb_fighters.h"

#define BOMB_PROTO 2
#define BOMB_MSG_MAX 64
#define BOMB_WIN 3
#define BOMB_CATCH_MS 500
#define BOMB_FUSE_MIN 11000
#define BOMB_FUSE_MAX 26000
#define BOMB_NEXT_MS 2000
#define BOMB_COUNT_MS 3000
#define BOMB_HOT_MS 20000  // 过去这么久就最红、最快(只看过去多久)
#define BOMB_OUTQ 8
#define BOMB_EVQ 16  // 砸笼子一帧能按好几下
#define BOMB_EN_MAX 4
#define BOMB_EN_PERFECT 1  // 完美扔中加几格(普通扔中不加)
#define BOMB_EN_LOSE 2     // 输一回合加几格
#define BOMB_MISS_MS 600   // 扔歪弹回:这么久扔不了
#define BOMB_SLING_CATCH_MS 1500  // 被弹弓砸晕:接住要等这么久
#define BOMB_DECOY_MS 1200 // 影分身选到假的:这么久扔不了
#define BOMB_CAGE_HITS 8   // 铁笼要砸几下
#define BOMB_GUARD_MS 300  // 砸开笼子后这么久不收 ●
#define BOMB_LID_MS 3000   // 锅盖崩回去,对方只剩这么久
#define BOMB_TAIJI_MS 400  // 太极架势接住到推回去
#define BOMB_GLASSES 3     // 老花镜管几次瞄准
#define BOMB_NOBODY 2      // ROUND 里「炸弹给谁」= 这场打完了
#define BOMB_NOCHAR 0xFF   // 还没选人 / PICK 里的「撤回」

typedef enum { BP_WAIT = 0, BP_COUNT, BP_PLAY, BP_BOOM, BP_OVER, BP_PICK } bomb_phase_t;  // 旧值不能改(状态行)
typedef enum { BK_OK = 0, BK_UP, BK_DOWN } bomb_key_t;
typedef enum {
    BE_ROUND = 1, BE_GOT, BE_THROWN, BE_BOOM_ME, BE_BOOM_PEER, BE_OVER, BE_BAD_VER,
    BE_PICK,       // 进选人页
    BE_PEER_PICK,  // 对方确定 / 撤回了
    BE_MISS,       // 扔歪了,弹回来
    BE_PERFECT,    // 完美(和 BE_THROWN 一起)
    BE_SKILL_OUT,  // 我放了招(skill_out),放特写
    BE_SKILL_IN,   // 带招的炸弹到了(skill_in),放特写 / 闪字
    BE_CAGE_HIT,   // 砸了笼子一下
    BE_CAGE_OPEN,  // 笼子砸开了
    BE_DECOY_PUFF, // 选到假的,「噗」
    BE_LID,        // 锅盖把炸弹崩回去了
    BE_TAIJI,      // 太极架势接住了,马上推回去
} bomb_ev_t;

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
    bool selecting;        // 在选人(主动方断线重连时据此重发 SELECT)
    uint8_t me_char, peer_char;  // bomb_fighter_t;BOMB_NOCHAR = 还没有
    bool me_picked, peer_picked;
    uint8_t en_me, en_peer;      // 能量 0..BOMB_EN_MAX
    uint16_t throws;       // 这一回合扔了几次
    uint32_t remain_ms;    // 我拿着时:引信还剩多少
    uint32_t elapsed_ms;   // 这一回合开始以后过去多久(画面、声音只看它)
    uint32_t hold_ms;      // 拿到多久了(接住冷却)
    uint32_t catch_ms;     // 这次接住要等多久
    uint32_t aim_x1000;    // 箭头:0..1999999,前一半往右走、后一半往回(×1000 免得慢的时候走不动)
    uint32_t miss_ms;      // 扔歪弹回 / 选到假炸弹:还剩多久扔不了
    uint32_t freeze_ms;    // 特写 / 闪字冻结:引信不烧、按键不算
    uint32_t guard_ms;     // 砸开笼子后不收 ● 的时间
    uint8_t cage_left;     // 铁笼还要砸几下
    uint8_t decoy;         // 影分身:0 没有 / 1 左边是真的 / 2 右边是真的
    uint8_t decoy_sel;     // 选中的那个(1 左 / 2 右)
    uint8_t glasses_left;  // 老花镜还管几次
    bool nag, fast;        // 这次拿着:被念叨 / 火箭快递(引信两倍快)
    bool lid, taiji;       // 这一回合身上的锅盖 / 太极架势
    bool pushing;          // 太极接住了,冻结完就推回去
    uint8_t skill_in, skill_out;  // 最近收到 / 放出的招(画面用)
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
bool bomb_key(bomb_t *g, uint8_t key);           // bomb_key_t;返回 true = 做了点什么(扔、扔歪、再来一场……)
bool bomb_press(bomb_t *g);                      // = bomb_key(g, BK_OK)
bool bomb_can_throw(const bomb_t *g);            // 现在按 ● 会瞄准判定、▲▼ 能放招(拿着、没冻结、接住过了、没在弹回、
                                                 // 不在笼子里、没在选影分身、砸开笼子后的 0.3 秒过了)
uint16_t bomb_aim_pos(const bomb_t *g);          // 箭头 0..1000
bomb_aim_t bomb_aim_now(const bomb_t *g);
bool bomb_pick(bomb_t *g, uint8_t fighter);      // 选人页确定;返回 true = 收下
bool bomb_unpick(bomb_t *g);                     // 确定后撤回
void bomb_pump(bomb_t *g);                       // 把排着的消息发出去
bool bomb_event(bomb_t *g, uint8_t *ev);
bool bomb_over(const bomb_t *g);                 // 有一方赢够了
// 玩家(和电脑)看得见的东西:没有引信还剩多少。电脑(bomb_bot)只拿这个,拿不到 bomb_t。
typedef struct {
    uint8_t phase;
    bool holding, can_throw, frozen;  // frozen = 特写 / 闪字中
    uint8_t cage_left;
    bool decoy;
    uint16_t aim_pos;
    int8_t aim_dir;       // 箭头往哪走:+1 右 / -1 左
    bomb_aim_t aim;
    uint8_t heat, en_me, en_peer, me_char, peer_char;
    uint32_t hold_ms;
} bomb_seen_t;
void bomb_seen(const bomb_t *g, bomb_seen_t *s);

uint8_t bomb_heat(const bomb_t *g);              // 0..255:炸弹有多红、多抖
uint32_t bomb_tick_gap(const bomb_t *g);         // 嘀嗒间隔(毫秒)
