// src/bomb_save.h —— 库巴传炸弹的存档(saves 分区的 "bomb"/"save"):赢了几场、下过几场、打过几个回合。
// 纯数据,读写由 bomb_app.c 用 kit_sv_* 做。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOMB_NS "bomb"   // = coopa.toml 的 id
#define BOMB_KEY "save"
#define BOMB_MAGIC 0x424D4F42u  // "BOMB"(小端)
#define BOMB_SAVE_VER 1

typedef struct {
    uint32_t magic;
    uint8_t ver;
    uint8_t pad[3];
    uint16_t wins;
    uint16_t games;
    uint32_t rounds;   // 打完的回合(封顶)
    uint32_t counted;  // 最后记过胜负的场号
} bomb_save_t;

void bomb_save_reset(bomb_save_t *s);
bool bomb_save_valid(const void *blob);
bool bomb_save_round(bomb_save_t *s);                          // 打完一回合;返回 true = 有变化
bool bomb_save_over(bomb_save_t *s, uint32_t match, bool won);  // 一场打完(同一场只记一次)
// 里程碑位:0 第一次传炸弹(打完一回合)、1 赢一场、2 赢 10 场。
uint8_t bomb_save_milestones(const bomb_save_t *s);
