# tests/Makefile —— 宿主测试(纯 C 的部分)。make -C tests:全部编译(AddressSanitizer + UBSan)并运行。
CC ?= cc
CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined \
          -I../src -I.
OUT := .build
HDRS := $(wildcard ../src/*.h) $(wildcard *.h)
TESTS := test_fighters test_game test_throw test_skills test_local test_balance test_match test_save test_sprites test_peer_glue

# 语音解码测试要外壳的解码器:make -C tests COOPA_SHELL=外壳目录(没给就跳过)
ifneq ($(COOPA_SHELL),)
TESTS += test_lines
endif

all: $(addprefix $(OUT)/,$(TESTS))
	@for t in $(TESTS); do $(OUT)/$$t || exit 1; done
	@echo "host tests: PASS"

$(OUT):
	mkdir -p $@

$(OUT)/test_game: test_game.c duo.c ../src/bomb_game.c ../src/bomb_fighters.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_throw: test_throw.c duo.c ../src/bomb_game.c ../src/bomb_fighters.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_skills: test_skills.c duo.c ../src/bomb_game.c ../src/bomb_fighters.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

GAME := ../src/bomb_game.c ../src/bomb_fighters.c ../src/bomb_bot.c

$(OUT)/test_local: test_local.c ../src/bomb_local.c $(GAME) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

# 数值统计跑几千场:开优化、不开 sanitizer(规则在别的测试里已经在 ASan 下跑过)
$(OUT)/test_balance: test_balance.c duo.c $(GAME) $(HDRS) | $(OUT)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -I../src -I. $(filter %.c,$^) -o $@

$(OUT)/test_match: test_match.c duo.c $(GAME) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_fighters: test_fighters.c ../src/bomb_fighters.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_lines: test_lines.c ../src/bomb_lines.c $(COOPA_SHELL)/main/kit_voice_dsp.c | $(OUT)
	$(CC) $(CFLAGS) -I$(COOPA_SHELL)/main $(filter %.c,$^) -o $@

$(OUT)/test_save: test_save.c ../src/bomb_save.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_sprites: test_sprites.c ../src/bomb_sprites.c stub_palette.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -Istub $(filter %.c,$^) -o $@

$(OUT)/test_peer_glue: test_peer_glue.c peer_glue.c duo.c ../src/bomb_game.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

.PHONY: all
