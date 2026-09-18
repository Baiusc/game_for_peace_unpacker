#!/usr/bin/env python3
"""确保 C++ 调试标注名称与 HumanBodyBones 槽位顺序一致。"""
from pathlib import Path


def main():
    path = Path(__file__).resolve().parents[1] / "cpp_overlay" / "src" / "main_win32.cpp"
    text = path.read_text(encoding="utf-8")
    expected = [
        "Hips", "LeftUpperLeg", "RightUpperLeg", "LeftLowerLeg", "RightLowerLeg",
        "LeftFoot", "RightFoot", "Spine", "Chest", "Neck", "Head",
        "LeftShoulder", "RightShoulder", "LeftUpperArm", "RightUpperArm",
        "LeftLowerArm", "RightLowerArm", "LeftHand", "RightHand",
    ]
    start = text.index("static const char* bone_names[]")
    block = text[start:text.index("};", start)]
    actual = [name for name in expected if ('"' + name + '"') in block]
    assert actual == expected, (actual, expected)
    print("bone name slot order: PASS")


if __name__ == "__main__":
    main()
