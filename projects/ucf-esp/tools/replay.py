#!/usr/bin/env python3
# UCF-ESP 离线验证器：加载 frida 帧样本（JSON），投影，打印表格，
# 跑黄金样本断言，可选生成 SVG 预览。
#
# 用法：
#   python tools/replay.py                              # 默认加载 sample_session.json
#   python tools/replay.py sample_session.json --golden --html tools/sample_preview.svg
#   python tools/replay.py my_capture.json             # 你本机 frida 抓的真实帧
#
# 不带参数直接运行 => 加载 sample_session.json 并跑黄金样本断言。

import sys
import os
import json
import argparse

import esp_core
import render_html

# 相对路径兜底：允许在工程根目录执行 `python tools/replay.py sample_session.json`，
# 也允许直接给裸文件名（会在本 tools 目录里找）。
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _resolve(path):
    if os.path.exists(path):
        return path
    alt = os.path.join(SCRIPT_DIR, os.path.basename(path))
    return alt if os.path.exists(alt) else path

# 黄金样本（identity VP, 1280x720）：world -> screen
GOLDEN = {
    (-0.35, 0.55, 0.0): (416, 162),
    (0.4, -0.45, 0.0): (896, 522),
    (0.0, 0.0, 0.0): (640, 360),
}


def load_frames(path):
    with open(_resolve(path), encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, list):
        return data
    if isinstance(data, dict):
        if "frames" in data:
            return data["frames"]
        if "type" in data:
            return [data]
    return []


def check_golden(marks, verbose=True):
    by_world = {}
    for m in marks:
        if m.world:
            by_world[(round(m.world[0], 3), round(m.world[1], 3), round(m.world[2], 3))] = m
    ok = True
    for w, exp in GOLDEN.items():
        m = by_world.get(w)
        if not m or m.screen is None:
            if verbose:
                print(f"  GOLDEN MISS  world={w}")
            ok = False
            continue
        good = abs(m.screen[0] - exp[0]) <= 1 and abs(m.screen[1] - exp[1]) <= 1
        if verbose:
            status = "OK  " if good else "FAIL"
            print(f"  GOLDEN {status} world={w} -> ({m.screen[0]:.0f},{m.screen[1]:.0f}) expect {exp}")
        ok = ok and good
    return ok


def main():
    ap = argparse.ArgumentParser(description="UCF-ESP offline replay/验证器")
    ap.add_argument("path", nargs="?", default="sample_session.json")
    ap.add_argument("--golden", action="store_true", help="跑黄金样本断言")
    ap.add_argument("--html", default=None, help="生成 SVG 预览路径")
    ap.add_argument("--width", type=int, default=None)
    ap.add_argument("--height", type=int, default=None)
    args = ap.parse_args()

    frames = load_frames(args.path)
    if not frames:
        print("no frames found in", args.path)
        sys.exit(1)

    frame = frames[0]
    w = args.width or frame.get("width", 1280)
    h = args.height or frame.get("height", 720)

    vp, marks = esp_core.project_frame(frame, w, h)

    print(f"frame tick={frame.get('tick')}  canvas={w}x{h}  VP floats={len(vp)}")
    print(f"{'kind':9} {'screen':>16} {'world':>24} {'team':>4} {'hp':>9} onscreen")
    print("-" * 64)
    for mk in marks:
        s = f"({mk.screen[0]:.1f},{mk.screen[1]:.1f})" if mk.screen else "None"
        wr = f"({mk.world[0]:.2f},{mk.world[1]:.2f},{mk.world[2]:.2f})" if mk.world else "None"
        print(f"{mk.kind:9} {s:>16} {wr:>24} {str(mk.team):>4} {str(mk.hp):>9} {mk.on_screen}")

    do_golden = args.golden or args.path.endswith("sample_session.json")
    if do_golden:
        print("\n== 黄金样本断言 ==")
        assert check_golden(marks), "golden sample mismatch"

    if args.html:
        drawn = render_html.render_preview(marks, w, h, args.html, vp)
        print(f"\nSVG preview -> {args.html}  ({drawn} 个屏幕内标记)")

    print("\nreplay: DONE")


if __name__ == "__main__":
    main()
