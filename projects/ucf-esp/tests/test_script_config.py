#!/usr/bin/env python3
"""宿主注入配置（UCFG）、frida_dump.js 档位逻辑、以及消息 payload 解析的离线校验。

不需要 frida / 游戏，只验证：
  1) build_script_source 拼出来的 UCFG 是合法 JSON，且能被 JS 解析；
  2) frida_dump.js / bundle 里确实实现了 0~4 五个档位（否则 --level 是空开关）；
  3) 默认（无 UCFG）走 level=4 全量，不会误降级；
  4) on_message 能同时吃下 dict（frida 16+ 直接解对象）和 JSON 字符串（老通道）；
  5) 单步探测（--probe）的 16 个步骤在源码与 bundle 中都存在；
  6) 取单例不再默认调用泛型基类上的 get_instance()（level3/4 硬崩溃头号嫌疑）；
  7) 数组遍历用的是 bridge 的 arr.get(i)，不是恒为 undefined 的 arr[i]；
  8) 不再默认调 get_isDead()（level3 实测崩点），死亡状态由血量推导；
  9) 取帧默认调度到 Unity 主线程（frida 线程上调用有真实方法体的托管方法会 AV）。
"""

import os
import re
import sys
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "tools")
sys.path.insert(0, TOOLS_DIR)

import frida_host  # noqa: E402

SRC = os.path.join(TOOLS_DIR, "frida_dump.js")
BUNDLE = os.path.join(TOOLS_DIR, "frida_dump.bundle.js")


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


def test_ucfg_shape():
    js = frida_host.build_script_source("/*body*/", level=2, interval_ms=1000, discover=True)
    first = js.splitlines()[0]
    expect(first.startswith("const UCFG = {") and first.endswith("};"),
           f"UCFG 行格式不对: {first!r}")
    import json
    cfg = json.loads(first[len("const UCFG = "):-1])
    expect(cfg["level"] == 2 and cfg["interval"] == 1000 and cfg["discover"] is True,
           f"UCFG 内容不对: {cfg}")
    expect(cfg["probe"] is False, f"未传 probe 时应为 False: {cfg}")
    expect(cfg["allowGetInstance"] is False, f"未传 allow_get_instance 时应为 False: {cfg}")
    expect(cfg["allowIsDead"] is False, f"未传 allow_is_dead 时应为 False: {cfg}")
    expect(cfg["mainThread"] is True, f"默认应调度到 Unity 主线程: {cfg}")
    expect(js.endswith("/*body*/"), "UCFG 必须拼在脚本正文前面")
    print("  PASS UCFG 注入格式（合法 JSON + 位于脚本最前 + 新开关默认值正确）")


def test_probe_and_flags():
    js = frida_host.build_script_source("/*body*/", probe=True, allow_get_instance=True,
                                        allow_is_dead=True, main_thread=False)
    cfg = json.loads(js.splitlines()[0][len("const UCFG = "):-1])
    expect(cfg["probe"] is True and cfg["allowGetInstance"] is True,
           f"--probe / --allow-get-instance 没进 UCFG: {cfg}")
    expect(cfg["allowIsDead"] is True and cfg["mainThread"] is False,
           f"--allow-is-dead / --no-main-thread 没进 UCFG: {cfg}")
    # camelCase 必须和 frida_dump.js 里读的键一致
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    expect('CFG.probe === true' in src, "JS 读的键应叫 probe")
    expect('CFG.allowGetInstance === true' in src, "JS 读的键应叫 allowGetInstance")
    expect('CFG.allowIsDead === true' in src, "JS 读的键应叫 allowIsDead")
    expect('CFG.mainThread !== false' in src, "JS 读的键应叫 mainThread 且默认开启")
    print("  PASS --probe / --allow-get-instance / --allow-is-dead / --no-main-thread 透传且键名一致")


def test_probe_steps_synced():
    """单步探测的锚点必须在源码和 bundle 里同时存在（防“改了源码忘了重打包”）。"""
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    with open(BUNDLE, encoding="utf-8") as f:
        bundle = f.read()
    anchors = [
        "单步探测开始",            # runner
        "runProbe(startFrameLoop)",  # 探测完才启动取帧
        "buildProbeSteps",
        "gmFromStaticField",
        "gmFromHeapScan",
        "gmFromGetInstance",
        "Il2Cpp.gc.choose",
        "allPlayers.get(0)",
        "get_position_Injected",
        "get_healthData",
        "【高危·默认跳过】",   # get_isDead（level3 实测崩点，默认不测）
        "【最后·高危】",
    ]
    for a in anchors:
        expect(a in src, f"frida_dump.js 缺少探测锚点 {a!r}")
        expect(a in bundle, f"frida_dump.bundle.js 缺少探测锚点 {a!r}（忘了重打包？）")
    # get_instance 必须是探测的**最后**一步，否则它一崩就看不到前面的结论；
    # 同理，实测崩点 get_isDead 也必须排在倒数第二，不能挡住后面的步骤。
    # （只看 buildProbeSteps 内部的顺序：这些名字在上面还有函数定义）
    seg = src[src.index("const buildProbeSteps"):]
    i_last = seg.index("【最后·高危】")
    expect(seg.index("gmFromHeapScan") < i_last, "get_instance 必须排在堆扫描之后")
    expect(seg.index("get_position_Injected") < i_last, "get_instance 必须排在坐标读取之后")
    i_dead = seg.index("【高危·默认跳过】")
    expect(i_dead < i_last, "get_isDead 必须排在 get_instance 之前")
    expect(seg.index("get_healthData") < i_dead, "get_isDead 必须排在血量读取之后")
    print(f"  PASS 单步探测 {len(anchors)} 个锚点在源码与 bundle 中同步，"
          "get_isDead 倒数第二、get_instance 最后")


def test_isdead_not_called_by_default():
    """level3 实测崩点：Entity.get_isDead()。

    dump.cs 223626 里它没有 [CompilerGeneratedAttribute]（= 有真实方法体），
    而同类的 get_team() 有该特性（字段直读）就没崩。默认必须由血量推导。
    """
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    expect("ALLOW_IS_DEAD" in src, "缺少 ALLOW_IS_DEAD 开关")
    # 生产路径（readPlayer）里：只有开关打开才 invoke
    seg = src[src.index("const readPlayer"):]
    seg = seg[:seg.index("\n  };")]
    expect("if (ALLOW_IS_DEAD)" in seg, "readPlayer 默认不能调 get_isDead()")
    expect("out.hp <= 0" in seg, "isDead 应由血量推导（hp<=0）")
    print("  PASS isDead 默认由 hp<=0 推导，不调 get_isDead()")


def test_main_thread_scheduling():
    """frida 的 setInterval 跑在自己线程上，不在 Unity 主线程。

    Entity.get_isDead() / Player.get_isMyPlayer() 都是有真实方法体的 getter，
    在 frida 线程上调会 AV。默认必须走 Il2Cpp.mainThread.schedule。
    """
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    expect("Il2Cpp.mainThread" in src, "缺少主线程调度")
    expect("t.schedule(fn)" in src, "必须用 Thread.schedule 而不是直接调")
    expect("callOnMainThread(frameTick)" in src, "取帧必须走主线程调度")
    expect("MT.busy" in src, "主线程 Post 是异步的，需要 busy 防重入")
    print("  PASS 取帧调度到 Unity 主线程，且带 busy 防重入")


def test_singleton_avoids_generic_call():
    """level3/4 硬崩溃的头号嫌疑：泛型基类 Singleton<T> 上的静态泛型方法。

    dump.cs 里 get_instance 是 RVA:-1 的共享泛型方法（还带隐藏 MethodInfo 参数），
    按普通签名调用会 AV。默认路径必须只走“静态 backing field / 堆扫描”。
    """
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    expect("ALLOW_GET_INSTANCE ? gmFromGetInstance() : null" in src,
           "getGameManager 默认不能调用 get_instance()")
    expect("GM_INSTANCE_FIELD" in src and "<instance>k__BackingField" in src,
           "应有编译生成的静态 backing field 兜底")
    expect("MAX_PLAYERS" in src, "数组长度应有上限兜底，防止读到垃圾值就一路越界")
    # 遍历必须走 bridge 的 arr.get(i)：Il2Cpp.Array 没有下标代理，arr[i] 恒为 undefined
    expect("useGet ? allPlayers.get(i)" in src, "数组遍历必须用 allPlayers.get(i)")
    print("  PASS 取单例避开泛型 get_instance，数组走 .get(i) 且有长度上限")


def test_defaults():
    js = frida_host.build_script_source("/*body*/")
    import json
    cfg = json.loads(js.splitlines()[0][len("const UCFG = "):-1])
    expect(cfg["level"] == frida_host.DEFAULT_LEVEL == 4, f"默认档位应为 4: {cfg}")
    print(f"  PASS 默认档位 level={cfg['level']}（全量）")
    # 采样间隔：实测 8ms 时单帧真耗时上百毫秒 -> 主线程被占满、游戏掉到个位数 FPS。
    # 默认必须回到“跑得完”的量级，否则又会重现“注入后卡顿”。
    expect(cfg["interval"] == 33, f"默认采样间隔应为 33ms（≈30Hz），实际 {cfg['interval']}")
    expect(cfg["bones"] is False, f"骨骼默认应关闭，需 --bones 显式开启: {cfg}")
    expect(cfg["bonesRefreshMs"] >= 100, f"骨骼应节流（默认 250ms），实际 {cfg['bonesRefreshMs']}")
    expect(cfg["visRefreshMs"] >= 100, f"遮挡应节流（默认 150ms），实际 {cfg['visRefreshMs']}")
    expect(cfg["visibility"] is True, f"遮挡检测默认开启且可用 --no-visibility 关闭: {cfg}")
    print(f"  PASS 性能默认值：interval={cfg['interval']}ms 骨骼默认关闭、刷新={cfg['bonesRefreshMs']}ms "
          f"遮挡={cfg['visRefreshMs']}ms")


def test_perf_guards():
    """主线程取帧的“省开支”三件套必须在源码与 bundle 里都在。

    背景：bridge 的 obj.tryMethod() 每次都会 Memory.allocUtf8String(名字) + native
    方法查找 + new Proxy()。取帧是「每帧 × 每玩家 × 每字段」上千次调用，于是单帧占用
    Unity 主线程 ~190ms（游戏实际只有 5 FPS）。三处对策：
      ① callMethod 走 (类|对象|名字|参数个数) 缓存，稳态下只剩 Map.get + invoke；
      ② 骨骼按墙钟节流 + 按玩家缓存（19 次 GetBoneTransform + 兜底 Transform.Find 很贵）；
      ③ 帧内分段计时，便于下次直接量出“卡在哪一段”。
    """
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    with open(BUNDLE, encoding="utf-8") as f:
        bundle = f.read()
    for token in ("BOUND_CACHE", "resolveInstanceMethod", "METHOD_CACHE",
                  "bonesDoRefreshThisFrame", "BONE_REFRESH_MS", "perfReport"):
        expect(token in src, f"frida_dump.js 缺少性能保护锚点 {token!r}")
        expect(token in bundle,
               f"frida_dump.bundle.js 缺少 {token!r}（改了源码忘了重打包？）")
    expect("findDeclaringClass" not in src,
           "旧的“每次遍历类层级找方法”实现应已被缓存版取代")
    expect("CLASS_OF" not in src,
           "禁止「对象地址 -> 类」缓存：Unity 回收对象后地址复用会把新对象当成旧类")
    # 骨骼不能每帧无条件读
    expect("readBonesOf" in src, "缺少骨骼读取")
    expect(src.count("readBonesOf(p)") == 1,
           "readBonesOf 只应在节流分支里被调用一次（每帧都调就又回去了）")
    expect("const BONES = CFG.bones === true" in src,
           "骨骼必须显式开启，避免无效模型的失败扫描进入默认取帧路径")
    expect("hit.add(0x1c).readFloat()" in src,
           "RaycastHit.m_Distance 必须读取 0x1c")
    expect("hit, -5, 0" in src,
           "Linecast 应使用 Unity 默认 Raycast 层掩码 -5")
    expect("hit.add(0x18).readFloat()" not in src,
           "0x18 是 RaycastHit.m_FaceID，不能当作距离")
    print("  PASS 方法缓存 / 骨骼节流 / 分段计时锚点齐全，且骨骼不再每帧无条件读")


def test_levels_implemented():
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    with open(BUNDLE, encoding="utf-8") as f:
        bundle = f.read()
    # 档位分支的锚点：源码与打包产物里都必须存在，否则改完忘了重打包
    anchors = {
        0: "level0 心跳",
        1: "level1 心跳",
        2: "level2 已发送",
        3: "LEVEL >= 4",
        4: "readObscuredInt",
    }
    for lvl, token in anchors.items():
        expect(token in src, f"frida_dump.js 缺少 level{lvl} 的锚点 {token!r}")
        expect(token in bundle,
               f"frida_dump.bundle.js 缺少 level{lvl} 的锚点 {token!r}（改了源码忘了重打包？）")
    expect("LEVEL <= 0" in src, "缺少 LEVEL<=0 的早退分支")
    expect("LEVEL <= 1" in src, "缺少 LEVEL<=1 的早退分支")
    expect("LEVEL <= 2" in src, "缺少 LEVEL<=2 的早退分支")
    print("  PASS 五个档位在源码与 bundle 中都已实现")


def test_fallback_without_ucfg():
    with open(SRC, encoding="utf-8") as f:
        src = f.read()
    m = re.search(r"const LEVEL\s*=\s*Number\.isFinite\(CFG\.level\)\s*\?\s*CFG\.level\s*:\s*(\d+)", src)
    expect(m is not None, "没找到 LEVEL 的默认值回退表达式")
    expect(int(m.group(1)) == 4, f"无 UCFG 时应回退到 4，实际 {m.group(1)}")
    print("  PASS 无 UCFG（frida -l 直接用）时回退 level=4")


def test_payload_coercion():
    """frida 16+ 直接给 dict，老版本给 JSON 串——两种都必须能解析。

    之前只写 json.loads(payload)，实测每帧刷
    "the JSON object must be str, bytes or bytearray, not dict"，
    看起来像游戏挂了，其实帧数据一直是好的。
    """
    frame = {"type": "frame", "tick": 1, "w2c": [1] * 16, "proj": [1] * 16,
             "local": None, "players": [], "width": 1280, "height": 720}
    got = frida_host._coerce_payload(frame)
    expect(got is frame or got == frame, "dict payload 应原样可用")
    got2 = frida_host._coerce_payload(json.dumps(frame))
    expect(got2 == frame, "JSON 字符串 payload 应能解析")
    got3 = frida_host._coerce_payload(json.dumps(frame).encode("utf-8"))
    expect(got3 == frame, "bytes payload 应能解析")
    try:
        frida_host._coerce_payload(12345)
        raise AssertionError("未知类型应抛 TypeError")
    except TypeError:
        pass
    print("  PASS payload 兼容 dict / str / bytes，未知类型抛 TypeError")


def main():
    print("脚本注入配置 / 档位 / 消息解析测试（离线）")
    for fn in (test_ucfg_shape, test_defaults, test_perf_guards,
               test_levels_implemented,
               test_fallback_without_ucfg, test_payload_coercion,
               test_probe_and_flags, test_probe_steps_synced,
               test_singleton_avoids_generic_call,
               test_isdead_not_called_by_default, test_main_thread_scheduling):
        fn()
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
