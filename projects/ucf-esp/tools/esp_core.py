#!/usr/bin/env python3
# UCF-ESP 宿主端核心：投影数学 + 帧处理。
#
# 复刻 src/projection.cpp 的列主序约定，并复用 validate_projection.py
# 的 world_to_screen() / combine_pv()。仅标准库，可被 frida_host / replay 直接 import。
#
# 约定（务必与 ucf_projection_lab 保持一致）：
#   - 矩阵为列主序扁平 16：m[col*4 + row]
#   - VP = P * V（combine_pv 内部按列主序相乘）
#   - 屏幕 y 轴向下，故 world_to_screen 对 yc 做了 (1 - ...) 翻转

WIDTH, HEIGHT = 1280, 720

# NDC 裁剪阈值：|ndc| 超过它就不画。
#
# 为什么用 NDC 幅度而不是“到相机的距离”：
#   第三人称游戏里相机就挂在本地玩家头顶（实测 0.95m），透视除法的 w→0.11，
#   屏幕坐标被放大成 (-153, 4121) —— 数学上没错，但画出来毫无意义，必须剔除。
#   然而“距离”不是好判据：世界尺度随游戏而变（合成样本里 0.65 单位完全正常），
#   而 NDC 幅度与尺度无关 —— ndc 超出 ±1 就是出了画面，超 3 倍就是离谱。
NDC_CLIP = 3.0


def clip_of(p, m):
    """返回 (ndc_x, ndc_y, wc)。wc<=0 表示点在相机后方（ndc 为 None）。"""
    wc = p[0] * m[3] + p[1] * m[7] + p[2] * m[11] + m[15]
    if wc <= 1e-3:
        return (None, None, wc)
    xc = p[0] * m[0] + p[1] * m[4] + p[2] * m[8] + m[12]
    yc = p[0] * m[1] + p[1] * m[5] + p[2] * m[9] + m[13]
    return (xc / wc, yc / wc, wc)


def world_to_screen(p, m, w=WIDTH, h=HEIGHT):
    """p=(x,y,z)；m=列主序扁平16；返回 (sx, sy) 或 None（点在相机后方）。

    注意：本函数**不做** NDC 裁剪，始终返回数学结果（哪怕是几千像素的爆炸值），
    裁剪判定在 project_frame 里做 —— 保留原始值便于诊断。
    """
    nx, ny, wc = clip_of(p, m)
    if nx is None:
        return None
    return ((nx * 0.5 + 0.5) * w, (1.0 - (ny * 0.5 + 0.5)) * h)


def combine_pv(P, V):
    """VP = P * V，P/V 均为列主序扁平 16，返回列主序扁平 16。"""
    VP = [0.0] * 16
    for c in range(4):
        for r in range(4):
            s = 0.0
            for k in range(4):
                s += P[k * 4 + r] * V[c * 4 + k]
            VP[c * 4 + r] = s
    return VP


def camera_position(V):
    """从 world→camera 矩阵反推相机世界坐标：cam = -Rᵀ · t。

    V 是列主序扁平 16，R 为其左上 3x3（行主序取 V[r][c] = V[c*4+r]），t 为第 4 列。
    """
    R = [[V[c * 4 + r] for c in range(3)] for r in range(3)]   # R[row][col]
    t = [V[3 * 4 + r] for r in range(3)]                       # t[row] = V[row][3]
    # (Rᵀ·t)[r] = Σ_k R[k][r] * t[k]
    return tuple(-sum(R[k][r] * t[k] for k in range(3)) for r in range(3))


def _dist3(a, b):
    if a is None or b is None:
        return None
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5


def _same_position(a, b, eps=1e-4):
    """判断两个玩家位置是否是同一帧的重复本地玩家记录。"""
    if a is None or b is None:
        return False
    return all(abs(float(a[i]) - float(b[i])) <= eps for i in range(3))


class Mark:
    """一个玩家在屏幕上的投影结果。"""
    __slots__ = ("kind", "screen", "world", "team", "hp", "max_hp",
                 "is_dead", "visible", "on_screen", "name", "dist", "clipped", "bones")

    def __init__(self, kind, screen, world, team, hp, max_hp, is_dead,
                 on_screen, name="", dist=None, clipped=False, bones=None, visible=True):
        self.kind = kind            # 'local' | 'teammate' | 'enemy'
        self.screen = screen        # (sx, sy) 或 None
        self.world = world          # (x, y, z) 或 None
        self.team = team
        self.hp = hp
        self.max_hp = max_hp
        self.is_dead = is_dead
        self.visible = visible
        self.on_screen = on_screen
        self.name = name
        self.dist = dist            # 到相机的距离（米），None 表示未知
        self.clipped = clipped      # True = 被近裁剪剔除（贴脸的点，画出来会爆炸）
        self.bones = bones or []     # [(sx, sy, valid), ...]，与 C++ MAX_BONES 槽位一致

    def as_dict(self):
        return {
            "kind": self.kind,
            "screen": self.screen,
            "world": self.world,
            "team": self.team,
            "hp": self.hp,
            "max_hp": self.max_hp,
            "is_dead": self.is_dead,
            "visible": self.visible,
            "on_screen": self.on_screen,
            "name": self.name,
            "dist": self.dist,
            "clipped": self.clipped,
            "bones": self.bones,
        }


def _vec(p):
    if not p:
        return None
    if isinstance(p, (list, tuple)) and len(p) >= 3:
        return (float(p[0]), float(p[1]), float(p[2]))
    return (float(p["x"]), float(p["y"]), float(p["z"]))


def _make(role, p, local_team, forced_local, w, h, VP, cam_pos, ndc_clip):
    world = _vec(p.get("pos"))
    screen = world_to_screen(world, VP, w, h) if world else None
    dist = _dist3(world, cam_pos)
    # NDC 裁剪：贴着相机的点（第三人称下就是本地玩家自己）w→0，
    # ndc 会飙到十几、屏幕坐标几千像素。数学没错，但不能画，直接剔除。
    clipped = False
    if world is not None:
        nx, ny, _ = clip_of(world, VP)
        clipped = nx is None or abs(nx) > ndc_clip or abs(ny) > ndc_clip
    on_screen = (screen is not None and not clipped
                 and 0.0 <= screen[0] <= w and 0.0 <= screen[1] <= h)
    team = p.get("team")
    if forced_local:
        kind = "local"
    elif local_team is not None and team is not None and team == local_team:
        kind = "teammate"
    else:
        # team 读不到时无法判断阵营，保守按敌人处理（宁可多画一个，别漏画）
        kind = "enemy"
    bones = []
    for b in (p.get("bones", []) or []):
        bp = _vec(b.get("pos")) if b.get("valid", False) else None
        bs = world_to_screen(bp, VP, w, h) if bp else None
        if bp:
            bnx, bny, _ = clip_of(bp, VP)
            bvalid = (bs is not None and bnx is not None and abs(bnx) <= ndc_clip and
                      abs(bny) <= ndc_clip and 0.0 <= bs[0] <= w and 0.0 <= bs[1] <= h)
        else:
            bvalid = False
        bones.append((bs[0], bs[1], True) if bvalid else (0.0, 0.0, False))
    return Mark(kind, screen, world, team,
                p.get("hp"), p.get("maxHp"), bool(p.get("isDead")),
                on_screen, dist=dist, clipped=clipped, bones=bones,
                visible=(p.get("visible") is not False))


def project_frame(frame, w=None, h=None, ndc_clip=NDC_CLIP):
    """把 frida_dump.js 发来的 frame 投影成屏幕坐标 marks。
    返回 (VP, marks)。"""
    w = w or frame.get("width", WIDTH)
    h = h or frame.get("height", HEIGHT)
    w2c = frame["w2c"]
    proj = frame["proj"]
    VP = combine_pv(proj, w2c)
    try:
        cam_pos = camera_position(w2c)
    except Exception:
        cam_pos = None

    local = frame.get("local")
    local_team = local.get("team") if (local and local.get("team") is not None) else None

    marks = []
    if local:
        marks.append(_make("local", local, local_team, True, w, h, VP,
                           cam_pos, ndc_clip))
    for p in frame.get("players", []):
        # allPlayers 通常包含 myPlayer；local 已单独放在 Frame.local，不能再投影一次。
        # 兼容旧帧：优先使用显式标记，缺失标记时用位置去重。
        p_world = _vec(p.get("pos")) if isinstance(p, dict) else None
        if isinstance(p, dict) and (p.get("isLocal") is True or
                                    p.get("isMyPlayer") is True or
                                    _same_position(p_world, _vec(local.get("pos")) if local else None)):
            continue
        marks.append(_make("other", p, local_team, False, w, h, VP,
                           cam_pos, ndc_clip))
    return VP, marks
