#!/usr/bin/env python3
# UCF 共享内存写入端（Windows）：把帧写入命名共享内存 "UcfFrame"，
# 与 cpp_overlay 的 SharedTransport 对接（C++ 用 CreateFileMapping 打开同名对象）。
#
# 用途：把你现有 frida_host.py 读到的 w2c/proj/players 打包成二进制帧写进去，
# C++ 叠加层每帧 OpenFileMapping + 读。二进制布局与 shared_state.hpp 的 FrameSlots
# 严格一致（小端、4 字节对齐），避免读到半帧（写者整帧拷贝后才翻转 cur）。
#
# 本脚本自带一个合成数据源做演示：不连游戏也能验证“Python -> C++ 共享内存”链路。
# 用法（Windows）：python tools/shm_writer.py
import ctypes
import mmap
import struct
import math
import time
import sys

# ---- 与 C++ Frame / PlayerState 严格对齐的二进制布局（小端 <）----------------
N_PLAYERS = 64
N_BONES = 19
BONE_FMT = "3f?3x"                              # pos(12)+valid+pad = 16
PS_FMT = struct.Struct("<3fiii?32s3x" + BONE_FMT * N_BONES)
FRAME_FMT = struct.Struct(
    "<16f16f"          # w2c[16], proj[16]
    "ii"               # width, height
    "?" "3x"           # inGame + 补齐到 4 字节
    + PS_FMT.format[1:] * (1 + N_PLAYERS)   # local + players[]
    + "i"              # playerCount
)
# FrameSlots = int cur + Frame
SLOT_FMT = struct.Struct("<i" + FRAME_FMT.format[1:])
SHM_NAME = "UcfFrame"
# 共享区 = cur(4) + 两个固定长度 Frame 槽；SLOT_FMT 只是单槽布局描述。
SHM_SIZE = 4 + 2 * FRAME_FMT.size


def _vec3(value, default=(0.0, 0.0, 0.0)):
    """兼容 Frame JSON 的 {x,y,z} 与共享内存写入所需的 [x,y,z]。"""
    if isinstance(value, dict):
        return (float(value.get("x", default[0])),
                float(value.get("y", default[1])),
                float(value.get("z", default[2])))
    if isinstance(value, (list, tuple)) and len(value) >= 3:
        return (float(value[0]), float(value[1]), float(value[2]))
    return tuple(float(v) for v in default)


def _ps_items(p):
    name = (p.get("name", "")[:31].encode("utf-8", "replace") + b"\x00" * 32)[:32]
    pos = _vec3(p.get("pos"))
    items = [pos[0], pos[1], pos[2],
            int(p.get("team", 0)), int(p.get("hp", 100)),
            int(p.get("maxHp", 100)), bool(p.get("isDead", False)), name]
    bones = p.get("bones", []) or []
    for i in range(N_BONES):
        b = bones[i] if i < len(bones) else {}
        pos = b.get("pos", [0.0, 0.0, 0.0])
        items.extend([float(pos[0]), float(pos[1]), float(pos[2]), bool(b.get("valid", False))])
    return tuple(items)


def build_frame(w2c, proj, width, height, in_game, local, players):
    """构造一个 Frame 的 bytes（与 C++ Frame 严格对齐）。"""
    head = struct.pack("<16f16fii?3x",
                       *(list(w2c) + list(proj) + [int(width), int(height), bool(in_game)]))
    body = PS_FMT.pack(*_ps_items(local))
    # frida 的 allPlayers 往往包含 myPlayer；Frame.local 已经单独保存，
    # 写入共享内存前剔除重复项，避免 C++ 把本地玩家再次当作队友绘制。
    local_pos = _vec3(local.get("pos"))
    filtered_players = []
    for player in players:
        if player.get("isLocal") is True or player.get("isMyPlayer") is True:
            continue
        player_pos = _vec3(player.get("pos"))
        if all(abs(player_pos[i] - local_pos[i]) <= 1e-4 for i in range(3)):
            continue
        filtered_players.append(player)
    players = filtered_players[:N_PLAYERS]
    for p in players:
        body += PS_FMT.pack(*_ps_items(p))
    empty = PS_FMT.pack(*_ps_items({"pos": [0.0, 0.0, 0.0]}))
    for _ in range(N_PLAYERS - len(players)):
        body += empty
    tail = struct.pack("<i", len(players))
    return head + body + tail


def main():
    if sys.platform != "win32":
        # 非 Windows 下 mmap 不支持 tagname；这里仅演示打包逻辑，确认布局与 C++ 一致
        print(f"[shm_writer] 仅 Windows 支持命名共享内存（tagname）。共享区大小 = {SHM_SIZE} 字节")
        f = build_frame([0]*16, [0]*16, 1280, 720, True,
                        {"pos": [0,0,0]}, [{"pos": [1,0,0], "hp": 50}]*12)
        print(f"[shm_writer] 打包 OK，长度 {len(f)}（应等于 {FRAME_FMT.size}）")
        assert len(f) == FRAME_FMT.size, "Frame 布局与 C++ 不一致！"
        print("[shm_writer] 布局校验通过")
        return

    shm = mmap.mmap(-1, SHM_SIZE, tagname=SHM_NAME)
    print(f"[shm_writer] 共享内存 '{SHM_NAME}' 已建立，大小 {SHM_SIZE} 字节；Ctrl+C 退出")
    t = 0.0
    try:
        while True:
            t += 0.016
            players = []
            for i in range(12):
                a = t * (0.6 + 0.05 * i) + i * 0.7
                r = 2.0 + 1.5 * math.sin(t * 0.3 + i)
                players.append({
                    "pos": [math.cos(a) * r, math.sin(a * 0.7) * 1.5, math.sin(a) * r],
                    "team": i % 2, "hp": 30 + int(70 * (0.5 + 0.5 * math.sin(t + i))),
                    "maxHp": 100,
                })
            frame = build_frame([0]*16, [0]*16, 1280, 720, True,
                                {"pos": [0, 0, 6]}, players)
            # 读 cur，写到后台槽，再翻转 cur（与 C++ 双缓冲语义一致）
            cur = struct.unpack_from("<i", shm, 0)[0]
            back = 1 - cur
            shm[4 + back * FRAME_FMT.size: 4 + (back + 1) * FRAME_FMT.size] = frame
            shm[0:4] = struct.pack("<i", back)
            time.sleep(1 / 60.0)
    except KeyboardInterrupt:
        print("\n[shm_writer] 退出")


if __name__ == "__main__":
    main()
