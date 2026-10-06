// src/bomb_ui.h —— 库巴传炸弹的画面:首页、提示页(找对方 / 断线)、游戏页(比分、大字、炸弹、提示)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOMB_UI_TEXT 64

typedef struct {
    char score[BOMB_UI_TEXT];
    char big[BOMB_UI_TEXT];
    char hint[BOMB_UI_TEXT];
    uint32_t big_color;
    bool show_bomb;
    int bomb_x, bomb_y;   // 放大后炸弹左上角
    uint8_t heat;         // 0..255:炸弹染红多少
    bool spark;           // 引信火花这一帧亮不亮
    bool boom;            // 画爆炸
    bool sooty;           // 画被炸黑的脸
    bool flash;           // 屏幕闪一下
} bomb_view_t;

void bomb_ui_title(uint8_t sel, unsigned wins);
void bomb_ui_note(const char *big, uint32_t color, const char *l1, const char *l2, const char *l3);
void bomb_ui_game(void);
void bomb_ui_game_set(const bomb_view_t *v);
