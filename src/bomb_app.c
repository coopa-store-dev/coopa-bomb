// src/bomb_app.c —— 库巴传炸弹的入口表和流程:首页 → 找对方 → 游戏(倒数、传炸弹、爆炸、结束)。
// 按键只有 ●(扔、再来一场、在找对方 / 断线时回首页);结束页 ▼ 回首页。不用长按:实体键长按会先发一次短按。
// 联机:kit_link(频道 BOMB_CH),规则和对账在 bomb_game.c。存档:saves 分区 "bomb"/"save"。
#include <stdio.h>
#include <string.h>

#include "bomb_game.h"
#include "bomb_save.h"
#include "bomb_sprites.h"
#include "bomb_texts.h"
#include "bomb_ui.h"
#include "esp_random.h"
#include "kit_app.h"
#include "kit_art.h"
#include "kit_link.h"
#include "kit_sfx.h"
#include "kit_sprite.h"
#include "kit_store.h"
#include "ui_kit.h"

#define BOMB_CH 0x7AC2u   // 联机频道:上架编号审核后才有,先用固定的(库巴大作战是 0x7AC1)
#define FLY_MS 300        // 扔出去时往上飞多久
#define BOMB_X 72         // 炸弹放大后左上角
#define BOMB_Y 120
#define FLASH_MS 250

typedef enum { PG_TITLE = 0, PG_LINK, PG_GAME } page_t;

static bomb_save_t s_save;
static bomb_t s_g;
static page_t s_page;
static uint8_t s_sel;
static uint32_t s_session;
static bool s_up;
static char s_note[160];
static uint32_t s_fly;     // 扔出去后还要飞多久(画面)
static uint32_t s_flash;   // 爆炸闪屏还剩多久
static uint32_t s_tick;    // 下一声嘀嗒还有多久
static uint32_t s_clock;   // 本页的时钟(火花闪烁、抖动)
static bomb_view_t s_view;

static bool link_send(void *ctx, const void *m, size_t n) {
    (void)ctx;
    return kit_link_send(m, n);
}

static uint32_t rnd(void) { return esp_random(); }  // 只在主动方开回合时取

static void store(void) { kit_sv_blob_store(BOMB_NS, BOMB_KEY, &s_save, sizeof(s_save)); }

static void note(const char *big, uint32_t color, const char *l1, const char *l2, const char *l3) {
    char key[sizeof(s_note)];
    snprintf(key, sizeof(key), "%s|%s|%s|%s|%lx", big, l1, l2, l3, (unsigned long)color);
    if (!strcmp(key, s_note)) return;
    memcpy(s_note, key, sizeof(s_note));
    bomb_ui_note(big, color, l1, l2, l3);
}

static void to_title(void) {
    kit_link_stop();
    bomb_lost(&s_g);
    s_up = false;
    s_page = PG_TITLE;
    s_note[0] = 0;
    bomb_ui_title(s_sel, s_save.wins);
}

static void to_link(void) {
    s_page = PG_LINK;
    s_up = false;
    s_note[0] = 0;
    kit_link_start(BOMB_CH);
}

static void show_link(kit_link_state_t st) {
    if (s_g.bad_ver) note(BX_BAD_VER, UK_RED, BX_BAD_VER2, "", BX_HOME_HINT);
    else if (st == KIT_LINK_NO_MEMORY) note(BX_NO_MEM, UK_RED, BX_NO_MEM2, "", BX_HOME_HINT);
    else if (st == KIT_LINK_LOST) note(BX_LOST, UK_INK, BX_LOST2, "", BX_HOME_HINT);
    else note(BX_SEARCH, UK_INK, BX_SEARCH2, BX_SEARCH3, BX_HOME_HINT);
}

static void show_game(void) {
    bomb_view_t *v = &s_view;
    memset(v, 0, sizeof(*v));
    const bomb_t *g = &s_g;
    snprintf(v->score, sizeof(v->score), BX_SCORE, (unsigned)g->score_me, (unsigned)g->score_peer);
    v->big_color = UK_INK;
    v->bomb_x = BOMB_X;
    v->bomb_y = BOMB_Y;
    v->flash = s_flash > 0;
    switch (g->phase) {
        case BP_COUNT:
            snprintf(v->big, sizeof(v->big), "%u", (unsigned)(g->delay_ms / 1000 + 1));
            snprintf(v->hint, sizeof(v->hint), BX_ROUND, (unsigned)g->round);
            v->show_bomb = g->holding;
            break;
        case BP_PLAY: {
            uint8_t heat = bomb_heat(g);
            if (g->holding) {
                v->show_bomb = true;
                v->heat = heat;
                v->spark = (s_clock / 120) & 1;
                int amp = heat / 64;  // 越热抖得越厉害:0..3 像素
                if (amp) v->bomb_x += (int)(s_clock / 40 % (unsigned)(2 * amp + 1)) - amp;
                if (g->hold_ms < BOMB_CATCH_MS) {  // 刚接到:从上面掉下来
                    v->bomb_y = -96 + (int)((BOMB_Y + 96) * g->hold_ms / BOMB_CATCH_MS);
                    snprintf(v->big, sizeof(v->big), "%s", BX_CATCH);
                } else {
                    snprintf(v->big, sizeof(v->big), "%s", heat > 170 ? BX_HOT : "");
                    snprintf(v->hint, sizeof(v->hint), "%s", BX_THROW);
                }
                v->big_color = UK_RED;
            } else if (s_fly) {  // 刚扔出去:往上飞
                v->show_bomb = true;
                v->bomb_y = BOMB_Y - (int)((BOMB_Y + 96) * (FLY_MS - s_fly) / FLY_MS);
                snprintf(v->big, sizeof(v->big), "%s", BX_FLY);
            } else {
                snprintf(v->big, sizeof(v->big), "%s", BX_AWAY);
                v->big_color = UK_GRAY;
            }
            break;
        }
        case BP_BOOM:
            v->boom = !g->loser_me || s_flash > 0;
            v->sooty = g->loser_me && !s_flash;
            snprintf(v->big, sizeof(v->big), "%s", g->loser_me ? BX_BOOM_ME : BX_BOOM_PEER);
            v->big_color = g->loser_me ? UK_RED : UK_GREEN;
            break;
        case BP_OVER: {
            bool won = g->score_me >= BOMB_WIN;
            snprintf(v->big, sizeof(v->big), "%s", won ? BX_WIN : BX_LOSE);
            v->big_color = won ? UK_RED : UK_INK;
            v->sooty = !won;
            snprintf(v->hint, sizeof(v->hint), "%s", g->me_again ? BX_WAIT_AGAIN : BX_AGAIN);
            break;
        }
        default: break;
    }
    bomb_ui_game_set(v);
}

static void on_event(uint8_t ev) {
    switch (ev) {
        case BE_ROUND: sfx_play(SFX_START); s_tick = 0; break;
        case BE_THROWN: s_fly = FLY_MS; sfx_play(SFX_WHOOSH); break;
        case BE_BOOM_ME:
        case BE_BOOM_PEER:
            s_flash = FLASH_MS;
            sfx_play(SFX_HIT);
            if (bomb_save_round(&s_save)) store();
            break;
        case BE_OVER:
            sfx_play(SFX_GAME_OVER);
            if (bomb_save_over(&s_save, s_g.match, s_g.score_me >= BOMB_WIN)) store();
            break;
        default: break;
    }
}

static void bomb_init_app(void) {
    static bomb_save_t scratch;
    if (!kit_sv_blob_load(BOMB_NS, BOMB_KEY, &s_save, sizeof(s_save), bomb_save_valid, &scratch)) {
        bomb_save_reset(&s_save);
    }
    bomb_init(&s_g, link_send, NULL, rnd);
}

static void bomb_enter(const kit_links_t *links) {
    (void)links;
    s_sel = 0;
    to_title();
}

static bool bomb_input(const kit_input_t *in) {
    if (in->long_press) return false;
    if (s_page == PG_TITLE) {
        if (in->key == KIT_KEY_OK) {
            if (s_sel == 1) return true;  // 回菜单
            to_link();
            return false;
        }
        s_sel ^= 1;
        bomb_ui_title(s_sel, s_save.wins);
        return false;
    }
    if (s_page == PG_LINK) {
        if (in->key == KIT_KEY_OK) to_title();
        return false;
    }
    if (s_g.phase == BP_OVER && in->key == KIT_KEY_DOWN) to_title();
    else if (in->key == KIT_KEY_OK) bomb_press(&s_g);
    return false;
}

static void bomb_frame(uint32_t dt) {
    if (s_page == PG_TITLE) return;
    kit_link_state_t st = kit_link_state();
    if (st == KIT_LINK_CONNECTED) {
        if (!s_up || kit_link_session() != s_session) {
            s_up = true;
            s_session = kit_link_session();
            bomb_connected(&s_g, kit_link_initiator());
        }
        uint8_t msg[KIT_LINK_MSG_MAX];
        size_t n;
        while ((n = kit_link_recv(msg, sizeof(msg))) > 0) bomb_on_msg(&s_g, msg, n);
    } else if (s_up) {
        s_up = false;
        bomb_lost(&s_g);
    }
    bomb_tick(&s_g, dt);
    bomb_pump(&s_g);
    uint8_t ev;
    while (bomb_event(&s_g, &ev)) on_event(ev);
    uk_countdown(&s_fly, dt);
    uk_countdown(&s_flash, dt);
    s_clock += dt;
    if (s_g.phase == BP_PLAY && s_g.holding && !uk_countdown(&s_tick, dt)) {
        sfx_play(SFX_TICK);
        s_tick = bomb_tick_gap(&s_g);
    }
    bool game = st == KIT_LINK_CONNECTED && s_g.phase != BP_WAIT;
    if (game && s_page != PG_GAME) {
        s_page = PG_GAME;
        s_note[0] = 0;
        bomb_ui_game();
    } else if (!game && s_page == PG_GAME) {
        s_page = PG_LINK;  // 断线 / 还没开回合:回提示页(比分留在 s_g 里,连回来接着打)
    }
    if (s_page == PG_GAME) show_game();
    else show_link(st);
}

// 一场没打完(包括断线等对方回来)都不让屏幕熄:对方一回来炸弹就在烧,黑屏时第一下 ● 只会点亮屏幕。
static unsigned bomb_busy(void) {
    bool mid = s_page == PG_GAME ? s_g.phase != BP_OVER : s_page == PG_LINK && s_g.match && !bomb_over(&s_g);
    return mid ? KIT_BUSY_TIMING : 0;
}

static bool bomb_won(void) { return s_save.wins > 0; }

static uint8_t bomb_milestones(void) { return bomb_save_milestones(&s_save); }

// 调试通道状态行(格式冻结,tests/sim_duo.py 和真卡测试在读):
// BM <页> <阶段> <拿着> <我的分> <对方分> <回合> <场号> <赢的场数>
static size_t bomb_status(char *buf, size_t len) {
    int n = snprintf(buf, len, "BM %d %d %d %u %u %u %08lx %u\n", (int)s_page, (int)s_g.phase, (int)s_g.holding,
                     (unsigned)s_g.score_me, (unsigned)s_g.score_peer, (unsigned)s_g.round,
                     (unsigned long)s_g.match, (unsigned)s_save.wins);
    return n < 0 ? 0 : ((size_t)n < len ? (size_t)n : len - 1);
}

static void bomb_save_now(void) { store(); }

static const kit_menu_card_t BOMB_CARD = {
    .title = BX_TITLE,
    .sub = BX_SUB,
    .art = { SPR_KID, SPR_DAD },
};

const kit_app_t bomb_app = {
    .id = "bomb",
    .sprite_budget = KIT_SPR_BUDGET(BOMB_SPR_IMAGES * 16 * 16 * 4, BOMB_SPR_IMAGES),
    .init = bomb_init_app,
    .enter = bomb_enter,
    .input = bomb_input,
    .frame = bomb_frame,
    .busy = bomb_busy,
    .won = bomb_won,
    .milestones = bomb_milestones,
    .status = bomb_status,
    .save_now = bomb_save_now,
    .card = &BOMB_CARD,
};
