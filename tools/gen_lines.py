#!/usr/bin/env python3
"""生成库巴传炸弹四个角色的专属语音(选人、必杀、超必杀、赢了),编进游戏里。

用微软神经网络语音合成(edge-tts 包,要联网),转成 16 kHz 单声道,去掉小喇叭放不出的低频、
裁掉前后静音、把峰值调到 16000,存成 IMA ADPCM(ffmpeg adpcm_ima_wav,块 1024 字节,trellis 搜索),
格式和 Coopa OS 内置台词一样,用 sfx_play_line 播放。

台词规矩:只说自己,或者对着「你」说,不点对方是谁(对面可能是任何一个家人)。

用法:brew install ffmpeg;python3 -m venv ~/.venvs/coopa-tts && ~/.venvs/coopa-tts/bin/pip install edge-tts
      ~/.venvs/coopa-tts/bin/python tools/gen_lines.py [--all]
写 src/bomb_lines.h、src/bomb_lines.c(都是生成的,改台词改这里)。
名字和台词都没变的句子保留原来的音频(合成服务两次输出不完全一样);--all 全部重新合成。
"""

import math
import re
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = ROOT / "src" / "bomb_lines.h"
C = ROOT / "src" / "bomb_lines.c"

# (声音, 音调, 语速)
GRANNY = ("zh-CN-shaanxi-XiaoniNeural", "-10Hz", "+0%")   # 陕西话
GRANDPA = ("zh-CN-YunjianNeural", "-15Hz", "-10%")
DAD = ("zh-CN-YunjianNeural", "+0Hz", "+5%")
KID = ("zh-CN-YunxiaNeural", "+0Hz", "+5%")               # 小男孩

FIGHTERS = [("KID", KID), ("DAD", DAD), ("GRANNY", GRANNY), ("GRANDPA", GRANDPA)]  # 顺序 = bomb_fighter_t
KINDS = ["PICK", "SPECIAL", "SUPER", "WIN"]  # 顺序 = 数组名 BL_PICK / BL_SPECIAL / BL_SUPER / BL_WIN
TEXT = {
    "KID": ["库巴来啦！", "弹弓，发射！", "影分身之术！", "耶！我赢啦！"],
    "DAD": ["放马过来吧！", "火箭快递，使命必达！", "铁笼！给我关起来！", "谁也别想赢过我！"],
    "GRANNY": ["看奶奶的厉害！", "听奶奶念叨念叨！", "锅盖神功！", "服不服？"],
    "GRANDPA": ["爷爷来陪你耍。", "戴上老花镜，看清楚喽！", "太极推手——走你！", "姜还是老的辣！"],
}

HIGHPASS_HZ = 120
TARGET_PEAK = 16000
SAMPLE_RATE = 16000
TRIM_LEVEL = 300
PAD = SAMPLE_RATE * 30 // 1000
ADPCM_BLOCK = 1024
ADPCM_BLOCK_SAMPLES = 1 + (ADPCM_BLOCK - 4) * 2


def synth(text: str, tmp: Path, voice: tuple[str, str, str]) -> list[int]:
    mp3, wav = tmp / "line.mp3", tmp / "line.wav"
    name, pitch, rate = voice
    subprocess.run([sys.executable, "-m", "edge_tts", "--voice", name, f"--pitch={pitch}", f"--rate={rate}",
                    "--text", text, "--write-media", str(mp3)], check=True, stderr=subprocess.DEVNULL)
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", str(mp3), "-ac", "1", "-ar", str(SAMPLE_RATE),
                    "-sample_fmt", "s16", str(wav)], check=True)
    with wave.open(str(wav)) as w:
        n = w.getnframes()
        samples = list(struct.unpack(f"<{n}h", w.readframes(n)))
    if n < SAMPLE_RATE // 10:
        sys.exit(f"语音合成几乎没出声:{text!r}(声音 {name} 可用吗?)")
    return samples


def process(samples: list[int]) -> list[int]:
    rc = 1.0 / (2 * math.pi * HIGHPASS_HZ)
    a = rc / (rc + 1.0 / SAMPLE_RATE)
    out, px, py = [], 0.0, 0.0
    for x in samples:
        py = a * (py + x - px)
        px = x
        out.append(py)
    loud = [i for i, s in enumerate(out) if abs(s) >= TRIM_LEVEL]
    clip = out[max(loud[0] - PAD, 0):min(loud[-1] + PAD + 1, len(out))]
    gain = TARGET_PEAK / max(abs(s) for s in clip)
    return [max(-32768, min(32767, round(s * gain))) for s in clip]


def adpcm(pcm: list[int], tmp: Path) -> bytes:
    src, dst = tmp / "pcm.wav", tmp / "adpcm.wav"
    with wave.open(str(src), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", str(src), "-c:a", "adpcm_ima_wav",
                    "-block_size", str(ADPCM_BLOCK), "-trellis", "16", str(dst)], check=True)
    raw = dst.read_bytes()
    k = raw.index(b"data")
    data = raw[k + 8:k + 8 + struct.unpack("<I", raw[k + 4:k + 8])[0]]
    full, rest = divmod(len(pcm), ADPCM_BLOCK_SAMPLES)
    size = full * ADPCM_BLOCK + (4 + rest // 2 if rest else 0)
    assert len(data) >= size
    return data[:size]


def existing() -> dict[str, tuple[str, bytes, int]]:
    """名字 → (台词, ADPCM, 样本数):已经生成过的。"""
    if not H.exists() or not C.exists():
        return {}
    texts = dict(re.findall(r"^// (\w+): (.*)$", H.read_text(), re.M))
    src = C.read_text()
    out = {}
    for name, body in re.findall(r"static const uint8_t L_(\w+)\[\] = \{([^}]*)\};", src):
        n = re.search(rf'\{{ L_{name}, (\d+), "{name}" \}}', src)
        if name in texts and n:
            out[name] = (texts[name], bytes(int(b, 16) for b in re.findall(r"0x([0-9a-f]{2})", body)), int(n.group(1)))
    return out


def main() -> int:
    redo = "--all" in sys.argv
    old = existing()
    lines = []  # (名字, 台词, ADPCM, 样本数)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for kind_i, kind in enumerate(KINDS):
            for fighter, voice in FIGHTERS:
                name, text = f"{kind}_{fighter}", TEXT[fighter][kind_i]
                if not redo and name in old and old[name][0] == text:
                    lines.append((name, text, old[name][1], old[name][2]))
                    continue
                pcm = process(synth(text, tmp, voice))
                lines.append((name, text, adpcm(pcm, tmp), len(pcm)))
                print(f"  {name}: {text}  {len(pcm) / SAMPLE_RATE:.2f} 秒", flush=True)
    h = ["// src/bomb_lines.h —— 四个角色的专属语音(tools/gen_lines.py 生成,勿手改)。下标 = bomb_fighter_t。",
         "// 格式同 Coopa OS 内置台词(16 kHz IMA ADPCM),用 sfx_play_line 播放。台词:", ""]
    h += [f"// {name}: {text}" for name, text, _, _ in lines]
    h += ["", "#pragma once", "", '#include "kit_lines.h"', "",
          "extern const kit_line_t BL_PICK[4], BL_SPECIAL[4], BL_SUPER[4], BL_WIN[4];", ""]
    H.write_text("\n".join(h))
    c = ["// src/bomb_lines.c —— tools/gen_lines.py 生成,勿手改。", '#include "bomb_lines.h"', ""]
    for name, _, data, _ in lines:
        c.append(f"static const uint8_t L_{name}[] = {{")
        for i in range(0, len(data), 16):
            c.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
        c.append("};")
    c.append("")
    for kind in KINDS:
        items = [f'{{ L_{kind}_{f}, {n}, "{kind}_{f}" }}' for f, _ in FIGHTERS
                 for name, _, _, n in lines if name == f"{kind}_{f}"]
        c.append(f"const kit_line_t BL_{kind}[4] = {{")
        c += [f"    {it}," for it in items]
        c.append("};")
    c.append("")
    C.write_text("\n".join(c))
    total = sum(len(x[2]) for x in lines)
    longest = max(x[3] for x in lines) / SAMPLE_RATE
    print(f"16 句,共 {total / 1024:.0f} KB,最长 {longest:.2f} 秒")
    return 0


if __name__ == "__main__":
    sys.exit(main())
