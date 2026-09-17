#!/usr/bin/env python3
"""用真实对局帧校准/锁死投影链路（离线，不需要 frida / 游戏）。

背景：曾经以为“投影公式错了”，因为本地玩家被投影成 screen=(-153, 4121)
（屏幕才 800x600）。实测手算证明公式**完全正确** —— 真因是第三人称相机就挂在
玩家头顶约 0.95m，透视除法的 w→0.11，坐标被放大几千像素。数学没错但画出来
毫无意义，所以加了近裁剪（NEAR_CLIP_M）。这个文件就是把这个结论钉死，
以后谁再动 combine_pv / world_to_screen / camera_position 都会立刻被发现。

运行： python tests/test_projection_calibration.py
"""

import json
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))

import esp_core  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, "fixtures", "real_frame_level3.json")
# 相机转向后的另一帧：实测日志里有 screen=(797,215) onscreen=True
FIXTURE_T3 = os.path.join(HERE, "fixtures", "real_frame_level3_tick3.json")

FAILURES = []


def expect(cond, msg):
    if cond:
        print(f"  PASS {msg}")
    else:
        FAILURES.append(msg)
        print(f"  FAIL {msg}")


def close(a, b, tol=1e-3):
    return a is not None and b is not None and abs(a - b) <= tol


# ---------------------------------------------------------------- 独立实现
def to_rows(m):
    """列主序扁平 16 -> 行主序 M[row][col]"""
    return [[m[c * 4 + r] for c in range(4)] for r in range(4)]


def matmul(A, B):
    return [[sum(A[r][k] * B[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def apply(M, p):
    return [sum(M[r][c] * p[c] for c in range(4)) for r in range(4)]


def load(path=FIXTURE):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ---------------------------------------------------------------- 用例
def test_vp_matches_independent_p_times_v():
    """VP 必须是 P*V（不是 V*P），且列主序乘法索引正确。"""
    fr = load()
    VP_flat = esp_core.combine_pv(fr["proj"], fr["w2c"])
    P = to_rows(fr["proj"])
    V = to_rows(fr["w2c"])
    ref = matmul(P, V)                       # 独立实现的 P*V
    worst = 0.0
    for r in range(4):
        for c in range(4):
            worst = max(worst, abs(VP_flat[c * 4 + r] - ref[r][c]))
    expect(worst < 1e-5, f"combine_pv == 独立 P*V（最大偏差 {worst:.2e}）")

    # 反向确认：V*P 必须与之不同，否则说明测试没有区分力
    wrong = matmul(V, P)
    diff = max(abs(VP_flat[c * 4 + r] - wrong[r][c]) for r in range(4) for c in range(4))
    expect(diff > 1e-2, f"V*P 与 P*V 确实不同（差 {diff:.3f}），本测试有区分力")


def test_camera_position():
    """从 w2c 反推相机世界坐标，应与手算 -Rᵀ·t 一致。"""
    fr = load()
    cam = esp_core.camera_position(fr["w2c"])
    V = to_rows(fr["w2c"])
    R = [[V[r][c] for c in range(3)] for r in range(3)]
    t = [V[r][3] for r in range(3)]
    ref = [-sum(R[k][r] * t[k] for k in range(3)) for r in range(3)]
    expect(all(close(cam[i], ref[i], 1e-6) for i in range(3)),
           f"camera_position ≈ ({cam[0]:.3f}, {cam[1]:.3f}, {cam[2]:.3f}) 与手算一致")
    # 实测值（2026-09-17）：相机就在本地玩家头顶不到 1 米 —— 第三人称相机
    expect(close(cam[0], 10.688, 0.01) and close(cam[1], 14.170, 0.01)
           and close(cam[2], -2.066, 0.01),
           "相机位置复现实测值 (10.688, 14.170, -2.066)")


def test_local_player_is_near_clipped():
    """本地玩家离相机 ~0.95m，投影会爆炸 —— 数学没错，但必须被近裁剪剔除。"""
    fr = load()
    VP, marks = esp_core.project_frame(fr)
    local = [m for m in marks if m.kind == "local"][0]
    expect(local.dist is not None and 0.8 < local.dist < 1.2,
           f"本地玩家到相机距离 {local.dist:.3f} m（预期 ~0.95m，第三人称相机）")
    expect(local.clipped is True, "本地玩家被近裁剪标记（clipped=True）")
    expect(local.on_screen is False, "本地玩家 on_screen=False（不会被画成几千像素的怪框）")
    # 关键：screen 坐标本身依然是那组“爆炸值”，证明我们没有偷偷改公式
    expect(local.screen is not None and abs(local.screen[0] + 152.6) < 2.0
           and abs(local.screen[1] - 4121.1) < 5.0,
           f"投影数学未被篡改：screen=({local.screen[0]:.1f}, {local.screen[1]:.1f}) 仍是实测值")


def test_world_to_screen_matches_manual():
    """world_to_screen 与纯手算逐点对照（含敌方）。"""
    fr = load()
    W, H = fr["width"], fr["height"]
    VP, marks = esp_core.project_frame(fr)
    P = to_rows(fr["proj"])
    V = to_rows(fr["w2c"])
    VP_ref = matmul(P, V)

    checked = 0
    for m in marks:
        if m.world is None:
            continue
        clip = apply(VP_ref, list(m.world) + [1.0])
        if clip[3] <= 1e-3:
            expect(m.screen is None, f"{m.kind} 在相机后方 -> screen=None")
            continue
        sx = (clip[0] / clip[3] * 0.5 + 0.5) * W
        sy = (1.0 - (clip[1] / clip[3] * 0.5 + 0.5)) * H
        if close(m.screen[0], sx, 1e-6) and close(m.screen[1], sy, 1e-6):
            checked += 1
        else:
            expect(False, f"{m.kind} 屏幕坐标不符：{m.screen} vs ({sx:.3f}, {sy:.3f})")
    expect(checked >= 5, f"{checked} 个点的屏幕坐标与手算一致")


def test_enemies_project_into_screen():
    """tick=3 帧里有目标真的落进屏幕，且坐标复现实测日志的 (797,215)。"""
    fr = load(FIXTURE_T3)
    W, H = fr["width"], fr["height"]
    _, marks = esp_core.project_frame(fr)
    on = [m for m in marks if m.on_screen]
    expect(len(on) >= 1, f"{len(on)} 个目标落在屏幕内")
    for m in on:
        expect(0.0 <= m.screen[0] <= W and 0.0 <= m.screen[1] <= H,
               f"{m.kind} screen=({m.screen[0]:.0f}, {m.screen[1]:.0f}) 在 {W}x{H} 内")
    if on:
        m = on[0]
        expect(abs(m.screen[0] - 797) < 3 and abs(m.screen[1] - 215) < 3,
               f"复现实测日志值 (797,215) -> 实算 ({m.screen[0]:.1f}, {m.screen[1]:.1f})")


def test_no_explosive_coord_is_drawable():
    """近裁剪的意义：任何被判“可画”的点都不能是几千像素的爆炸值。"""
    for path in (FIXTURE, FIXTURE_T3):
        fr = load(path)
        W, H = fr["width"], fr["height"]
        _, marks = esp_core.project_frame(fr)
        for m in marks:
            if not m.on_screen or m.screen is None:
                continue
            ok = (-W <= m.screen[0] <= 2 * W) and (-H <= m.screen[1] <= 2 * H)
            expect(ok, f"tick={fr['tick']} {m.kind} 可画坐标量级正常 "
                       f"({m.screen[0]:.0f}, {m.screen[1]:.0f})")
        # 被裁剪的点绝不能同时被判可画
        expect(not any(m.clipped and m.on_screen for m in marks),
               f"tick={fr['tick']} 被近裁剪的点不会被判可画")


def test_team_comparison_splits_mates_from_enemies():
    """team 为 int 时队友/敌人才分得开（bridge 返回对象时全员会被判 enemy）。"""
    fr = load()
    _, marks = esp_core.project_frame(fr)
    mates = [m for m in marks if m.kind == "teammate"]
    enemies = [m for m in marks if m.kind == "enemy"]
    expect(len(mates) >= 2, f"识别到 {len(mates)} 个队友（team==local.team）")
    expect(len(enemies) >= 4, f"识别到 {len(enemies)} 个敌人")

    # 反向验证旧 bug：team 若还是 bridge 返回的 dict，两个对象永不相等 -> 全 enemy
    broken = json.loads(json.dumps(fr))
    for p in [broken["local"]] + broken["players"]:
        p["team"] = {"handle": "0x%x" % id(p), "type": {"handle": "0x798416fc"}}
    _, bad = esp_core.project_frame(broken)
    expect(sum(1 for m in bad if m.kind == "teammate") == 0,
           "team 为对象时确实分不出队友（复现旧 bug，故必须读 int）")


def test_ndc_clip_is_configurable():
    """NDC 裁剪阈值可调：放大到极大时本地玩家不再被裁（用于按需验证）。"""
    fr = load()
    _, marks = esp_core.project_frame(fr, ndc_clip=1e9)
    local = [m for m in marks if m.kind == "local"][0]
    expect(local.clipped is False, "ndc_clip 放大后不再裁剪（阈值可调）")


def test_synthetic_sample_is_not_over_clipped():
    """合成样本尺度与真实游戏不同，绝不能被距离类阈值误裁（回归）。"""
    path = os.path.join(HERE, "..", "tools", "sample_session.json")
    if not os.path.isfile(path):
        print("  SKIP 没有 sample_session.json")
        return
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    frames = data if isinstance(data, list) else data.get("frames", [data])
    shown = 0
    for fr in frames:
        _, marks = esp_core.project_frame(fr)
        shown += sum(1 for m in marks if m.on_screen)
    expect(shown >= 3, f"合成样本仍有 {shown} 个可见目标（距离尺度不同不应被误裁）")


def main():
    print("投影校准测试：真实对局帧（离线）")
    for fn in (test_vp_matches_independent_p_times_v,
               test_camera_position,
               test_local_player_is_near_clipped,
               test_world_to_screen_matches_manual,
               test_enemies_project_into_screen,
               test_no_explosive_coord_is_drawable,
               test_team_comparison_splits_mates_from_enemies,
               test_ndc_clip_is_configurable,
               test_synthetic_sample_is_not_over_clipped):
        fn()
    if FAILURES:
        print(f"\n{len(FAILURES)} 项失败：")
        for m in FAILURES:
            print("  -", m)
        return 1
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
