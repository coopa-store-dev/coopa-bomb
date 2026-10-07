// src/bomb_ui.c —— 见 bomb_ui.h。游戏页的控件在 bomb_ui_game 里一次建好,每帧只改位置、文字、颜色、显隐。
// 图都放在本游戏的精灵会话里(sprite_budget 见 bomb_app.c)。
#include "bomb_ui.h"

#include <stdio.h>
#include <string.h>

#include "bomb_fighters.h"
#include "bomb_sprites.h"
#include "bomb_texts.h"
#include "kit_sprite.h"
#include "ui_kit.h"

#define BIG 6       // 炸弹、爆炸、大头像放大 6 倍(16 → 96 像素)
#define FACE_X 72
#define FACE_Y 110
#define AIM_X 20    // 瞄准条
#define AIM_Y 238
#define AIM_W 200
#define AIM_H 12
#define RAYS 16

static const char *const NAMES[BF_COUNT] = { BX_KID, BX_DAD, BX_GRANNY, BX_GRANDPA };

static lv_obj_t *s_scr, *s_score, *s_big, *s_hint, *s_bomb, *s_bomb2, *s_spark, *s_zap, *s_cage, *s_boom, *s_face;
static lv_obj_t *s_en[2][4], *s_lid, *s_taiji, *s_glasses;
static lv_obj_t *s_bar, *s_green, *s_perfect, *s_arrow;
static lv_obj_t *s_cut_bg, *s_ray[RAYS], *s_cut_face, *s_cut_name;
static lv_point_precise_t s_ray_pts[RAYS][2];
static uint8_t s_face_char = 0xFF, s_cut_shown = 0xFF;

static lv_obj_t *centered(lv_obj_t *scr, const lv_font_t *f, uint32_t color, const char *s, int y) {
    lv_obj_t *t = uk_text(scr, f, color, s);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
    return t;
}

// 自己画的图放大 scale 倍;控件保持原图大小、以中心为轴放大。
static lv_obj_t *image(lv_obj_t *scr, const spr_art_t *a, int scale) {
    lv_obj_t *o = lv_image_create(scr);
    lv_image_set_src(o, kit_img(a, spr_decode));
    lv_obj_set_size(o, a->w, a->h);
    lv_image_set_scale(o, (uint32_t)(256 * scale));
    lv_image_set_antialias(o, false);
    return o;
}

// (x, y) = 放大后的左上角
static void place(lv_obj_t *o, int w, int scale, int x, int y) {
    lv_obj_set_pos(o, x + (w * scale - w) / 2, y + (w * scale - w) / 2);
}

static const spr_art_t *hot(uint8_t f) { return bomb_art((bs_spr_t)(BS_HOT_KID + (f < BF_COUNT ? f : 0))); }

void bomb_ui_title(uint8_t sel, unsigned wins) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F36, UK_INK, BX_TITLE, 24);
    centered(scr, UK_F12, UK_INK, BX_SUB, 70);
    for (int f = 0; f < BF_COUNT; f++) place(image(scr, hot((uint8_t)f), 3), 16, 3, 24 + f * 50, 94);
    const char *items[3] = { BX_CPU, BX_LINK, BX_BACK };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *p = uk_pill(scr, UK_F24, sel == i ? UK_RED : UK_LINE, UK_WHITE, items[i]);
        lv_obj_align(p, LV_ALIGN_TOP_MID, 0, 158 + i * 44);
    }
    char w[32];
    snprintf(w, sizeof(w), BX_WINS, wins);
    centered(scr, UK_F12, UK_INK, w, 296);
    uk_screen_swap(scr);
}

void bomb_ui_note(const char *big, uint32_t color, const char *l1, const char *l2, const char *l3) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    place(image(scr, bomb_art(BS_BOMB), 4), 16, 4, 88, 30);
    centered(scr, UK_F24, color, big, 120);
    centered(scr, UK_F12, UK_INK, l1, 170);
    centered(scr, UK_F12, UK_INK, l2, 200);
    centered(scr, UK_F12, UK_INK, l3, 270);
    uk_screen_swap(scr);
}

void bomb_ui_pick(uint8_t sel, bool picked, uint8_t crowns, const char *title, const char *hint, const char *peer) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F24, UK_INK, title, 6);
    for (int f = 0; f < BF_COUNT; f++) {
        int cx = (f % 2) * 120, cy = 40 + (f / 2) * 104;
        if (sel == f) uk_rect(scr, cx + 6, cy, 108, 100, picked ? UK_GREEN : UK_GOLD);
        uk_rect(scr, cx + 10, cy + 4, 100, 92, UK_WAINSCOT);
        place(image(scr, hot((uint8_t)f), 4), 16, 4, cx + 28, cy + 6);
        if (crowns >> f & 1) place(image(scr, bomb_art(BS_CROWN), 3), 8, 3, cx + 76, cy + 4);
        lv_obj_t *n = uk_text(scr, UK_F12, UK_INK, NAMES[f]);
        lv_obj_set_width(n, 100);
        lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(n, cx + 10, cy + 74);
    }
    centered(scr, UK_F12, UK_INK, peer, 252);
    centered(scr, UK_F12, UK_INK, hint, 270);
    lv_obj_t *home = uk_pill(scr, UK_F12, sel == BF_COUNT ? UK_RED : UK_LINE, UK_WHITE, BX_HOME);
    lv_obj_align(home, LV_ALIGN_TOP_MID, 0, 290);
    uk_screen_swap(scr);
}

static void energy_row(int side, int x0, int dir) {
    for (int i = 0; i < 4; i++) s_en[side][i] = uk_rect(s_scr, x0 + dir * i * 13, 30, 11, 8, UK_LINE);
}

void bomb_ui_game(uint8_t me_char, uint8_t peer_char) {
    s_scr = uk_screen_new(UK_WALL);
    // 顶栏:左边自己、右边对方;双方同一个人时对方头像加灰框
    place(image(s_scr, hot(me_char), 2), 16, 2, 4, 4);
    if (peer_char == me_char) uk_rect(s_scr, 202, 2, 36, 36, UK_GRAY);
    place(image(s_scr, hot(peer_char), 2), 16, 2, 204, 4);
    energy_row(0, 40, 1);
    energy_row(1, 189, -1);
    s_lid = uk_sprite(s_scr, SPR_POTLID, 1);
    uk_spos(s_lid, 40, 12);
    s_taiji = image(s_scr, bomb_art(BS_TAIJI), 1);
    place(s_taiji, 16, 1, 56, 10);
    s_glasses = uk_text(s_scr, UK_F12, UK_INK, "");
    lv_obj_set_pos(s_glasses, 4, 40);
    s_score = centered(s_scr, UK_F24, UK_INK, "", 6);
    s_big = centered(s_scr, UK_F36, UK_INK, "", 56);
    // 中间
    s_face = image(s_scr, hot(me_char), BIG);
    s_face_char = me_char;
    place(s_face, 16, BIG, FACE_X, FACE_Y);
    s_boom = image(s_scr, bomb_art(BS_BOOM), BIG);
    place(s_boom, 16, BIG, FACE_X, FACE_Y);
    s_bomb = image(s_scr, bomb_art(BS_BOMB), BIG);
    lv_obj_set_style_image_recolor(s_bomb, lv_color_hex(UK_RED), 0);
    s_bomb2 = image(s_scr, bomb_art(BS_BOMB), BIG);
    s_spark = image(s_scr, bomb_art(BS_SPARK), 3);
    s_zap = image(s_scr, bomb_art(BS_ZAP), 3);
    s_cage = image(s_scr, bomb_art(BS_CAGE), BIG);
    // 瞄准条
    s_bar = uk_rect(s_scr, AIM_X, AIM_Y, AIM_W, AIM_H, UK_LINE);
    s_green = uk_rect(s_scr, AIM_X, AIM_Y, 1, AIM_H, UK_GREEN);
    s_perfect = uk_rect(s_scr, AIM_X, AIM_Y, 1, AIM_H, UK_GOLD);
    s_arrow = uk_rect(s_scr, AIM_X, AIM_Y - 6, 4, AIM_H + 12, UK_INK);
    s_hint = centered(s_scr, UK_F12, UK_INK, "", 290);
    // 特写(最上层):黑底、放射速度线、大头像、招式名
    s_cut_bg = uk_rect(s_scr, 0, 0, UK_W, UK_H, UK_INK);
    for (int i = 0; i < RAYS; i++) {
        s_ray[i] = lv_line_create(s_scr);
        lv_obj_set_style_line_width(s_ray[i], 6, 0);
        lv_obj_set_style_line_rounded(s_ray[i], false, 0);
    }
    s_cut_face = image(s_scr, hot(me_char), 10);
    s_cut_shown = me_char;
    s_cut_name = centered(s_scr, UK_F36, UK_GOLD, "", 0);
    uk_screen_swap(s_scr);
}

// 速度线:从屏幕中心往外,长度随时间涨;i 是第几根。
static void ray(int i, uint32_t t) {
    static const int8_t DX[RAYS] = { 0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38 };
    static const int8_t DY[RAYS] = { -100, -92, -71, -38, 0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92 };
    int r0 = 30 + (int)(t % 160) / 2, r1 = r0 + 60 + (int)(t / 4 % 120);
    s_ray_pts[i][0].x = UK_W / 2 + DX[i] * r0 / 100;
    s_ray_pts[i][0].y = UK_H / 2 + DY[i] * r0 / 100;
    s_ray_pts[i][1].x = UK_W / 2 + DX[i] * r1 / 100;
    s_ray_pts[i][1].y = UK_H / 2 + DY[i] * r1 / 100;
    lv_line_set_points(s_ray[i], s_ray_pts[i], 2);
    lv_obj_set_style_line_color(s_ray[i], lv_color_hex(((t / 80 + (uint32_t)i) & 1) ? UK_RED : UK_GOLD), 0);
}

static void cut_set(const bomb_view_t *v) {
    bool on = v->cut != CUT_NONE, super = v->cut == CUT_SUPER;
    uk_show(s_cut_bg, super);
    for (int i = 0; i < RAYS; i++) {
        uk_show(s_ray[i], super);
        if (super) ray(i, v->cut_t);
    }
    uk_show(s_cut_face, on);
    uk_show(s_cut_name, on);
    if (!on) return;
    if (s_cut_shown != v->cut_char) {
        lv_image_set_src(s_cut_face, kit_img(hot(v->cut_char), spr_decode));
        s_cut_shown = v->cut_char;
    }
    uk_set_text(s_cut_name, v->cut_name);
    uint32_t t = v->cut_t;
    if (super) {  // 头像从左边冲进来,招式名从上面砸下来、抖两下
        int x = t < 250 ? -160 + (int)(200 * t / 250) : 40;
        lv_image_set_scale(s_cut_face, 256 * 10);
        place(s_cut_face, 16, 10, x, 60);
        int y = t < 200 ? -60 : t < 400 ? -60 + (int)(300 * (t - 200) / 200) : 240;
        int shake = t >= 400 && t < 640 ? ((t / 40) & 1 ? 4 : -4) : 0;
        lv_obj_align(s_cut_name, LV_ALIGN_TOP_MID, shake, y);
        lv_obj_set_style_text_color(s_cut_name, lv_color_hex((t / 100) & 1 ? UK_GOLD : UK_WHITE), 0);
    } else {  // 头像从屏幕下边弹上来
        int y = t < 200 ? UK_H - (int)((UK_H - 130) * t / 200) : 130;
        lv_image_set_scale(s_cut_face, 256 * BIG);
        place(s_cut_face, 16, BIG, FACE_X, y);
        lv_obj_align(s_cut_name, LV_ALIGN_TOP_MID, 0, 236);
        lv_obj_set_style_text_color(s_cut_name, lv_color_hex(UK_RED), 0);
    }
}

void bomb_ui_game_set(const bomb_view_t *v) {
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(v->flash ? UK_WHITE : UK_WALL), 0);
    uk_set_text(s_score, v->score);
    uk_set_text(s_big, v->big);
    lv_obj_set_style_text_color(s_big, lv_color_hex(v->big_color), 0);
    uk_set_text(s_hint, v->hint);
    for (int side = 0; side < 2; side++) {
        uint8_t en = side ? v->en_peer : v->en_me;
        for (int i = 0; i < 4; i++) {
            uint32_t c = i < en ? (en == 4 && !v->blink ? UK_GOLD : UK_ORANGE) : UK_LINE;
            lv_obj_set_style_bg_color(s_en[side][i], lv_color_hex(c), 0);
        }
    }
    uk_show(s_lid, v->lid);
    uk_show(s_taiji, v->taiji);
    char g[24] = "";
    if (v->glasses) snprintf(g, sizeof(g), BX_GLASSES_N, (unsigned)v->glasses);
    uk_set_text(s_glasses, g);
    // 炸弹(影分身时两个:选中的那个亮)
    bool two = v->show_bomb && v->decoy;
    uk_show(s_bomb, v->show_bomb);
    uk_show(s_bomb2, two);
    uk_show(s_spark, v->show_bomb && v->spark && !v->cage);
    uk_show(s_zap, v->show_bomb && v->zap);
    uk_show(s_cage, v->show_bomb && v->cage);
    if (v->show_bomb) {
        int x = two ? 16 : v->bomb_x;
        place(s_bomb, 16, BIG, x, v->bomb_y);
        place(s_spark, 8, 3, x + 66, v->bomb_y - 16);  // 引信头在炸弹右上角
        place(s_zap, 8, 3, x - 8, v->bomb_y + 10);
        place(s_cage, 16, BIG, x, v->bomb_y);
        lv_obj_set_style_image_recolor_opa(s_bomb, (lv_opa_t)(v->heat / 2), 0);
        lv_obj_set_style_image_opa(s_bomb, two && v->decoy != 1 ? LV_OPA_50 : LV_OPA_COVER, 0);
        if (two) {
            place(s_bomb2, 16, BIG, 128, v->bomb_y);
            lv_obj_set_style_image_opa(s_bomb2, v->decoy != 2 ? LV_OPA_50 : LV_OPA_COVER, 0);
        }
    }
    uk_show(s_boom, v->boom);
    uk_show(s_face, v->face >= 0);
    if (v->face >= 0) {
        if (s_face_char != (uint8_t)v->face) {
            lv_image_set_src(s_face, kit_img(hot((uint8_t)v->face), spr_decode));
            s_face_char = (uint8_t)v->face;
        }
        lv_obj_set_style_image_recolor(s_face, lv_color_hex(UK_INK), 0);
        lv_obj_set_style_image_recolor_opa(s_face, v->sooty ? LV_OPA_70 : LV_OPA_TRANSP, 0);
    }
    // 瞄准条
    uk_show(s_bar, v->aim);
    uk_show(s_green, v->aim);
    uk_show(s_perfect, v->aim);
    uk_show(s_arrow, v->aim);
    if (v->aim) {
        int gw = v->green_half * 2 * AIM_W / 1000, pw = v->perfect_half * 2 * AIM_W / 1000;
        lv_obj_set_pos(s_green, AIM_X + AIM_W / 2 - gw / 2, AIM_Y);
        lv_obj_set_width(s_green, gw);
        lv_obj_set_pos(s_perfect, AIM_X + AIM_W / 2 - pw / 2, AIM_Y);
        lv_obj_set_width(s_perfect, pw > 2 ? pw : 2);
        lv_obj_set_pos(s_arrow, AIM_X + v->aim_pos * AIM_W / 1000 - 2, AIM_Y - 6);
    }
    cut_set(v);
}
