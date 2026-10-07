#!/usr/bin/env python3
"""两个桌面模拟器(外壳的 tools/sim)装上库巴传炸弹,打一场:
两边选「两人联机」→ 连上、选人(x 库巴、y 爷爷)、倒数 → 拿着炸弹的一方等箭头进绿区按 ●,能量够了放招
→ 第 2 回合中途停住一边 3 秒(断线)再放开,两边接着打、比分一直镜像 → 打到 3 分,两边都到结束页、
胜者一样、赢的一方「赢了 1 场」;两边都放过招。
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
BP_WAIT, BP_COUNT, BP_PLAY, BP_BOOM, BP_OVER, BP_PICK = range(6)


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
        if len(f) < 18 or f[1] != "BM":
            return None
        keys = ["page", "phase", "hold", "me", "peer", "round", "match", "wins", "mode", "char", "peer_char", "en",
                "peer_en", "stage", "aim", "can"]
        return {k: (v if k == "match" else int(v)) for k, v in zip(keys, f[2:])}

    def enter_game(self):
        """开机菜单光标在游戏上:按一次 ●,等游戏首页出来再按下一次(多按就直接进了「和电脑打」)。"""
        end = time.time() + 60
        while self.stat() is None:
            if time.time() > end:
                sys.exit(f"{self.name} 进不了游戏")
            self.key("O")
            for _ in range(25):
                if self.stat():
                    break
                time.sleep(0.2)
        assert self.stat()["page"] == PG_TITLE

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
        for c in (x, y):  # 开机菜单光标在游戏上:● 进游戏,▼ 到「两人联机」,● 开始
            c.enter_game()
            c.key("D")
            c.key("O")
        wait(lambda: x.stat()["phase"] == BP_PICK and y.stat()["phase"] == BP_PICK, 60, "两边进选人页")
        time.sleep(0.6)  # 刚进选人页 0.4 秒不收键
        x.key("O")  # 库巴(光标默认在上次的人:新存档是库巴)
        for _ in range(3):
            y.key("D")  # 爷爷
        y.key("O")
        wait(lambda: x.stat()["page"] == PG_GAME and x.stat()["phase"] not in (BP_PICK, BP_WAIT), 60, "开打")
        sx, sy = x.stat(), y.stat()
        assert sx["char"] == 0 and sx["peer_char"] == 3 and sy["char"] == 3 and sy["peer_char"] == 0, (sx, sy)
        casts = {"x": 0, "y": 0}
        paused = False
        end = time.time() + 600
        while time.time() < end:
            sx, sy = x.stat(), y.stat()
            if sx["phase"] == BP_OVER and sy["phase"] == BP_OVER:
                break
            # 两边同一阶段才比(一边刚炸、BOOM 还在路上时比分本来就差一拍)
            if sx["phase"] == sy["phase"] and sx["phase"] in (BP_PLAY, BP_BOOM) and sx["round"] == sy["round"]:
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
                if s["phase"] != BP_PLAY or not s["hold"] or not s["can"]:
                    continue
                if s["en"] >= 2 and random.random() < 0.5:  # 放招(超必杀 / 必杀)
                    c.key("D" if s["en"] >= 4 else "U")
                    casts[c.name] += 1
                elif 420 <= s["aim"] <= 580:
                    c.key("O")
            time.sleep(0.05)
        sx, sy = x.stat(), y.stat()
        assert sx["phase"] == sy["phase"] == BP_OVER, (sx, sy)
        assert sx["me"] == sy["peer"] and sx["peer"] == sy["me"] and max(sx["me"], sx["peer"]) == 3, (sx, sy)
        assert paused, "没测到断线"
        assert casts["x"] and casts["y"], casts
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
