// src/bomb_ui.h —— 库巴传炸弹的画面:首页、提示页(找对方 / 断线)、选人页、游戏页(顶栏、炸弹、瞄准条、提示、特写)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOMB_UI_TEXT 64

typedef enum { CUT_NONE = 0, CUT_SPECIAL, CUT_SUPER } bomb_cut_t;

typedef struct {
    // 顶栏
    char score[BOMB_UI_TEXT];
    uint8_t en_me, en_peer;   // 0..4
    bool blink;               // 满 4 格的那排闪(这一帧亮不亮)
    bool lid, taiji;          // 我身上的锅盖 / 太极架势
    uint8_t glasses;          // 老花镜还管几次
    // 中间
    char big[BOMB_UI_TEXT];
    uint32_t big_color;
    bool show_bomb;
    int bomb_x, bomb_y;       // 放大后炸弹左上角
    uint8_t heat;             // 0..255:炸弹染红多少
    bool spark;               // 引信火花这一帧亮不亮
    bool zap;                 // 火箭快递:电火花
    bool cage;                // 关在铁笼里
    uint8_t decoy;            // 影分身:0 没有 / 1 选中左边 / 2 选中右边
    bool boom;                // 画爆炸
    int face;                 // 大头像(被炸黑 / 赢家):角色,-1 = 不画
    bool sooty;               // 大头像染黑
    bool flash;               // 屏幕闪一下
    // 瞄准条
    bool aim;
    uint16_t aim_pos, green_half, perfect_half;
    char hint[BOMB_UI_TEXT];
    // 特写
    uint8_t cut;              // bomb_cut_t
    uint8_t cut_char;
    const char *cut_name;
    uint32_t cut_t;           // 特写开始后多久(毫秒)
} bomb_view_t;

void bomb_ui_title(uint8_t sel, unsigned wins);
void bomb_ui_note(const char *big, uint32_t color, const char *l1, const char *l2, const char *l3);
// 选人页:sel 0..3 = 角色、4 = 回首页;crowns 位 = 通关过的角色;title / hint / peer 是三行字。
void bomb_ui_pick(uint8_t sel, bool picked, uint8_t crowns, const char *title, const char *hint, const char *peer);
void bomb_ui_game(uint8_t me_char, uint8_t peer_char);
void bomb_ui_game_set(const bomb_view_t *v);

#define BOMB_CUT_SUPER_MS 1200
#define BOMB_CUT_SPECIAL_MS 600
