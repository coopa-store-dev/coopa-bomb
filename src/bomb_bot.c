// src/bomb_bot.c —— 见 bomb_bot.h。
#include "bomb_bot.h"

typedef struct {
    uint16_t react_ms, react_jitter;  // 接到后反应多久(± 抖动)
    uint16_t err_ms;                  // 按早按晚的误差(±)
    uint8_t p_super, p_special;       // 能量够时放招的概率(%)
    uint16_t mash_ms;                 // 砸笼子的间隔
} bot_param_t;

static const bot_param_t P[BOT_LEVELS] = {
    [BOT_EASY] = { 700, 300, 220, 0, 10, 220 },
    [BOT_NORMAL] = { 450, 200, 130, 50, 35, 160 },
    [BOT_HARD] = { 250, 100, 50, 80, 50, 120 },
};

void bomb_bot_init(bomb_bot_t *b, uint8_t level, uint32_t (*rnd)(void)) {
    *b = (bomb_bot_t){ .level = level < BOT_LEVELS ? level : BOT_NORMAL, .rnd = rnd };
}

static int32_t spread(bomb_bot_t *b, uint32_t half) { return half ? (int32_t)(b->rnd() % (2 * half + 1)) - (int32_t)half : 0; }

static bool chance(bomb_bot_t *b, uint8_t pct) { return b->rnd() % 100 < pct; }

// 箭头离中点还有多久到(往前走、碰头折回);刚过中点多久。单位毫秒。
static void center_times(const bomb_seen_t *s, int32_t *to, int32_t *since) {
    int32_t v = s->aim.speed_x1000 ? s->aim.speed_x1000 : 1, p = s->aim_pos, mid = BOMB_AIM_MID;
    int32_t dist_to, dist_since;
    if (s->aim_dir > 0) {
        dist_to = p <= mid ? mid - p : (1000 - p) + (1000 - mid);
        dist_since = p >= mid ? p - mid : p + mid;
    } else {
        dist_to = p >= mid ? p - mid : p + mid;
        dist_since = p <= mid ? mid - p : (1000 - p) + (1000 - mid);
    }
    *to = dist_to * 1000 / v;
    *since = dist_since * 1000 / v;
}

int bomb_bot_step(bomb_bot_t *b, const bomb_seen_t *s, uint32_t dt) {
    const bot_param_t *p = &P[b->level];
    if (s->phase != BP_PLAY || !s->holding) {
        b->ready = b->decoy_done = false;
        b->wait_ms = 0;
        return -1;
    }
    if (s->frozen) return -1;
    if (s->cage_left) {  // 砸笼子
        b->wait_ms -= (int32_t)dt;
        if (b->wait_ms > 0) return -1;
        b->wait_ms = p->mash_ms + spread(b, p->mash_ms / 4);
        return BK_OK;
    }
    if (!b->ready) {  // 刚能动:先反应一会儿
        b->ready = true;
        b->wait_ms = p->react_ms + spread(b, p->react_jitter);
        b->err_ms = spread(b, p->err_ms);
        return -1;
    }
    if (b->wait_ms > 0) {
        b->wait_ms -= (int32_t)dt;
        if (b->wait_ms > 0) return -1;
        if (s->decoy && !b->decoy_done) {  // 两个炸弹:看不出来,随便选一个
            b->decoy_done = true;
            return b->rnd() & 1 ? BK_UP : BK_DOWN;
        }
        if (s->can_throw && s->en_me >= 4 && chance(b, p->p_super)) {
            b->casts[1]++;
            return BK_DOWN;
        }
        if (s->can_throw && s->en_me >= 2 && chance(b, p->p_special)) {
            b->casts[0]++;
            return BK_UP;
        }
    }
    if (!s->can_throw && !s->decoy) return -1;  // 弹回 / 接住冷却 / 砸开后
    int32_t to, since;
    center_times(s, &to, &since);
    bool go = b->err_ms >= 0 ? to <= b->err_ms : (since >= -b->err_ms && since < -b->err_ms + (int32_t)dt + 20);
    if (!go) return -1;
    b->presses++;
    b->err_ms = spread(b, p->err_ms);  // 下一下(扔歪了还得再按)重新抽
    return BK_OK;
}
