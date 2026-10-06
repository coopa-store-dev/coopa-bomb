#!/usr/bin/env python3
"""两个桌面模拟器(外壳的 tools/sim)装上库巴传炸弹,打一场:
两边按「开始」→ 连上、倒数 → 谁拿着炸弹谁过一会儿按 ● 扔 → 第 2 回合中途停住一边 3 秒(断线)再放开,
两边接着打、比分一直镜像 → 打到 3 分,两边都到结束页、胜者一样、赢的一方「赢了 1 场」。
  tests/sim_duo.py --shell 外壳仓库目录(或环境变量 COOPA_SHELL) [--font-dir DIR] [--keep]"""
import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time

GAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PG_TITLE, PG_LINK, PG_GAME = range(3)
BP_WAIT, BP_COUNT, BP_PLAY, BP_BOOM, BP_OVER = range(5)


class Card:
    def __init__(self, shell, root, name, seed, catalog, fonts):
        sys.path.insert(0, os.path.join(shell, "tools", "sim"))
        from simctl import Sim  # noqa: E402
        d = os.path.join(root, name)
        os.makedirs(d)
        self.name = name
        sock = os.path.join(d, "ctl.sock")
        self.proc = subprocess.Popen(
            [os.path.join(shell, "tools/sim/run.sh"), "--headless", "--wipe", "--seed", str(seed), "--catalog", catalog,
             "--games", "11", *(["--font-dir", fonts] if fonts else []), "--ctl", sock,
             "--state", os.path.join(d, "state"), "--out", os.path.join(d, "out")],
            stdout=open(os.path.join(d, "stdout.log"), "w"), stderr=subprocess.STDOUT)
        end = time.time() + 600
        while not os.path.exists(sock):
            if self.proc.poll() is not None or time.time() > end:
                sys.exit(f"模拟器 {name} 没起来,看 {d}/stdout.log")
            time.sleep(0.2)
        self.ctl = Sim(sock)

    def stat(self):
        f = self.ctl.cmd("stat").split()
        if len(f) < 10 or f[1] != "BM":
            return None
        keys = ["page", "phase", "hold", "me", "peer", "round", "match", "wins"]
        return {k: (v if k == "match" else int(v)) for k, v in zip(keys, f[2:])}

    def key(self, k):
        self.ctl.cmd("key " + k)
        time.sleep(0.15)

    def close(self):
        self.proc.terminate()
        self.proc.wait(10)


def wait(pred, secs, what):
    end = time.time() + secs
    while time.time() < end:
        v = pred()
        if v:
            return v
        time.sleep(0.2)
    sys.exit(f"等不到:{what}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shell", default=os.environ.get("COOPA_SHELL"))
    ap.add_argument("--font-dir")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    if not a.shell:
        ap.error("要给外壳仓库目录:--shell DIR 或环境变量 COOPA_SHELL")
    shell = os.path.abspath(a.shell)
    root = tempfile.mkdtemp(prefix="bomb-duo.")
    catalog = os.path.join(root, "catalog.toml")
    subprocess.run([sys.executable, os.path.join(GAME, "tests", "make_catalog.py"), shell, catalog], check=True,
                   stdout=subprocess.DEVNULL)
    x, y = Card(shell, root, "x", 1, catalog, a.font_dir), Card(shell, root, "y", 2, catalog, a.font_dir)
    try:
        for c in (x, y):  # 开机菜单光标在游戏上:● 进游戏,● 开始
            wait(lambda c=c: (c.key("O"), c.stat())[1], 60, f"{c.name} 进游戏")
            c.key("O")
        wait(lambda: x.stat()["page"] == PG_GAME and y.stat()["page"] == PG_GAME, 60, "两边进游戏页")
        paused = False
        end = time.time() + 600
        while time.time() < end:
            sx, sy = x.stat(), y.stat()
            if sx["phase"] == BP_OVER and sy["phase"] == BP_OVER:
                break
            if sx["phase"] in (BP_PLAY, BP_BOOM) and sy["phase"] in (BP_PLAY, BP_BOOM) and sx["round"] == sy["round"]:
                assert sx["me"] == sy["peer"] and sx["peer"] == sy["me"], (sx, sy)
            if not paused and sx["round"] == 2 and sx["phase"] == BP_PLAY:
                y.ctl.cmd("pause")  # 第 2 回合中途:一边停 3 秒
                wait(lambda: x.stat()["page"] != PG_GAME, 10, "x 发现断线")
                time.sleep(3)
                y.ctl.cmd("resume")
                wait(lambda: x.stat()["page"] == PG_GAME and y.stat()["page"] == PG_GAME, 30, "断线后接着打")
                paused = True
                continue
            for c, s in ((x, sx), (y, sy)):
                if s["phase"] == BP_PLAY and s["hold"] and random.random() < 0.3:
                    c.key("O")
            time.sleep(0.3)
        sx, sy = x.stat(), y.stat()
        assert sx["phase"] == sy["phase"] == BP_OVER, (sx, sy)
        assert sx["me"] == sy["peer"] and sx["peer"] == sy["me"] and max(sx["me"], sx["peer"]) == 3, (sx, sy)
        assert paused, "没测到断线"
        winner = x if sx["me"] == 3 else y
        assert winner.stat()["wins"] == 1
        print(f"一场打完 {sx['me']}:{sx['peer']},中途断线续上")
        print("sim_duo: ok")
        return 0
    finally:
        x.close()
        y.close()
        if a.keep:
            print("留着:", root)
        else:
            shutil.rmtree(root)


if __name__ == "__main__":
    sys.exit(main())
