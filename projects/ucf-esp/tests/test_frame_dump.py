#!/usr/bin/env python3
"""离线验证 --dump-frame 的限流、命名和 Frame JSON 可回读。"""
import json
import os
import tempfile

import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
from frida_host import FrameDumpWriter  # noqa: E402


def main():
    frame = {
        "type": "frame", "w2c": [1.0] * 16, "proj": [2.0] * 16,
        "width": 1280, "height": 720, "inGame": True,
        "local": {"pos": [0, 0, 0], "bones": []},
        "players": [{"pos": [1, 2, 3], "hp": 100, "maxHp": 100,
                      "bones": []}], "playerCount": 1,
    }
    with tempfile.TemporaryDirectory() as directory:
        writer = FrameDumpWriter(directory, every=2, max_frames=2)
        for _ in range(6):
            writer.write(frame)
        files = sorted(name for name in os.listdir(directory) if name.endswith(".json"))
        assert len(files) == 2
        assert files[0].startswith("real_frame_") and files[0].endswith("_000000.json")
        loaded = json.loads(open(os.path.join(directory, files[0]), encoding="utf-8").read())
        assert loaded["type"] == "frame" and loaded["players"][0]["pos"] == [1, 2, 3]
    print("frame dump: PASS")
    print("files=2 fields=type,w2c,proj,local,players,playerCount")


if __name__ == "__main__":
    main()
