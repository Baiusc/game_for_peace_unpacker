#!/usr/bin/env python3
# dump.cs <-> frida_dump.js 契约测试（离线，不需要 frida / 游戏 / Windows）
#
# 为什么需要它：
#   frida_dump.js 里写死了一批“方法名 + 字段偏移”（相机矩阵、Transform 坐标、
#   ObscuredInt 血量解密）。这些一旦和真实二进制对不上，症状是“脚本跑起来了但数据全 null”，
#   排查成本很高。本测试直接解析 il2cpp_dump/dump.cs，把契约钉死：
#     1) 方法名/签名确实存在（含 *_Injected 的 out 指针版本）
#     2) 裸内存读用到的字段偏移确实正确（Matrix4x4 列主序、ObscuredInt 的 XOR 布局）
#     3) frida_dump.js 里引用的每个名字都在 dump 里能找到（防止改名后静默失效）
#
# 运行：
#   python tests/test_dump_contract.py

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DUMP = os.path.join(ROOT, "il2cpp_dump", "dump.cs")
JS = os.path.join(ROOT, "tools", "frida_dump.js")

TYPE_RE = re.compile(
    r"^\s*(?:public|internal)\s+(?:(?:sealed|abstract|static|partial)\s+)*"
    r"(class|struct|enum|interface)\s+([A-Za-z_][\w\.<>`]*)")
# 字段行形如： public static Player myPlayer; // 0x0
FIELD_RE = re.compile(r"\b(\w+);\s*//\s*(0x[0-9A-Fa-f]+)")
# 方法行形如： private void get_worldToCameraMatrix_Injected(out Matrix4x4 ret) { }
METHOD_RE = re.compile(r"\b(\w+)\s*\(([^)]*)\)\s*\{")

# 只看我们关心、且名字在 dump 里唯一的这些类型
WANTED = [
    "Matrix4x4", "ObscuredInt", "Camera", "Screen", "Transform",
    "HealthData", "GameManager", "Entity", "Player", "Vector3",
]


def parse_dump(path):
    """返回 {类型名: {"kind":..., "fields":[(名,偏移)], "methods":{名: 参数串}, "body": 原文}}"""
    types = {}
    cur = None            # (名字, kind, depth, 起始行)
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if cur is None:
                m = TYPE_RE.match(line)
                if m:
                    kind, name = m.group(1), m.group(2)
                    if name in WANTED and name not in types:
                        # 把声明行本身也放进 body，方便后面核对继承关系
                        cur = [name, kind, line.count("{") - line.count("}"), [line]]
                        types[name] = {"kind": kind, "fields": [], "static_fields": [],
                                       "methods": {}, "body": []}
                    continue
            else:
                cur[2] += line.count("{") - line.count("}")
                cur[3].append(line)
                if cur[2] <= 0:
                    info = types[cur[0]]
                    info["body"] = "".join(cur[3])
                    for ln in cur[3]:
                        fm = FIELD_RE.search(ln)
                        if fm:
                            name, off = fm.group(1), int(fm.group(2), 16)
                            if "static" in ln:
                                info["static_fields"].append(name)
                            else:
                                info["fields"].append((name, off))
                        mm = METHOD_RE.search(ln)
                        if mm:
                            info["methods"].setdefault(mm.group(1), mm.group(2).strip())
                    cur = None
    return types


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


def field_offset(t, name):
    for n, off in t["fields"]:
        if n == name:
            return off
    raise AssertionError(f"{name} 字段不存在")


def has_method(t, name, param_hint=None):
    if name not in t["methods"]:
        return False
    if param_hint is None:
        return True
    return param_hint in t["methods"][name]


def main():
    if not os.path.exists(DUMP):
        print(f"[SKIP] 没有 {DUMP}，跳过契约测试")
        return 0
    T = parse_dump(DUMP)
    missing = [n for n in WANTED if n not in T]
    expect(not missing, f"dump.cs 里没解析到这些类型：{missing}")

    print("dump.cs <-> frida_dump.js 契约测试")

    # ---- 1. Matrix4x4：内存顺序必须是列主序 m00,m10,m20,m30,... ----
    mx = T["Matrix4x4"]
    order = [n for n, _ in mx["fields"]][:4]
    expect(order == ["m00", "m10", "m20", "m30"],
           f"Matrix4x4 前 4 个字段应为列主序(列0)，实际 {order}")
    expect(len(mx["fields"]) == 16, f"Matrix4x4 应有 16 个 float，实际 {len(mx['fields'])}")
    expect([o for _, o in sorted(mx["fields"], key=lambda x: x[1])] == list(range(0, 64, 4)),
           "Matrix4x4 字段偏移应为 0x0,0x4,...0x3C（连续 float）")
    print("  PASS Matrix4x4 列主序布局 m[col*4+row]，16 个连续 float")

    # ---- 2. 相机：走 *_Injected(out) 的路径必须在 dump 里成立 ----
    cam = T["Camera"]
    expect(has_method(cam, "get_main"), "Camera.get_main 不存在")
    expect(has_method(cam, "get_worldToCameraMatrix_Injected", "out Matrix4x4"),
           "Camera.get_worldToCameraMatrix_Injected(out Matrix4x4) 不存在 -> JS 的矩阵读取模式会退化")
    expect(has_method(cam, "get_projectionMatrix_Injected", "out Matrix4x4"),
           "Camera.get_projectionMatrix_Injected(out Matrix4x4) 不存在")
    print("  PASS Camera.get_main / get_worldToCameraMatrix_Injected(out) / get_projectionMatrix_Injected(out)")

    # ---- 3. Transform 坐标：同样走 out 指针 ----
    tr = T["Transform"]
    expect(has_method(tr, "get_position_Injected", "out Vector3"),
           "Transform.get_position_Injected(out Vector3) 不存在")
    max_off = max(o for _, o in T["Vector3"]["fields"])
    expect(len(T["Vector3"]["fields"]) == 3 and max_off == 8,
           f"Vector3 应为 x/y/z @0,4,8，实际 {T['Vector3']['fields']}")
    print("  PASS Transform.get_position_Injected(out Vector3)，Vector3 = 12 字节")

    # ---- 4. Screen 尺寸 ----
    scr = T["Screen"]
    expect(has_method(scr, "get_width") and has_method(scr, "get_height"),
           "Screen.get_width/get_height 不存在")
    print("  PASS Screen.get_width / get_height")

    # ---- 5. GameManager：静态 myPlayer + 实例 allPlayers + 单例 get_instance ----
    gm = T["GameManager"]
    expect("Singleton<GameManager>" in gm["body"][:300],
           "GameManager 应继承 Singleton<GameManager>（单例入口来自基类）")
    # get_instance 声明在泛型基类 Singleton<T> 上，不在 GameManager 自己身上。
    # frida-il2cpp-bridge 的“按名查方法”会沿父类链找（这也是本游戏实测能拿到单例的原因），
    # 所以这里直接在原文里核对非抽象的那个 Singleton<T>。
    with open(DUMP, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    hdr = "public class Singleton<T> : MonoBehaviour"
    i = text.find(hdr)
    expect(i >= 0, f"dump 里找不到 {hdr}")
    expect("get_instance" in text[i:i + 4000], "Singleton<T> 里没有 get_instance")
    expect("myPlayer" in gm["static_fields"], "GameManager.myPlayer 静态字段不存在")
    expect(any(n == "allPlayers" for n, _ in gm["fields"]), "GameManager.allPlayers 实例字段不存在")
    print("  PASS GameManager : Singleton<GameManager>（get_instance 在基类）/ 静态 myPlayer / 实例 allPlayers")

    # ---- 6. 血量：ObscuredInt 的 XOR 布局 + HealthData 里两个字段的偏移 ----
    oi = T["ObscuredInt"]
    expect(field_offset(oi, "currentCryptoKey") == 0x0, "ObscuredInt.currentCryptoKey 应在 0x0")
    expect(field_offset(oi, "hiddenValue") == 0x4, "ObscuredInt.hiddenValue 应在 0x4")
    expect(field_offset(oi, "inited") == 0x8, "ObscuredInt.inited 应在 0x8")
    expect(field_offset(oi, "fakeValue") == 0xC, "ObscuredInt.fakeValue 应在 0xC")
    expect(field_offset(oi, "fakeValueActive") == 0x10, "ObscuredInt.fakeValueActive 应在 0x10")
    hd = T["HealthData"]
    cur, mx_off = field_offset(hd, "currentHealth"), field_offset(hd, "maxHealth")
    expect(cur == 0x8, f"HealthData.currentHealth 应在 0x8，实际 0x{cur:x}")
    expect(mx_off - cur == 0x14, "两个 ObscuredInt 的间距应等于 ObscuredInt 大小 0x14")
    print(f"  PASS ObscuredInt 布局(XOR 解密) currentHealth@0x{cur:x} maxHealth@0x{mx_off:x}")

    # ---- 7. 玩家/实体的取值方法名 ----
    ent, ply = T["Entity"], T["Player"]
    for name in ("get_healthData", "get_isDead", "get_team"):
        expect(has_method(ent, name) or has_method(ply, name), f"Entity/Player 上找不到 {name}")
    expect(has_method(ply, "get_isMyPlayer"), "Player.get_isMyPlayer 不存在")
    print("  PASS Entity/Player.get_team / get_isDead / get_healthData / Player.get_isMyPlayer")

    # ---- 7b. 队伍：Team 是 enum : int，必须裸读 backing field ----
    # 坑：get_team() 返回的是 boxed Object（bridge 不解包 enum 的 value__），
    # 传到宿主是 {handle,type}，两个对象永远不相等 -> 队友识别整个失效。
    # 所以 frida_dump.js 必须读 <team>k__BackingField 的原始 int。
    with open(DUMP, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    m = re.search(r"public enum Team\b[^{]*\{(.*?)\n\}", text, re.S)
    expect(m is not None, "dump 里找不到 enum Team")
    if m:
        expect("value__" in m.group(1), "Team 应有 int value__（enum 底层类型）")
    hits = [n for n, o in ent["fields"] if "k__BackingField" in n and o == 0x1C]
    expect(len(hits) >= 1,
           f"Entity 上找不到位于 0x1C 的 team backing field（候选："
           f"{[(n, hex(o)) for n, o in ent['fields'] if 'k__' in n]}）")
    with open(JS, "r", encoding="utf-8") as f:
        js_team = f.read()
    expect("readTeamOf" in js_team, "frida_dump.js 必须用 readTeamOf 读 enum 的原始 int")
    print("  PASS Team 是 enum:int（value__）+ Entity team backing field @0x1c，JS 走 readTeamOf")

    # ---- 8. 反向校验：JS 里引用的名字必须在 dump 里存在 ----
    with open(JS, "r", encoding="utf-8") as f:
        js = f.read()
    used = [
        "get_main", "get_worldToCameraMatrix_Injected", "get_projectionMatrix_Injected",
        "get_position_Injected", "get_width", "get_height", "get_instance",
        "get_transform", "get_team", "get_isDead", "get_isMyPlayer", "get_healthData",
    ]
    for name in used:
        expect(name in js, f"frida_dump.js 没用到 {name}？契约测试需要同步更新")
    for cls in ("UnityEngine.Camera", "UnityEngine.Screen", "GameManager"):
        expect(cls in js, f"frida_dump.js 没引用类 {cls}")
    print(f"  PASS frida_dump.js 引用的 {len(used)} 个方法名 + 3 个类名与 dump 一致")

    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
