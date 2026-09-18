#!/usr/bin/env python3
"""DEV JSONL 录制器与回放统计的离线回归。"""
import json
import tempfile
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import frida_host
import replay


def main():
    frame = {
        "type": "frame", "w2c": [1.0] * 16, "proj": [1.0] * 16,
        "width": 1280, "height": 720, "inGame": True,
        "local": {"pos": [0, 0, 0], "bones": [{"valid": True}], "name": "local"},
        "players": [{"pos": [1, 2, 3], "bones": [{"valid": False}], "name": "p"}],
        "playerCount": 1,
    }
    with tempfile.TemporaryDirectory() as td:
        path = Path(td) / "frames.jsonl"
        recorder = frida_host.FrameRecorder(str(path), max_frames=2, every=2)
        recorder.write(frame)
        recorder.write(frame)
        recorder.write(frame)
        recorder.close()
        lines = path.read_text(encoding="utf-8").splitlines()
        assert len(lines) == 1 and json.loads(lines[0])["seq"] == 0
        frames = replay.load_frames(str(path))
        assert len(frames) == 1
        stats = replay.frame_stats(frames)
        assert stats["bone_total"] == 2 and stats["bone_valid"] == 1
    print("DEV JSONL recorder/replay: PASS")


if __name__ == "__main__":
    main()
