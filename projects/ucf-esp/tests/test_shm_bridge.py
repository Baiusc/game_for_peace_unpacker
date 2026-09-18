#!/usr/bin/env python3
"""离线验证 Frida 共享内存桥复用的二进制 Frame 布局。"""
import importlib.util
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, "cpp_overlay", "tools", "shm_writer.py")
SPEC = importlib.util.spec_from_file_location("ucf_shm_writer_test", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main():
    frame = MODULE.build_frame(
        [1.0] * 16, [2.0] * 16, 1280, 720, True,
        {"pos": [0.0, 1.0, 2.0], "bones": [{"pos": [1, 2, 3], "valid": True}]},
        [{"pos": [4.0, 5.0, 6.0], "hp": 75,
          "bones": [{"pos": [7, 8, 9], "valid": True}]}],
    )
    assert len(frame) == MODULE.FRAME_FMT.size
    assert MODULE.SLOT_FMT.size == 4 + MODULE.FRAME_FMT.size
    assert MODULE.SHM_NAME == "UcfFrame"
    dict_frame = MODULE.build_frame(
        [0.0] * 16, [0.0] * 16, 800, 600, True,
        {"pos": {"x": 1.0, "y": 2.0, "z": 3.0}},
        [{"pos": {"x": 4.0, "y": 5.0, "z": 6.0}, "hp": 90}],
    )
    assert len(dict_frame) == MODULE.FRAME_FMT.size
    print("shm bridge codec: PASS")
    print(f"frame_bytes={len(frame)} slot_bytes={MODULE.SLOT_FMT.size}")


if __name__ == "__main__":
    main()
