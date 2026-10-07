<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Coopa Hot Potato (库巴传炸弹)

A family brawler version of hot potato: pick a family member, throw the bomb across, build up energy and unleash
specials and supers. Nobody knows how long the fuse is; whoever holds it when it goes off loses the round. First to 3
rounds wins.

- **Vs computer**: play alone against the other three family members (easy → normal → hard). Beat all three and that
  character gets a crown on the pick screen.
- **Two players**: each holds a Coopa card, face to face.

## How to play

- **● throw**: an arrow swings left and right; press ● while it is in the green zone to throw, the gold centre is a
  "perfect". Miss and the bomb bounces back. The hotter the bomb, the faster the arrow.
- **Energy**: 4 cells each. Perfect +1, hit by the other side's move +1, losing a round +2.
- **▲ special** (2 cells) and **▼ super** (4 cells): only while you hold the bomb; a move always lands.

| Character | ▲ Special | ▼ Super |
|---|---|---|
| Coopa | Slingshot: the other side is dazed for 1.5 s | Shadow clone: two bombs arrive, only one is real |
| Dad | Rocket delivery: the fuse burns twice as fast over there | Iron cage: press ● 8 times to break it open |
| Grandma | Nagging: the other side's arrow speeds up, the green zone shrinks | Pot-lid shield: the next time it goes off in her hands, the lid bounces it back |
| Grandpa | Reading glasses: his next 3 throws aim slower and wider | Tai chi push: the next throw coming in is pushed straight back |

- You can't throw it back during the first half second after catching it. A super plays a close-up on both cards and
  the fuse on the other side waits for it.
- The bomb gets redder and ticks faster — but its colour never tells you when it will go off.
- Disconnects are fine: the round restarts, scores and energy are kept.

## Development

Needs shell API 2 (`kit_api = 2`, `kit_link.h`) and SDK 4.

- `make -C tests`: host tests for the rules, moves, link protocol, saves and the computer player; `test_balance` plays
  thousands of matches to check the numbers. With `COOPA_SHELL=<shell dir>` it also decodes the voice lines.
- `tools/gen_lines.py`: regenerate the four characters' voice lines (needs edge-tts and ffmpeg).
- `tests/sim_cpu.py`, `tests/sim_duo.py`: play the ladder / a two-card match in the desktop simulator.
- `coopa run`: play in the browser; open the printed `duo.html` for two cards on one page.
- `coopa check`: the same checks the store runs.

All rights reserved.
