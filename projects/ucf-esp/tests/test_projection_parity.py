#!/usr/bin/env python3
"""用同一真实 Frame 样本比较 Python 与 C++ 投影结果。"""
import json
import os
import struct
import subprocess
import tempfile

import sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import esp_core  # noqa: E402


def as_xyz(value):
    if isinstance(value, dict):
        return (float(value["x"]), float(value["y"]), float(value["z"]))
    return (float(value[0]), float(value[1]), float(value[2]))


def f32(value):
    return struct.unpack("<f", struct.pack("<f", float(value)))[0]


def cpp_world_to_screen(point, matrix, width, height):
    """按 projection.cpp 的 float 运算顺序计算期望值。"""
    w = f32(f32(f32(point[0]) * f32(matrix[3])) +
            f32(f32(point[1]) * f32(matrix[7])))
    w = f32(f32(w) + f32(f32(point[2]) * f32(matrix[11])))
    w = f32(f32(w) + f32(matrix[15]))
    if w <= 0.001:
        return None
    x = f32(f32(f32(point[0]) * f32(matrix[0])) +
            f32(f32(point[1]) * f32(matrix[4])))
    x = f32(f32(x) + f32(f32(point[2]) * f32(matrix[8])))
    x = f32(f32(x) + f32(matrix[12]))
    y = f32(f32(f32(point[0]) * f32(matrix[1])) +
            f32(f32(point[1]) * f32(matrix[5])))
    y = f32(f32(y) + f32(f32(point[2]) * f32(matrix[9])))
    y = f32(f32(y) + f32(matrix[13]))
    sx = f32(f32(f32(x / w) * 0.5) + 0.5) * f32(width)
    sy = f32(f32(1.0 - f32(f32(y / w) * 0.5 + 0.5)) * f32(height))
    return (f32(sx), f32(sy))


def main():
    fixture_path = os.path.join(ROOT, "tests", "fixtures", "real_frame_level3.json")
    with open(fixture_path, encoding="utf-8") as stream:
        frame = json.load(stream)
    vp = esp_core.combine_pv(frame["proj"], frame["w2c"])
    vp = [float(f"{value:.9f}") for value in vp]
    points = []
    for player in [frame.get("local")] + frame.get("players", []):
        if not player or "pos" not in player:
            continue
        points.append(as_xyz(player["pos"]))
        for bone in player.get("bones", []) or []:
            if bone.get("valid") and bone.get("pos"):
                points.append(as_xyz(bone["pos"]))
    rows = [f"{frame['width']} {frame['height']}"]
    rows.append(" ".join(f"{float(v):.9f}" for v in vp))
    expected = []
    for point in points:
        screen = cpp_world_to_screen(point, vp, frame["width"], frame["height"])
        if screen is not None:
            expected.append((point, screen))
    rows.append(str(len(expected)))
    for point, screen in expected:
        rows.append(" ".join(f"{float(v):.9f}" for v in (*point, *screen)))
    parity_exe = os.path.join(ROOT, "cpp_overlay", "build", "test_projection_parity")
    if os.name == "nt":
        parity_exe += ".exe"
    if not os.path.isfile(parity_exe):
        raise SystemExit(f"missing parity executable: {parity_exe}; build cpp_overlay first")
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="ascii") as stream:
        stream.write("\n".join(rows) + "\n")
        input_path = stream.name
    try:
        result = subprocess.run([parity_exe, input_path], capture_output=True,
                                text=True, check=False)
    finally:
        os.unlink(input_path)
    if result.returncode != 0:
        raise SystemExit(result.stderr or f"C++ parity exit={result.returncode}")
    print("projection parity: PASS")
    print(f"fixture=real_frame_level3.json points={len(expected)} max_error<=1e-4")


if __name__ == "__main__":
    main()
