// src/bomb_ui.c —— 见 bomb_ui.h。控件很少(十来个),每帧只改位置、文字、颜色。图放在本游戏的精灵会话里。
#include "bomb_ui.h"

#include <stdio.h>
#include <string.h>

#include "bomb_sprites.h"
#include "bomb_texts.h"
#include "kit_sprite.h"
#include "ui_kit.h"

#define BIG 6       // 炸弹、爆炸、脸放大 6 倍(16 → 96 像素)
#define FACE_X 72
#define FACE_Y 120

static lv_obj_t *s_scr, *s_score, *s_big, *s_hint, *s_bomb, *s_spark, *s_boom, *s_face;

static lv_obj_t *centered(lv_obj_t *scr, const lv_font_t *f, uint32_t color, const char *s, int y) {
    lv_obj_t *t = uk_text(scr, f, color, s);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
    return t;
}

// 自己画的图(或借外壳的)放大 scale 倍;控件保持原图大小、以中心为轴放大。
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

void bomb_ui_title(uint8_t sel, unsigned wins) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F36, UK_INK, BX_TITLE, 30);
    centered(scr, UK_F12, UK_INK, BX_SUB, 78);
    place(image(scr, bomb_art(BS_BOMB), 4), 16, 4, 88, 100);
    const char *items[2] = { BX_START, BX_BACK };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *p = uk_pill(scr, UK_F24, sel == i ? UK_RED : UK_LINE, UK_WHITE, items[i]);
        lv_obj_align(p, LV_ALIGN_TOP_MID, 0, 186 + i * 46);
    }
    char w[32];
    snprintf(w, sizeof(w), BX_WINS, wins);
    centered(scr, UK_F12, UK_INK, w, 288);
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

void bomb_ui_game(void) {
    s_scr = uk_screen_new(UK_WALL);
    s_score = centered(s_scr, UK_F24, UK_INK, "", 22);
    s_big = centered(s_scr, UK_F36, UK_INK, "", 62);
    s_face = image(s_scr, spr_art(SPR_KID), BIG);
    place(s_face, 16, BIG, FACE_X, FACE_Y);
    lv_obj_set_style_image_recolor(s_face, lv_color_hex(UK_INK), 0);
    s_boom = image(s_scr, bomb_art(BS_BOOM), BIG);
    place(s_boom, 16, BIG, FACE_X, FACE_Y);
    s_bomb = image(s_scr, bomb_art(BS_BOMB), BIG);
    lv_obj_set_style_image_recolor(s_bomb, lv_color_hex(UK_RED), 0);
    s_spark = image(s_scr, bomb_art(BS_SPARK), 3);
    s_hint = centered(s_scr, UK_F24, UK_INK, "", 262);
    uk_screen_swap(s_scr);
}

void bomb_ui_game_set(const bomb_view_t *v) {
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(v->flash ? UK_WHITE : UK_WALL), 0);
    uk_set_text(s_score, v->score);
    uk_set_text(s_big, v->big);
    lv_obj_set_style_text_color(s_big, lv_color_hex(v->big_color), 0);
    uk_set_text(s_hint, v->hint);
    uk_show(s_bomb, v->show_bomb);
    uk_show(s_spark, v->show_bomb && v->spark);
    if (v->show_bomb) {
        place(s_bomb, 16, BIG, v->bomb_x, v->bomb_y);
        place(s_spark, 8, 3, v->bomb_x + 66, v->bomb_y - 16);  // 引信头在炸弹右上角
        lv_obj_set_style_image_recolor_opa(s_bomb, (lv_opa_t)(v->heat / 2), 0);
    }
    uk_show(s_boom, v->boom);
    uk_show(s_face, v->sooty);
    lv_obj_set_style_image_recolor_opa(s_face, LV_OPA_70, 0);
}
