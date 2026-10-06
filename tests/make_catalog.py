#!/usr/bin/env python3
"""生成一份带库巴传炸弹(11 号,local:本仓库)的商店目录,给外壳的模拟器 / 卡上构建用。
  tests/make_catalog.py SHELL OUT.toml
SHELL = 外壳仓库目录;目录里原有的游戏照抄。"""
import sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
shell, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
src = (shell / "store" / "catalog.toml").read_text(encoding="utf-8")
src = src.replace('icons = ["icons/', f'icons = ["{shell}/store/icons/').replace('", "icons/', f'", "{shell}/store/icons/')
src = src.replace('repo = "local:', f'repo = "local:{shell}/')
out.write_text(src + f'''
[[game]]
no = 11
id = "bomb"
author = "库巴老爸"
blurb = "炸弹传来传去,炸在谁手里谁输"
repo = "local:{GAME}"
commit = ""
price = 10
starter = false
listed = true
stars = [2, 3, 4]
size_kb = 60
tags = []
icons = []
''', encoding="utf-8")
print(out)
