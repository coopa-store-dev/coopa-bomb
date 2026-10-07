// src/bomb_app.c —— 库巴传炸弹的入口表和流程:首页 → 和电脑打(闯关)/ 两人联机(找对方)→ 选人 → 游戏 → 结束页。
// 按键:● 扔 / 确定 / 再来;▲ 必杀;▼ 超必杀(结束页 ▼ 回首页,选人页 ▲▼ 选)。不用长按:实体键长按会先发一次短按。
// 规则和对账在 bomb_game.c;联机走 kit_link(频道 BOMB_CH);和电脑打是 bomb_local.c 的两个 bomb_t + bomb_bot.c。
// 存档:saves 分区 "bomb"/"save"。
#include <stdio.h>
#include <string.h>

#include "bomb_bot.h"
#include "bomb_game.h"
#include "bomb_lines.h"
#include "bomb_local.h"
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
#define SAY_MS 600        // 「完美!」「扔歪了!」「锅盖神功!」这类大字闪多久
#define OVER_DEAF_MS 1000 // 结束页前 1 秒不收键(狂按的那一下别把人带回首页)
#define STAGES 3

typedef enum { PG_TITLE = 0, PG_LINK, PG_GAME } page_t;
typedef enum { MODE_LINK = 0, MODE_CPU } play_mode_t;
typedef enum { SCR_OTHER = 0, SCR_PICK, SCR_GAME } screen_t;

static const char *const NAMES[BF_COUNT] = { BX_KID, BX_DAD, BX_GRANNY, BX_GRANDPA };
static const char *const SKILL[SK_COUNT] = {
    [SK_SLING] = BX_SK_SLING, [SK_DECOY] = BX_SK_DECOY, [SK_ROCKET] = BX_SK_ROCKET, [SK_CAGE] = BX_SK_CAGE,
    [SK_NAG] = BX_SK_NAG, [SK_LID] = BX_SK_LID, [SK_GLASSES] = BX_SK_GLASSES, [SK_TAIJI] = BX_SK_TAIJI,
};

static bomb_save_t s_save;
static bomb_t s_link;          // 联机时的这一边
static bomb_local_t s_loc;     // 和电脑打:me + cpu
static bomb_bot_t s_bot;       // 电脑的脑子
static bomb_t *G = &s_link;    // 当前这一边
static uint8_t s_mode;         // play_mode_t
static uint8_t s_stage;        // 闯关第几关(1..3);联机时 0
static uint8_t s_order[STAGES];// 闯关要打的三个人
static bool s_auto;            // 和电脑打:进选人页时自动选上一关的人(下一关 / 再打一次)
static bool s_cleared;         // 三关都赢了
static page_t s_page;
static screen_t s_screen;
static uint8_t s_sel;          // 首页光标
static uint8_t s_cursor;       // 选人页光标(0..3 角色,4 回首页)
static uint8_t s_game_me = 0xFF, s_game_peer = 0xFF;  // 游戏页按哪两个人建的
static uint32_t s_session;
static bool s_up;
static char s_note[192];
static uint32_t s_fly, s_flash, s_tick, s_clock, s_say_ms, s_quiet, s_over_ms;
static const char *s_say;
static uint32_t s_say_color;
static uint8_t s_cut, s_cut_char;
static const char *s_cut_name;
static uint32_t s_cut_ms;
static bomb_view_t s_view;

static bool link_send(void *ctx, const void *m, size_t n) {
    (void)ctx;
    return kit_link_send(m, n);
}

static uint32_t rnd(void) { return esp_random(); }

static void store(void) { kit_sv_blob_store(BOMB_NS, BOMB_KEY, &s_save, sizeof(s_save)); }

// 内容没变就不重画(key 里放着画这一页用到的所有东西)
static bool changed(const char *key) {
    if (!strcmp(key, s_note)) return false;
    snprintf(s_note, sizeof(s_note), "%s", key);
    return true;
}

static void note(const char *big, uint32_t color, const char *l1, const char *l2, const char *l3) {
    char key[sizeof(s_note)];
    snprintf(key, sizeof(key), "N%s|%s|%s|%s|%lx", big, l1, l2, l3, (unsigned long)color);
    if (!changed(key)) return;
    s_screen = SCR_OTHER;
    bomb_ui_note(big, color, l1, l2, l3);
}

static void say(const char *text, uint32_t color) {
    s_say = text;
    s_say_color = color;
    s_say_ms = SAY_MS;
}

static void voice(const kit_line_t *line, sfx_id_t prefix) {
    sfx_play_line(line, prefix, false);
    s_quiet = line->samples / 16 + 200;  // 16 kHz:说话的这段时间不放嘀嗒(嘀嗒会切断语音)
}

static void to_title(void) {
    if (s_mode == MODE_LINK) kit_link_stop();
    bomb_lost(&s_link);
    s_up = false;
    s_page = PG_TITLE;
    s_screen = SCR_OTHER;
    s_note[0] = 0;
    s_cut = CUT_NONE;
    bomb_ui_title(s_sel, s_save.wins);
}

static void to_link(void) {
    s_mode = MODE_LINK;
    G = &s_link;
    s_stage = 0;
    s_page = PG_LINK;
    s_up = false;
    s_note[0] = 0;
    kit_link_start(BOMB_CH);
}

static void to_cpu(void) {
    s_mode = MODE_CPU;
    bomb_local_init(&s_loc, rnd);
    bomb_local_flow(&s_loc);
    G = &s_loc.me;
    s_stage = 1;
    s_auto = s_cleared = false;
    s_page = PG_GAME;
    s_screen = SCR_OTHER;
    s_note[0] = 0;
}

// 闯关:除了自己的人,另外三个随机排
static void make_order(uint8_t me) {
    uint8_t n = 0;
    for (uint8_t f = 0; f < BF_COUNT; f++)
        if (f != me) s_order[n++] = f;
    for (int i = STAGES - 1; i > 0; i--) {
        int j = (int)(rnd() % (uint32_t)(i + 1));
        uint8_t t = s_order[i];
        s_order[i] = s_order[j];
        s_order[j] = t;
    }
}

static void pick(uint8_t f) {
    if (!bomb_pick(G, f)) return;
    voice(&bomb_line_pick[f], SFX_NONE);
    if (bomb_save_pick(&s_save) != f) {
        bomb_save_set_pick(&s_save, f);
        store();
    }
    if (s_mode == MODE_CPU && s_stage == 1 && !s_auto) make_order(f);
}

static void start_cut(uint8_t sk, uint8_t ch) {
    bool super = bomb_skill_super(sk);
    s_cut = super ? CUT_SUPER : CUT_SPECIAL;
    s_cut_ms = 0;
    s_cut_char = ch < BF_COUNT ? ch : 0;
    s_cut_name = SKILL[sk];
    voice(super ? &bomb_line_super[s_cut_char] : &bomb_line_special[s_cut_char], super ? SFX_CHAIN : SFX_NONE);
}

static void on_event(uint8_t ev) {
    switch (ev) {
        case BE_ROUND: sfx_play(SFX_START); s_tick = 0; break;
        case BE_THROWN:
            s_fly = FLY_MS;
            if (s_cut == CUT_NONE) sfx_play(SFX_WHOOSH);
            break;
        case BE_MISS: sfx_play(SFX_BEAT); say(BX_MISS, UK_GRAY); break;
        case BE_PERFECT: sfx_play(SFX_PERFECT); say(BX_PERFECT, UK_GOLD); break;
        case BE_SKILL_OUT: start_cut(G->skill_out, G->me_char); break;
        case BE_SKILL_IN:
            if (G->skill_in == SK_LID_BOUNCE) {
                sfx_play(SFX_HIT);
                say(BX_SK_LID_BIG, UK_RED);
            } else if (G->skill_in == SK_TAIJI_PUSH) {
                sfx_play(SFX_HIT);
                say(BX_SK_TAIJI_BIG, UK_RED);
            } else {
                start_cut(G->skill_in, G->peer_char);
            }
            break;
        case BE_LID: sfx_play(SFX_HIT); say(BX_SK_LID_BIG, UK_GREEN); break;
        case BE_TAIJI: sfx_play(SFX_HIT); say(BX_SK_TAIJI_BIG, UK_GREEN); break;
        case BE_CAGE_HIT: sfx_play(SFX_TAP); break;
        case BE_CAGE_OPEN: sfx_play(SFX_HIT); break;
        case BE_DECOY_PUFF: sfx_play(SFX_FEINT); break;
        case BE_BOOM_ME:
        case BE_BOOM_PEER:
            s_flash = FLASH_MS;
            sfx_play(SFX_HIT);
            if (bomb_save_round(&s_save)) store();
            break;
        case BE_OVER: {
            bool won = G->score_me >= BOMB_WIN;
            uint8_t w = won ? G->me_char : G->peer_char;
            voice(&bomb_line_win[w < BF_COUNT ? w : 0], SFX_GAME_OVER);
            s_over_ms = OVER_DEAF_MS;
            bool dirty = bomb_save_over(&s_save, G->match, won);
            if (s_mode == MODE_CPU && won && s_stage == STAGES) {
                s_cleared = true;
                dirty |= bomb_save_set_crown(&s_save, G->me_char);
            }
            if (dirty) store();
            break;
        }
        case BE_PICK: s_cursor = bomb_save_pick(&s_save); break;
        default: break;
    }
}

static void bomb_init_app(void) {
    static bomb_save_t scratch;
    if (!kit_sv_blob_load(BOMB_NS, BOMB_KEY, &s_save, sizeof(s_save), bomb_save_valid, &scratch)) {
        bomb_save_reset(&s_save);
    }
    bomb_init(&s_link, link_send, NULL, rnd);
}

static void bomb_enter(const kit_links_t *links) {
    (void)links;
    s_sel = 0;
    s_mode = MODE_LINK;
    G = &s_link;
    to_title();
}

// 和电脑打的结束页按 ●:下一关 / 再打一次 / 通关后重新闯
static void cpu_again(void) {
    if (s_cleared) {
        s_stage = 1;
        s_cleared = s_auto = false;
    } else {
        if (G->score_me >= BOMB_WIN) s_stage++;
        s_auto = true;
    }
    bomb_press(&s_loc.me);
    bomb_press(&s_loc.cpu);
    bomb_local_flow(&s_loc);
}

static uint8_t key_of(kit_key_t k) { return k == KIT_KEY_OK ? BK_OK : k == KIT_KEY_UP ? BK_UP : BK_DOWN; }

static bool bomb_input(const kit_input_t *in) {
    if (in->long_press) return false;
    if (s_page == PG_TITLE) {
        if (in->key == KIT_KEY_OK) {
            if (s_sel == 2) return true;  // 回菜单
            if (s_sel == 0) to_cpu();
            else to_link();
            return false;
        }
        s_sel = (uint8_t)((s_sel + (in->key == KIT_KEY_UP ? 2 : 1)) % 3);
        bomb_ui_title(s_sel, s_save.wins);
        return false;
    }
    if (s_page == PG_LINK) {
        if (in->key == KIT_KEY_OK) to_title();
        return false;
    }
    if (G->phase == BP_PICK) {
        if (G->me_picked) {
            if (in->key == KIT_KEY_DOWN) bomb_unpick(G);
        } else if (in->key == KIT_KEY_OK) {
            if (s_cursor >= BF_COUNT) to_title();
            else pick(s_cursor);
        } else {
            s_cursor = (uint8_t)((s_cursor + (in->key == KIT_KEY_UP ? BF_COUNT : 1)) % (BF_COUNT + 1));
        }
        return false;
    }
    if (G->phase == BP_OVER) {
        if (s_over_ms) return false;
        if (in->key == KIT_KEY_DOWN) to_title();
        else if (in->key == KIT_KEY_OK) {
            if (s_mode == MODE_CPU) cpu_again();
            else bomb_press(G);
        }
        return false;
    }
    bomb_key(G, key_of(in->key));
    return false;
}

static void show_link(kit_link_state_t st) {
    if (s_link.bad_ver) note(BX_BAD_VER, UK_RED, BX_BAD_VER2, "", BX_HOME_HINT);
    else if (st == KIT_LINK_NO_MEMORY) note(BX_NO_MEM, UK_RED, BX_NO_MEM2, "", BX_HOME_HINT);
    else if (st == KIT_LINK_LOST) note(BX_LOST, UK_INK, BX_LOST2, "", BX_HOME_HINT);
    else note(BX_SEARCH, UK_INK, BX_SEARCH2, BX_SEARCH3, BX_HOME_HINT);
}

static void show_pick(void) {
    const char *title = BX_PICK;
    char peer[64] = "";
    if (s_mode == MODE_LINK) {
        if (G->peer_picked && G->peer_char < BF_COUNT) snprintf(peer, sizeof(peer), BX_PEER_PICKED, NAMES[G->peer_char]);
        else snprintf(peer, sizeof(peer), "%s", BX_PEER_PICKING);
    }
    const char *hint = G->me_picked ? BX_PICK_WAIT : BX_PICK_HINT;
    uint8_t sel = G->me_picked ? G->me_char : s_cursor;
    uint8_t crowns = 0;
    for (uint8_t f = 0; f < BF_COUNT; f++) crowns |= (uint8_t)(bomb_save_crown(&s_save, f) << f);
    char key[sizeof(s_note)];
    snprintf(key, sizeof(key), "P%u|%u|%u|%s|%s|%s", sel, G->me_picked, crowns, title, hint, peer);
    if (!changed(key)) return;
    s_screen = SCR_PICK;
    bomb_ui_pick(sel, G->me_picked, crowns, title, hint, peer);
}

static void keys_hint(char *buf, size_t len) {
    int n = snprintf(buf, len, "%s", BX_KEY_THROW);
    uint8_t sp = bomb_skill_of(G->me_char, false), su = bomb_skill_of(G->me_char, true);
    if (G->en_me >= bomb_skill_cost(sp) && sp && n > 0 && (size_t)n < len)
        n += snprintf(buf + n, len - (size_t)n, BX_KEY_SPECIAL, SKILL[sp]);
    if (G->en_me >= bomb_skill_cost(su) && su && n > 0 && (size_t)n < len)
        snprintf(buf + n, len - (size_t)n, BX_KEY_SUPER, SKILL[su]);
}

static void show_game(void) {
    if (s_screen != SCR_GAME || s_game_me != G->me_char || s_game_peer != G->peer_char) {
        s_game_me = G->me_char;
        s_game_peer = G->peer_char;
        s_screen = SCR_GAME;
        s_note[0] = 0;
        bomb_ui_game(s_game_me, s_game_peer);
    }
    bomb_view_t *v = &s_view;
    memset(v, 0, sizeof(*v));
    const bomb_t *g = G;
    snprintf(v->score, sizeof(v->score), BX_SCORE, (unsigned)g->score_me, (unsigned)g->score_peer);
    v->en_me = g->en_me;
    v->en_peer = g->en_peer;
    v->blink = (s_clock / 200) & 1;
    v->lid = g->lid;
    v->taiji = g->taiji;
    v->glasses = g->glasses_left;
    v->big_color = UK_INK;
    v->bomb_x = BOMB_X;
    v->bomb_y = BOMB_Y;
    v->flash = s_flash > 0;
    v->face = -1;
    switch (g->phase) {
        case BP_COUNT:
            snprintf(v->big, sizeof(v->big), "%u", (unsigned)(g->delay_ms / 1000 + 1));
            if (s_mode == MODE_CPU) snprintf(v->hint, sizeof(v->hint), BX_STAGE, (unsigned)s_stage);
            else snprintf(v->hint, sizeof(v->hint), BX_ROUND, (unsigned)g->round);
            v->show_bomb = g->holding;
            break;
        case BP_PLAY: {
            uint8_t heat = bomb_heat(g);
            if (g->holding) {
                v->show_bomb = !g->freeze_ms || s_cut == CUT_NONE;
                v->heat = heat;
                v->spark = (s_clock / 120) & 1;
                v->zap = g->fast && ((s_clock / 80) & 1);
                v->cage = g->cage_left > 0;
                v->decoy = g->decoy ? g->decoy_sel : 0;
                int amp = heat / 64;  // 越热抖得越厉害:0..3 像素
                if (amp) v->bomb_x += (int)(s_clock / 40 % (unsigned)(2 * amp + 1)) - amp;
                v->big_color = UK_RED;
                if (g->freeze_ms || g->hold_ms < g->catch_ms) {  // 刚接到:从上面掉下来(被弹弓砸晕时掉得慢)
                    uint32_t t = g->freeze_ms ? 0 : g->hold_ms;
                    v->bomb_y = -96 + (int)((BOMB_Y + 96) * t / g->catch_ms);
                    snprintf(v->big, sizeof(v->big), "%s", BX_CATCH);
                } else if (g->cage_left) {
                    snprintf(v->hint, sizeof(v->hint), BX_CAGE_HINT, (unsigned)g->cage_left);
                } else {
                    snprintf(v->big, sizeof(v->big), "%s", heat > 170 ? BX_HOT : "");
                    if (g->decoy) snprintf(v->hint, sizeof(v->hint), "%s", BX_DECOY_HINT);
                    else keys_hint(v->hint, sizeof(v->hint));
                    bomb_aim_t a = bomb_aim_now(g);
                    v->aim = true;
                    v->aim_pos = bomb_aim_pos(g);
                    v->green_half = a.green_half;
                    v->perfect_half = a.perfect_half;
                }
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
            if (g->loser_me && !s_flash) {
                v->face = g->me_char;
                v->sooty = true;
            }
            snprintf(v->big, sizeof(v->big), "%s", g->loser_me ? BX_BOOM_ME : BX_BOOM_PEER);
            v->big_color = g->loser_me ? UK_RED : UK_GREEN;
            break;
        case BP_OVER: {
            bool won = g->score_me >= BOMB_WIN;
            v->face = g->me_char;
            v->sooty = !won;
            const char *big = won ? BX_WIN : BX_LOSE, *hint = g->me_again ? BX_WAIT_AGAIN : BX_AGAIN;
            if (s_mode == MODE_CPU) {
                big = s_cleared ? BX_CLEAR : big;
                hint = s_cleared ? BX_CLEAR_HINT : won ? BX_NEXT : BX_RETRY;
            }
            snprintf(v->big, sizeof(v->big), "%s", big);
            v->big_color = won ? UK_RED : UK_INK;
            snprintf(v->hint, sizeof(v->hint), "%s", s_over_ms ? "" : hint);
            break;
        }
        default: break;
    }
    if (s_say_ms && g->phase == BP_PLAY) {
        snprintf(v->big, sizeof(v->big), "%s", s_say);
        v->big_color = s_say_color;
    }
    if (s_cut != CUT_NONE) {
        v->cut = s_cut;
        v->cut_char = s_cut_char;
        v->cut_name = s_cut_name;
        v->cut_t = s_cut_ms;
    }
    bomb_ui_game_set(v);
}

// 联机:收发消息、断线;返回现在能不能显示游戏页
static bool link_frame(uint32_t dt) {
    kit_link_state_t st = kit_link_state();
    if (st == KIT_LINK_CONNECTED) {
        if (!s_up || kit_link_session() != s_session) {
            s_up = true;
            s_session = kit_link_session();
            bomb_connected(&s_link, kit_link_initiator());
        }
        uint8_t msg[KIT_LINK_MSG_MAX];
        size_t n;
        while ((n = kit_link_recv(msg, sizeof(msg))) > 0) bomb_on_msg(&s_link, msg, n);
    } else if (s_up) {
        s_up = false;
        bomb_lost(&s_link);
    }
    bomb_tick(&s_link, dt);
    bomb_pump(&s_link);
    bool game = st == KIT_LINK_CONNECTED && s_link.phase != BP_WAIT;
    if (!game) show_link(st);
    return game;
}

// 和电脑打:两边一起走,电脑按自己的脑子按键;进选人页时电脑(和「下一关」时的自己)自动选人
static void cpu_frame(uint32_t dt) {
    bomb_t *me = &s_loc.me, *cpu = &s_loc.cpu;
    if (me->phase == BP_PICK && !me->me_picked && s_auto) pick(bomb_save_pick(&s_save));
    if (cpu->phase == BP_PICK && !cpu->me_picked && me->me_picked) {
        bomb_bot_init(&s_bot, (uint8_t)(s_stage - 1), rnd);
        bomb_pick(cpu, s_order[s_stage - 1]);
    }
    bomb_tick(me, dt);
    bomb_tick(cpu, dt);
    bomb_seen_t s;
    bomb_seen(cpu, &s);
    int k = bomb_bot_step(&s_bot, &s, dt);
    if (k >= 0) bomb_key(cpu, (uint8_t)k);
    bomb_local_flow(&s_loc);
    uint8_t ev;
    while (bomb_event(cpu, &ev)) {}
}

static void bomb_frame(uint32_t dt) {
    if (s_page == PG_TITLE) return;
    bool game = true;
    if (s_mode == MODE_LINK) game = link_frame(dt);
    else cpu_frame(dt);
    uint8_t ev;
    while (bomb_event(G, &ev)) on_event(ev);
    uk_countdown(&s_fly, dt);
    uk_countdown(&s_flash, dt);
    uk_countdown(&s_say_ms, dt);
    uk_countdown(&s_quiet, dt);
    uk_countdown(&s_over_ms, dt);
    if (s_cut != CUT_NONE) {
        s_cut_ms += dt;
        if (s_cut_ms >= (s_cut == CUT_SUPER ? BOMB_CUT_SUPER_MS : BOMB_CUT_SPECIAL_MS)) s_cut = CUT_NONE;
    }
    s_clock += dt;
    if (G->phase == BP_PLAY && G->holding && !G->freeze_ms && s_cut == CUT_NONE && !s_quiet &&
        !uk_countdown(&s_tick, dt)) {
        sfx_play(SFX_TICK);
        s_tick = bomb_tick_gap(G);
    }
    s_page = game ? PG_GAME : PG_LINK;
    if (!game) {
        s_screen = SCR_OTHER;
        return;
    }
    if (G->phase == BP_PICK) show_pick();
    else if (G->phase != BP_WAIT) show_game();
}

// 一场没打完(包括选人、断线等对方回来)都不让屏幕熄:对方一回来炸弹就在烧,黑屏时第一下 ● 只会点亮屏幕。
static unsigned bomb_busy(void) {
    bool mid = s_page == PG_GAME ? G->phase != BP_OVER
                                 : s_page == PG_LINK && s_link.match && !bomb_over(&s_link);
    return mid ? KIT_BUSY_TIMING : 0;
}

static bool bomb_won(void) { return s_save.wins > 0; }

static uint8_t bomb_milestones(void) { return bomb_save_milestones(&s_save); }

// 调试通道状态行(前 8 个字段格式冻结,tests/sim_duo.py 和真卡测试在读;2.0 在末尾加了后面的):
// BM <页> <阶段> <拿着> <我的分> <对方分> <回合> <场号> <赢的场数> <模式> <我的人> <对方的人> <我的能量> <对方能量>
//    <第几关> <箭头> <能不能扔>
static size_t bomb_status(char *buf, size_t len) {
    const bomb_t *g = G;
    int n = snprintf(buf, len, "BM %d %d %d %u %u %u %08lx %u %u %u %u %u %u %u %u %d\n", (int)s_page, (int)g->phase,
                     (int)g->holding, (unsigned)g->score_me, (unsigned)g->score_peer, (unsigned)g->round,
                     (unsigned long)g->match, (unsigned)s_save.wins, (unsigned)s_mode, (unsigned)g->me_char,
                     (unsigned)g->peer_char, (unsigned)g->en_me, (unsigned)g->en_peer, (unsigned)s_stage,
                     (unsigned)bomb_aim_pos(g), (int)bomb_can_throw(g));
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
