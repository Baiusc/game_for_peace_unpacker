#!/usr/bin/env python3
# UCF-ESP 进程探针：当出现多个同名 UnityCrossFire.exe 时，找出“真正加载了
# GameAssembly.dll 的那个进程”。
#
# 背景（实测坑）：某些 Unity 单机包会残留 10MB 左右的同名小进程（启动器/单实例
# 存根），而真正跑游戏的主进程有几百 MB。按进程名取“第一个匹配”会注进空壳，
# 表现就是 frida-il2cpp-bridge 报：
#   after 10 seconds, IL2CPP module 'GameAssembly.dll' has not been loaded yet
#
# 用法（本机 Windows）：
#   python tools/frida_probe.py                    # 探测所有 UnityCrossFire.exe
#   python tools/frida_probe.py --name other.exe   # 指定进程名
#   python tools/frida_probe.py --pid 17304        # 只探测指定 pid
#
# 输出示例：
#   pid=17304  arch=ia32  ptr=4  modules=142  GameAssembly.dll=YES
#   pid=6964   arch=ia32  ptr=4  modules=21   GameAssembly.dll=no
#
# 本工具只做只读枚举（attach + 读模块列表 + detach），不修改游戏内存。

import sys
import time
import argparse
import threading

# 注意：这里不在模块顶层 import frida，方便没装 frida 的机器仍能 import 本模块
# （frida_host.py 的 --replay 离线模式就依赖这一点）。

DEFAULT_NAME = "UnityCrossFire.exe"
DEFAULT_TIMEOUT = 3.0

# 只读探针：报告架构与是否已加载 GameAssembly.dll，不做任何写操作
PROBE_JS = r"""
(function () {
    var mods = Process.enumerateModules().map(function (m) { return m.name; });
    var hit = mods.filter(function (n) {
        return n.toLowerCase() === 'gameassembly.dll';
    });
    var mono = mods.filter(function (n) {
        return n.toLowerCase() === 'mono-2.0-bdwgc.dll' || n.toLowerCase() === 'mono.dll';
    });
    send({
        arch: Process.arch,
        pointerSize: Process.pointerSize,
        moduleCount: mods.length,
        hasGameAssembly: hit.length > 0,
        hasMono: mono.length > 0,
        platform: Process.platform
    });
})();
"""


def probe(device, pid, timeout=DEFAULT_TIMEOUT):
    """attach 到 pid，读取架构与模块表，返回 dict；失败返回 {'error': ...}。"""
    session = None
    try:
        session = device.attach(pid)
        script = session.create_script(PROBE_JS)
        box = {}
        done = threading.Event()

        def on_message(message, data):
            if message.get("type") == "send":
                box.update(message["payload"])
                done.set()

        script.on("message", on_message)
        script.load()
        done.wait(timeout)
        if not box:
            return {"error": f"探针 {timeout:.0f}s 内无响应（进程可能已挂起/受保护）"}
        return box
    except Exception as e:  # attach 被拒、进程退出、权限不足等
        return {"error": f"{type(e).__name__}: {e}"}
    finally:
        if session is not None:
            try:
                session.detach()
            except Exception:
                pass


def pids_by_name(device, name):
    name = name.lower()
    return [p.pid for p in device.enumerate_processes() if p.name.lower() == name]


def wait_for_module(device, pid, timeout, interval=0.4):
    """等待目标进程把 GameAssembly.dll 载入（游戏刚启动时 IL2CPP 还没加载完）。"""
    deadline = time.time() + timeout
    last = {}
    while time.time() < deadline:
        last = probe(device, pid)
        if last.get("hasGameAssembly"):
            return last
        time.sleep(interval)
    return last


def main():
    import frida

    ap = argparse.ArgumentParser(description="UCF-ESP 进程探针：找对真正跑 IL2CPP 的进程")
    ap.add_argument("--name", default=DEFAULT_NAME, help=f"进程名（默认 {DEFAULT_NAME}）")
    ap.add_argument("--pid", type=int, default=None, help="只探测指定 pid")
    ap.add_argument("--wait", type=float, default=0.0,
                    help="对每个候选等待 GameAssembly.dll 出现的秒数（默认 0，只探测一次）")
    args = ap.parse_args()

    device = frida.get_local_device()

    if args.pid:
        candidates = [args.pid]
    else:
        candidates = pids_by_name(device, args.name)
        if not candidates:
            print(f"[!] 没有找到名为 {args.name} 的进程，请先启动游戏")
            return 1

    print(f"[*] 候选进程 {len(candidates)} 个: {candidates}")
    print(f"[*] frida {frida.__version__}")
    print()

    good = []
    for pid in candidates:
        info = wait_for_module(device, pid, args.wait) if args.wait > 0 else probe(device, pid)
        if "error" in info:
            print(f"  pid={pid:<8} 探测失败: {info['error']}")
            continue
        ok = info["hasGameAssembly"]
        flag = "YES" if ok else "no "
        extra = " (Mono 运行时，非 IL2CPP)" if info.get("hasMono") else ""
        print(f"  pid={pid:<8} arch={info['arch']:<5} ptr={info['pointerSize']} "
              f"modules={info['moduleCount']:<4} GameAssembly.dll={flag}{extra}")
        if ok:
            good.append(pid)

    print()
    if len(good) == 1:
        print(f"[OK] 目标进程 = {good[0]}（唯一加载了 GameAssembly.dll 的进程）")
        print(f"     可直接: python tools/frida_host.py --no-launch --pid {good[0]}")
        return 0
    if len(good) > 1:
        print(f"[!] 有多个进程都加载了 GameAssembly.dll: {good}")
        print("    Unity 多开通常不可用，建议只保留一个实例。")
        return 2
    print("[!] 没有候选进程加载 GameAssembly.dll。")
    print("    1) 若游戏还在启动/加载中，用 --wait 5 再等一会儿；")
    print("    2) 若全部是同名小进程，说明真进程未启动或已退出；")
    print("    3) 目标是 Mono 运行时（见上面标注）则不是 IL2CPP，需另换思路。")
    return 3


if __name__ == "__main__":
    sys.exit(main())
