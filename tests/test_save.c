// tests/test_save.c —— 存档:新存档合法、一场只记一次、里程碑、坏数据不认。
#include "bomb_save.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_reset(void) {
    bomb_save_t s;
    bomb_save_reset(&s);
    assert(bomb_save_valid(&s) && bomb_save_milestones(&s) == 0 && sizeof(s) <= 2048);
}

static void test_over_counted_once(void) {  // Review Focus 5
    bomb_save_t s;
    bomb_save_reset(&s);
    assert(bomb_save_round(&s));
    assert(bomb_save_milestones(&s) == 1);
    assert(bomb_save_over(&s, 7, true));
    assert(!bomb_save_over(&s, 7, true));  // 结束页断线再连:同一场不再记
    assert(s.wins == 1 && s.games == 1 && bomb_save_milestones(&s) == 3);
    assert(bomb_save_over(&s, 8, false) && s.wins == 1 && s.games == 2);
}

static void test_ten_wins(void) {
    bomb_save_t s;
    bomb_save_reset(&s);
    bomb_save_round(&s);
    for (uint32_t i = 1; i <= 10; i++) assert(bomb_save_over(&s, i, true));
    assert(bomb_save_milestones(&s) == 7);
}

static void test_rejects_garbage(void) {
    bomb_save_t s;
    uint32_t x = 5;
    for (int i = 0; i < 2000; i++) {
        uint8_t *p = (uint8_t *)&s;
        for (size_t k = 0; k < sizeof(s); k++) {
            x = x * 1103515245u + 12345u;
            p[k] = (uint8_t)(x >> 16);
        }
        assert(!bomb_save_valid(&s));
    }
    bomb_save_reset(&s);
    s.wins = 3;  // 赢的比下的还多
    assert(!bomb_save_valid(&s));
}

int main(void) {
    test_reset();
    test_over_counted_once();
    test_ten_wins();
    test_rejects_garbage();
    puts("test_save: ok");
    return 0;
}
