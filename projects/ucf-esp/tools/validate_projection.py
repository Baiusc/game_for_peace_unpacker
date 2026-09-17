#!/usr/bin/env python3
# UCF-ESP: 离线验证 world->screen 投影公式，以及和 ucf_projection_lab 一致的
# 列主序矩阵布局。纯标准库，无外部依赖；不带参数直接运行。
#
# 复现 src/projection.cpp 的 world_to_screen() 与黄金样本：
#   identity VP, 1280x720:
#     world=(-0.35,0.55,0) -> (416,162)
#     world=( 0.4,-0.45,0) -> (896,522)
#     world=( 0,   0,  0) -> (640,360)
# 并演示 VP = P * V 的列主序组合（供 Frida 宿主端复用）。

WIDTH, HEIGHT = 1280, 720


def world_to_screen(p, m, w=WIDTH, h=HEIGHT):
    # m 为列主序扁平 16: m[col*4+row]，与 src/projection.cpp 一致
    wc = p[0] * m[3] + p[1] * m[7] + p[2] * m[11] + m[15]
    if wc <= 1e-3:
        return None
    xc = p[0] * m[0] + p[1] * m[4] + p[2] * m[8] + m[12]
    yc = p[0] * m[1] + p[1] * m[5] + p[2] * m[9] + m[13]
    return ((xc / wc * 0.5 + 0.5) * w, (1.0 - (yc / wc * 0.5 + 0.5)) * h)


def combine_pv(P, V):
    # VP = P * V，两者均为列主序扁平 16
    VP = [0.0] * 16
    for c in range(4):
        for r in range(4):
            s = 0.0
            for k in range(4):
                s += P[k * 4 + r] * V[c * 4 + k]
            VP[c * 4 + r] = s
    return VP


def unity_matrix(m00, m01, m02, m03,
                 m10, m11, m12, m13,
                 m20, m21, m22, m23,
                 m30, m31, m32, m33):
    # 按字段名填，返回列主序扁平（m[col*4+row]）
    return [
        m00, m10, m20, m30,
        m01, m11, m21, m31,
        m02, m12, m22, m32,
        m03, m13, m23, m33,
    ]


def approx(a, b, tol=0.5):
    return a is not None and b is not None and abs(a - b) <= tol


def main():
    # 1) 黄金样本：identity VP
    I = unity_matrix(1, 0, 0, 0,
                     0, 1, 0, 0,
                     0, 0, 1, 0,
                     0, 0, 0, 1)
    fixtures = [(-0.35, 0.55, 0.0), (0.4, -0.45, 0.0), (0.0, 0.0, 0.0)]
    expected = [(416, 162), (896, 522), (640, 360)]
    print("== 黄金样本 (identity VP) ==")
    for f, e in zip(fixtures, expected):
        s = world_to_screen(f, I)
        ok = approx(s[0], e[0]) and approx(s[1], e[1])
        print(f"  world={f} -> screen=({s[0]:.1f},{s[1]:.1f})  expect={e}  {'OK' if ok else 'FAIL'}")
        assert ok, f"golden sample mismatch: {f} -> {s}"

    # 2) 组合正确性：P=I 时 VP==V
    Vt = unity_matrix(1, 0, 0, 2,     # 平移 (+2,0,0)
                      0, 1, 0, 0,
                      0, 0, 1, 0,
                      0, 0, 0, 1)
    VP = combine_pv(I, Vt)
    assert VP == Vt, "combine(I,V) should equal V"
    # 原点经 +2 平移（裁剪空间 x=2）后投影到右外侧 x=(2*0.5+0.5)*1280=1920
    s = world_to_screen((0.0, 0.0, 0.0), VP)
    print(f"\n== 组合 VP=P*V (P=I, V=平移+2) ==")
    print(f"  world=(0,0,0) -> screen=({s[0]:.1f},{s[1]:.1f})  expect x=1920  {'OK' if approx(s[0],1920,1) else 'FAIL'}")
    assert approx(s[0], 1920, 1) and approx(s[1], 360, 1)

    # 3) 组合结合律自检：combine(combine(A,B),C) == combine(A, combine(B,C))
    A = unity_matrix(2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1)
    B = unity_matrix(1,0,0,1, 0,1,0,0, 0,0,1,0, 0,0,0,1)
    C = unity_matrix(1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,0,1)
    l = combine_pv(combine_pv(A, B), C)
    r = combine_pv(A, combine_pv(B, C))
    assert l == r, "matrix combine not associative"
    print("\n== 组合结合律 ==")
    print("  combine(combine(A,B),C) == combine(A,combine(B,C))  OK")

    print("\nucf validate_projection: ALL PASS")


if __name__ == "__main__":
    main()
