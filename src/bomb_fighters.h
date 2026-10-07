// src/bomb_fighters.h —— 四个角色的招式表和瞄准参数(纯数据 / 纯计算,宿主测试)。
// 招式编号直接写进 THROW 消息(协议 2),不靠对方的角色查表;编号一旦发布不能改。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { BF_KID = 0, BF_DAD, BF_GRANNY, BF_GRANDPA, BF_COUNT } bomb_fighter_t;

typedef enum {
    SK_NONE = 0,
    SK_SLING, SK_DECOY,     // 库巴:弹弓、影分身
    SK_ROCKET, SK_CAGE,     // 爸爸:火箭快递、铁笼
    SK_NAG, SK_LID,         // 奶奶:念叨、锅盖神功
    SK_GLASSES, SK_TAIJI,   // 爷爷:老花镜、太极推手
    SK_LID_BOUNCE,          // 锅盖把炸弹崩回去(不是玩家按的)
    SK_TAIJI_PUSH,          // 太极架势把炸弹推回去
    SK_COUNT
} bomb_skill_t;

#define BOMB_AIM_COLD 2000   // 箭头来回一趟(毫秒):炸弹凉的时候
#define BOMB_AIM_HOT 1200    // 最烫的时候
#define BOMB_AIM_MID 500     // 瞄准条 0..1000,绿区以中点为中心
#define BOMB_GREEN_HALF 150  // 绿区半宽(整条 30%)
#define BOMB_PERFECT_HALF 40 // 完美区半宽(8%)
#define BOMB_GREEN_MIN_MS 100   // 任何叠加下,箭头扫过绿区至少这么久
#define BOMB_PERFECT_MIN_MS 30

uint8_t bomb_skill_of(uint8_t fighter, bool super);  // 越界返回 SK_NONE
bool bomb_skill_super(uint8_t sk);
uint8_t bomb_skill_cost(uint8_t sk);                  // 必杀 2、超必杀 4、其他 0
bool bomb_skill_hits(uint8_t sk);                     // 作用在对方身上(对方因此 +1 能量)
uint32_t bomb_skill_freeze(uint8_t sk);               // 收到的一方冻结多久(特写 / 闪字)

typedef struct {
    uint16_t speed_x1000;   // 箭头每毫秒走几个单位 ×1000(单程 1000 个单位)
    uint16_t green_half;
    uint16_t perfect_half;
} bomb_aim_t;

// heat 0..255(bomb_heat);nag = 被念叨;glasses = 戴着老花镜。先老花镜再念叨,最后守最小毫秒。
bomb_aim_t bomb_aim_params(uint8_t heat, bool nag, bool glasses);
