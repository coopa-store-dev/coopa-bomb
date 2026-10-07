# tests/Makefile —— 宿主测试(纯 C 的部分)。make -C tests:全部编译(AddressSanitizer + UBSan)并运行。
CC ?= cc
CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined \
          -I../src -I.
OUT := .build
HDRS := $(wildcard ../src/*.h) $(wildcard *.h)
TESTS := test_fighters test_game test_match test_save test_sprites test_peer_glue

all: $(addprefix $(OUT)/,$(TESTS))
	@for t in $(TESTS); do $(OUT)/$$t || exit 1; done
	@echo "host tests: PASS"

$(OUT):
	mkdir -p $@

$(OUT)/test_game: test_game.c duo.c ../src/bomb_game.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_match: test_match.c duo.c ../src/bomb_game.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_fighters: test_fighters.c ../src/bomb_fighters.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_save: test_save.c ../src/bomb_save.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

$(OUT)/test_sprites: test_sprites.c ../src/bomb_sprites.c stub_palette.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -Istub $(filter %.c,$^) -o $@

$(OUT)/test_peer_glue: test_peer_glue.c peer_glue.c duo.c ../src/bomb_game.c $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

.PHONY: all
