// src/bomb_sprites.c —— 见 bomb_sprites.h。原创像素画,只用外壳调色板里的颜色。
#include "bomb_sprites.h"

#include <stddef.h>

static const char *const BOMB[] = {
    "...........yo...",
    "..........oy....",
    ".........kk.....",
    "........kgk.....",
    "......kkkkkk....",
    "....kkxxxxxxkk..",
    "...kxxxxxxxxxxk.",
    "..kxxwwxxxxxxxxk",
    "..kxwwxxxxxxxxxk",
    ".kxxwxxxxxxxxxxk",
    ".kxxxxxxxxxxxxxk",
    ".kxxxxxxxxxxxxxk",
    "..kxxxxxxxxxxxk.",
    "..kxxxxxxxxxxxk.",
    "...kxxxxxxxxxk..",
    "....kkkkkkkkk...",
};

static const char *const SPARK[] = {
    "...y....",
    ".y.y.y..",
    "..yoy...",
    "yyorryy.",
    "..yoy...",
    ".y.y.y..",
    "...y....",
    "........",
};

static const char *const BOOM[] = {
    ".......y........",
    "...y...yy...y...",
    "....y.yooy.y....",
    ".....yoooooy....",
    "..yyyoorrooyyy..",
    "...yoorrrrooy...",
    "..yoorrwwrrooy..",
    "yyoorrwwwwrrooyy",
    "..yoorrwwrrooy..",
    "...yoorrrrooy...",
    "..yyyoorrooyyy..",
    ".....yoooooy....",
    "....y.yooy.y....",
    "...y...yy...y...",
    ".......y........",
    "................",
};

// 四个人的热血头像(选人页、顶栏、特写放大 10 倍、被炸黑的脸):皱眉、眼里有光、喊出来。顺序同 bomb_fighter_t。
static const char *const HOT_KID[] = {
    "..y.kkkkkkkk.y..",
    "...kKKKKKKKKk...",
    "..kKKKKKKKKKKk..",
    ".kKKKKKKKKKKKKk.",
    ".kKKKKKKKKKKKKk.",
    ".kKssssssssssKk.",
    ".kskkkssssskkksk",
    ".ksswkkssskkwssk",
    ".kssKwssssswKssk",
    ".kfsssssnsssssfk",
    "..kssssssssssk..",
    "..kssskrrksssk..",
    "...kssskksssk...",
    "....kssssssk....",
    "...kzwwttwwzk...",
    "..kzzzwtTwzzzk..",
};

static const char *const HOT_DAD[] = {
    "................",
    "....kkkkkkkk....",
    "...kKKKKKKKKk...",
    "..kKKKKKKKKKKk..",
    "..kKKKKKKKKKKk..",
    "..kKssssssssKk..",
    ".kskkkssssskkksk",
    ".ksgggssssgggsk.",
    ".ksgwgssssgwgsk.",
    "..kssssnnssssk..",
    "..kskwwwwwwksk..",
    "..kskrrrrrrksk..",
    "...kskkkkkksk...",
    "..kBBwwsswwBBk..",
    ".kBBBBwBBwBBBBk.",
    ".kBBBBBBBBBBBBk.",
};

static const char *const HOT_GRANNY[] = {
    "................",
    "......kkkk......",
    ".....kKKKKk.....",
    "...kkKKKKKKkk...",
    "..kKKKKKKKKKKk..",
    "..kvvvvvvvvvvk..",
    ".kVVVVVVVVVVVVk.",
    "..kkksssssskkk..",
    "..kKswkssskwsKk.",
    "..kKfssnnssfKk..",
    "...kskwwwwksk...",
    "...kssrrrrssk...",
    "....kssssssk....",
    "...kdddssdddk...",
    "..kddddddddddk..",
    ".kdxddddddddxdk.",
};

static const char *const HOT_GRANDPA[] = {
    "................",
    "....kkkkkkkk....",
    "...kKKKKKKKKk...",
    "..kKKKKKKKKKKk..",
    "..khKKKKKKKKhk..",
    "..khsssssssshk..",
    ".kskkkssssskkksk",
    ".ksswkssssskwssk",
    "..kssssssnssssk.",
    "..khhhhhhhhhhk..",
    "..khkwwwwwwkhk..",
    "...khhhhhhhhk...",
    "...keesseexek...",
    "..keeeesexeeek..",
    "..keeeeexeeeek..",
    "..keeexxEeeeek..",
};

// 铁笼:关住炸弹(爸爸的超必杀)。
static const char *const CAGE[] = {
    "kkkkkkkkkkkkkkkk",
    "kgggggggggggggk.",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    ".g..g..g..g..g..",
    "kgggggggggggggk.",
    "kkkkkkkkkkkkkkkk",
};

// 太极:爷爷摆好架势时头像旁边的小图。
static const char *const TAIJI[] = {
    ".....kkkkkk.....",
    "...kkwwwwxxkk...",
    "..kwwwwwwxxxxk..",
    ".kwwwwwwxxxxxxk.",
    ".kwwwxxwxxxxxxk.",
    "kwwwwxxwxxxxxxxk",
    "kwwwwwwwxxxxxxxk",
    "kwwwwwwwwxxxxxxk",
    "kwwwwwwwwxxxxxxk",
    "kwwwwwwwxwwxxxxk",
    "kwwwwwwxxwwxxxxk",
    ".kwwwwwxxxxxxxk.",
    ".kwwwwxxxxxxxxk.",
    "..kwwwxxxxxxxk..",
    "...kkwwxxxxkk...",
    ".....kkkkkk.....",
};

// 电火花:火箭快递(炸弹冒电)。
static const char *const ZAP[] = {
    "....yy..",
    "...yy...",
    "..yyy...",
    ".yyyyyy.",
    "...yyy..",
    "...yy...",
    "..yy....",
    ".y......",
};

// 小皇冠:这个人闯关通关过。
static const char *const CROWN[] = {
    "y..y..y.",
    "yy.yy.yy",
    "yyyyyyyy",
    "yryyyryy",
    "yyyyyyyy",
    "oooooooo",
    "........",
    "........",
};

#define ART(a, w) { w, (uint8_t)(sizeof(a) / sizeof(a[0])), a }

static const spr_art_t ARTS[BS_COUNT] = {
    ART(BOMB, 16), ART(SPARK, 8), ART(BOOM, 16),
    ART(HOT_KID, 16), ART(HOT_DAD, 16), ART(HOT_GRANNY, 16), ART(HOT_GRANDPA, 16),
    ART(CAGE, 16), ART(TAIJI, 16), ART(ZAP, 8), ART(CROWN, 8),
};

const spr_art_t *bomb_art(bs_spr_t id) { return (unsigned)id < BS_COUNT ? &ARTS[id] : NULL; }
