<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Coopa Hot Potato (库巴传炸弹)

Two players, each holding a Coopa card, linked face to face, pass a bomb back and forth: when the bomb is on your
card, press ● and it flies over to the other card. Nobody knows how long the fuse is; whoever holds it when it
goes off loses the round. First to 3 rounds wins.

- One key only: ●.
- You can't throw it back during the first half second after catching it.
- The bomb gets redder and ticks faster — but its colour never tells you when it will go off.
- Disconnects are fine: the round restarts and the score is kept.

## Development

Needs shell API 2 (`kit_api = 2`, `kit_link.h`) and SDK 4.

- `make -C tests`: host tests for the match rules, link protocol and saves.
- `coopa run`: play in the browser; open the printed `duo.html` for two cards on one page.
- `coopa check`: the same checks the store runs.

All rights reserved.
