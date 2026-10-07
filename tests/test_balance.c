// tests/test_balance.c —— 调数值:两个电脑(普通)把 16 种角色组合各打 N 场,打印统计并检查设计 §10 的目标;
// 再让「厉害」打「简单」。场数可以用第一个参数改(默认 100)。
#include "bomb_bot.h"
#include "duo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const char *NAME[BF_COUNT] = { "kid", "dad", "granny", "grandpa" };

typedef struct {
    uint32_t ms;
    uint32_t supers, specials, presses, misses, perfects;  // 两个人加起来
    bool a_won;
} result_t;

static result_t play(uint8_t ca, uint8_t cb, uint8_t la, uint8_t lb, uint32_t seed) {
    duo_seed(seed);
    duo_init();
    duo_connect();
    flow();
    duo_pick(ca, cb);
    bomb_bot_t ba, bb;
    bomb_bot_init(&ba, la, duo_rnd);
    bomb_bot_init(&bb, lb, duo_rnd);
    result_t r = { 0 };
    bomb_t *g[2] = { &D.a, &D.b };
    bomb_bot_t *bot[2] = { &ba, &bb };
    while (!(D.a.phase == BP_OVER && D.b.phase == BP_OVER)) {
        assert(r.ms < 30u * 60u * 1000u);
        for (int i = 0; i < 2; i++) {
            bomb_seen_t s;
            bomb_seen(g[i], &s);
            int k = bomb_bot_step(bot[i], &s, 10);
            if (k >= 0) bomb_key(g[i], (uint8_t)k);
        }
        run(10);
        r.ms += 10;
        for (int i = 0; i < 2; i++) {
            uint8_t ev;
            while (bomb_event(g[i], &ev)) {
                r.misses += ev == BE_MISS;
                r.perfects += ev == BE_PERFECT;
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        r.supers += bot[i]->casts[1];
        r.specials += bot[i]->casts[0];
        r.presses += bot[i]->presses;
    }
    r.a_won = D.a.score_me >= BOMB_WIN;
    return r;
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 100;
    uint32_t wins[BF_COUNT] = { 0 }, games[BF_COUNT] = { 0 };
    uint32_t pair_w[BF_COUNT][BF_COUNT] = { { 0 } }, pair_n[BF_COUNT][BF_COUNT] = { { 0 } };
    double ms = 0, supers = 0, specials = 0, presses = 0, misses = 0, perfects = 0;
    uint32_t total = 0, seed = 1;
    for (uint8_t ca = 0; ca < BF_COUNT; ca++) {
        for (uint8_t cb = 0; cb < BF_COUNT; cb++) {
            for (int i = 0; i < n; i++) {
                result_t r = play(ca, cb, BOT_NORMAL, BOT_NORMAL, seed++);
                total++;
                ms += r.ms;
                supers += r.supers;
                specials += r.specials;
                presses += r.presses;
                misses += r.misses;
                perfects += r.perfects;
                if (ca == cb) continue;
                uint8_t w = r.a_won ? ca : cb, l = r.a_won ? cb : ca;
                wins[w]++;
                games[ca]++;
                games[cb]++;
                pair_w[w][l]++;
                pair_n[ca][cb]++;
                pair_n[cb][ca]++;
            }
        }
    }
    double avg_s = ms / total / 1000, sup = supers / total / 2, spe = specials / total / 2;
    printf("  %u 场(普通对普通):平均 %.0f 秒;每人每场 必杀 %.2f、超必杀 %.2f;扔歪 %.0f%%、完美 %.0f%%\n", total, avg_s,
           spe, sup, 100 * misses / presses, 100 * perfects / presses);
    bool ok = avg_s >= 90 && avg_s <= 180 && sup >= 1 && sup <= 3;
    for (uint8_t c = 0; c < BF_COUNT; c++) {
        double w = 100.0 * wins[c] / games[c];
        printf("  %-8s 胜率 %.0f%%:", NAME[c], w);
        for (uint8_t o = 0; o < BF_COUNT; o++) {
            if (o == c) continue;
            double pw = 100.0 * pair_w[c][o] / pair_n[c][o];
            printf(" 对 %s %.0f%%", NAME[o], pw);
            ok = ok && pw >= 30 && pw <= 70;
        }
        printf("\n");
        ok = ok && w >= 40 && w <= 60;
    }
    uint32_t hard = 0, m = 3u * (uint32_t)n;
    for (uint32_t i = 0; i < m; i++) {
        uint8_t ca = (uint8_t)(i % BF_COUNT), cb = (uint8_t)(i / BF_COUNT % BF_COUNT);
        bool hard_is_a = i & 1;
        result_t r = play(ca, cb, hard_is_a ? BOT_HARD : BOT_EASY, hard_is_a ? BOT_EASY : BOT_HARD, seed++);
        hard += r.a_won == hard_is_a;
    }
    double hw = 100.0 * hard / m;
    printf("  厉害对简单 %u 场:厉害赢 %.0f%%\n", m, hw);
    ok = ok && hw >= 75 && hw <= 95;
    if (!ok) {
        puts("test_balance: 数值没达到设计 §10 的目标");
        return 1;
    }
    puts("test_balance: ok");
    return 0;
}
