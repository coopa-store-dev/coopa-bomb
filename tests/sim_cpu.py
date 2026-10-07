#!/usr/bin/env python3
"""一个桌面模拟器(外壳的 tools/sim)装上库巴传炸弹,和电脑闯三关:
首页「和电脑打」→ 选奶奶 → 每关:箭头进绿区按 ●、能量够了放招 → 赢了 ● 下一关、输了 ● 再打一次
→ 三关都赢(通关)→ ● 再闯一次回到选人页。中途不开蓝牙(状态行的模式 = 1)。
脚本隔着控制接口按键有延迟,不一定打得过「厉害」:打到第 3 关、用过「再打一次」也算过(最多 12 场)。
  tests/sim_cpu.py --shell 外壳仓库目录(或环境变量 COOPA_SHELL) [--font-dir DIR] [--keep] [--shots DIR]"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

GAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BP_WAIT, BP_COUNT, BP_PLAY, BP_BOOM, BP_OVER, BP_PICK = range(6)
KEYS = ["page", "phase", "hold", "me", "peer", "round", "match", "wins", "mode", "char", "peer_char", "en", "peer_en",
        "stage", "aim", "can"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shell", default=os.environ.get("COOPA_SHELL"))
    ap.add_argument("--font-dir")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--shots", help="把各个画面截图存到这个目录")
    a = ap.parse_args()
    if not a.shell:
        ap.error("要给外壳仓库目录:--shell DIR 或环境变量 COOPA_SHELL")
    shell = os.path.abspath(a.shell)
    sys.path.insert(0, os.path.join(shell, "tools", "sim"))
    from simctl import Sim  # noqa: E402
    root = tempfile.mkdtemp(prefix="bomb-cpu.")
    catalog = os.path.join(root, "catalog.toml")
    subprocess.run([sys.executable, os.path.join(GAME, "tests", "make_catalog.py"), shell, catalog], check=True,
                   stdout=subprocess.DEVNULL)
    sock = os.path.join(root, "ctl.sock")
    proc = subprocess.Popen(
        [os.path.join(shell, "tools/sim/run.sh"), "--headless", "--wipe", "--seed", "5", "--catalog", catalog,
         "--games", "11", *(["--font-dir", a.font_dir] if a.font_dir else []), "--ctl", sock,
         "--state", os.path.join(root, "state"), "--out", os.path.join(root, "out")],
        stdout=open(os.path.join(root, "stdout.log"), "w"), stderr=subprocess.STDOUT)
    end = time.time() + 600
    while not os.path.exists(sock):
        if proc.poll() is not None or time.time() > end:
            sys.exit(f"模拟器没起来,看 {root}/stdout.log")
        time.sleep(0.2)
    c = Sim(sock)
    shots = set()

    def stat():
        f = c.cmd("stat").split()
        return {k: (v if k == "match" else int(v)) for k, v in zip(KEYS, f[2:])} if len(f) >= 18 and f[1] == "BM" else None

    def key(k):
        c.cmd("key " + k)
        time.sleep(0.12)

    def shot(name):
        if a.shots and name not in shots:
            shots.add(name)
            c.cmd(f"shot {os.path.join(os.path.abspath(a.shots), name)}.png")

    try:
        end = time.time() + 60
        while not stat():  # 开机菜单光标在游戏上:按一次 ●,等游戏首页出来(别多按,多按就进了「和电脑打」)
            assert time.time() < end, "进不了游戏"
            key("O")
            for _ in range(25):
                if stat():
                    break
                time.sleep(0.2)
        assert stat()["page"] == 0
        shot("1-title")
        key("O")  # 和电脑打
        time.sleep(0.8)  # 刚进选人页 0.4 秒不收键
        shot("2-pick")
        key("D")
        key("D")  # 奶奶
        key("O")
        cleared, matches, casts, retried, top, end = False, 0, 0, False, 1, time.time() + 1500
        while time.time() < end:
            s = stat()
            assert s["mode"] == 1, s
            if s["phase"] == BP_PLAY and s["hold"] and s["can"]:
                shot("3-aim")
                if s["en"] >= 4 or (s["en"] >= 2 and s["peer_en"] >= 3):
                    key("D" if s["en"] >= 4 else "U")
                    casts += 1
                    time.sleep(0.3)
                    shot("4-super" if s["en"] >= 4 else "5-special")
                elif 450 <= s["aim"] <= 550:
                    key("O")
            elif s["phase"] == BP_PLAY and not s["hold"]:
                shot("6-away")
            elif s["phase"] == BP_OVER:
                time.sleep(1.3)  # 结束页前 1 秒不收键
                s = stat()
                matches += 1
                won = s["me"] == 3
                retried |= not won
                top = max(top, s["stage"])
                print(f"  第 {s['stage']} 关 对 {s['peer_char']}:{s['me']}:{s['peer']} {'赢' if won else '输'}", flush=True)
                if won and s["stage"] == 3:
                    shot("8-clear")
                    cleared = True
                    key("O")  # 再闯一次:回选人页
                    time.sleep(0.8)
                    s = stat()
                    assert s["phase"] == BP_PICK and s["stage"] == 1, s
                    break
                shot("7-over")
                if matches >= 12:
                    break
                key("O")
                time.sleep(0.8)
            time.sleep(0.03)
        assert cleared or (top == 3 and retried), f"没通关,也没打到第 3 关(到第 {top} 关)"
        assert casts > 0
        print(f"{'通关' if cleared else '打到第 3 关'}:{matches} 场,放招 {casts} 次")
        print("sim_cpu: ok")
        return 0
    finally:
        proc.terminate()
        proc.wait(10)
        if a.keep:
            print("留着:", root)
        else:
            shutil.rmtree(root)


if __name__ == "__main__":
    sys.exit(main())
