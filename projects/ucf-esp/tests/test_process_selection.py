#!/usr/bin/env python3
# 进程选择逻辑的离线单元测试（不需要 frida、不需要游戏）。
#
# 背景：同名 UnityCrossFire.exe 可能同时存在多个（真实游戏 + 若干 ~10MB 空壳）。
# 如果按“第一个同名匹配”去 attach，就会注到空壳里，frida-il2cpp-bridge 报
#   after 10 seconds, IL2CPP module 'GameAssembly.dll' has not been loaded yet
# 本测试用假进程表验证 select_game_pid 的选择/报错行为。
#
# 运行：
#   python tests/test_process_selection.py

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))

import frida_host    # noqa: E402
import frida_probe   # noqa: E402

FAKE_PIDS = [6964, 14500, 17304]   # 两个空壳 + 一个真游戏（真实实测过的组合）


def _info(has_ga):
    return {"arch": "ia32", "pointerSize": 4,
            "moduleCount": 142 if has_ga else 21,
            "hasGameAssembly": has_ga, "hasMono": False, "platform": "windows"}


def test_pick_real_game_among_shells():
    frida_probe.pids_by_name = lambda dev, name: list(FAKE_PIDS)
    frida_probe.probe = lambda dev, pid, timeout=2.5: _info(pid == 17304)
    pid = frida_host.select_game_pid(None, "UnityCrossFire.exe", wait_seconds=0, quiet=True)
    assert pid == 17304, f"应选中 17304，实际 {pid}"
    print("  PASS 多实例 -> 选中加载了 GameAssembly.dll 的 17304")


def test_all_shells_raises():
    frida_probe.pids_by_name = lambda dev, name: list(FAKE_PIDS)
    frida_probe.probe = lambda dev, pid, timeout=2.5: _info(False)
    try:
        frida_host.select_game_pid(None, "UnityCrossFire.exe", wait_seconds=0, quiet=True)
    except RuntimeError as e:
        assert "GameAssembly.dll" in str(e)
        print("  PASS 全是空壳 -> 抛出带排查建议的 RuntimeError")
        return
    raise AssertionError("全是空壳时应抛 RuntimeError，但正常返回了")


def test_no_candidate_raises():
    frida_probe.pids_by_name = lambda dev, name: []
    try:
        frida_host.select_game_pid(None, "UnityCrossFire.exe", wait_seconds=0, quiet=True)
    except RuntimeError as e:
        assert "没有找到进程" in str(e)
        print("  PASS 没有同名进程 -> 提示先启动游戏")
        return
    raise AssertionError("没有候选进程时应抛 RuntimeError")


def test_forced_pid_wins():
    def _boom(*a, **k):
        raise AssertionError("指定 --pid 时不应再探测")
    frida_probe.probe = _boom
    pid = frida_host.select_game_pid(None, "UnityCrossFire.exe", forced_pid=42)
    assert pid == 42
    print("  PASS --pid 42 -> 直接采用，不探测")


def test_wait_until_module_appears():
    """游戏刚启动、IL2CPP 还没加载：应等一会儿而不是立刻报错。"""
    state = {"n": 0}

    def probe(dev, pid, timeout=2.5):
        state["n"] += 1
        return _info(state["n"] >= 3)   # 第 3 轮才加载出来

    frida_probe.pids_by_name = lambda dev, name: [17304]
    frida_probe.probe = probe
    pid = frida_host.select_game_pid(None, "UnityCrossFire.exe", wait_seconds=5, quiet=True)
    assert pid == 17304 and state["n"] >= 3
    print(f"  PASS 模块延迟加载 -> 轮询 {state['n']} 次后选中 17304")


def test_shell_detection_for_kill_stale():
    """--kill-stale 只应挑出空壳，不能误杀真游戏。"""
    frida_probe.pids_by_name = lambda dev, name: list(FAKE_PIDS)
    frida_probe.probe = lambda dev, pid, timeout=2.5: _info(pid == 17304)
    killed = []
    frida_host._taskkill_pids = lambda pids: killed.extend(pids)
    frida_host.time.sleep = lambda s: None
    shells = frida_host.kill_stale_shells(None, "UnityCrossFire.exe")
    assert sorted(shells) == [6964, 14500], shells
    assert 17304 not in killed, "不能杀掉真正加载了 GameAssembly 的进程"
    print(f"  PASS 空壳识别 -> {sorted(shells)}，游戏进程 17304 未被误杀")


def test_resolve_game_path_existing_file():
    """--game 给完整路径且文件存在 -> 直接采用（不再 Popen 一个裸文件名）。"""
    got = frida_host.resolve_game_path(__file__)
    assert got and os.path.isabs(got) and os.path.exists(got), got
    print("  PASS --game 指向真实文件 -> 直接采用绝对路径")


def test_resolve_game_path_bare_name_and_missing():
    """裸文件名：能在已知目录里找到就自动定位；找不到要返回 None 并给提示。"""
    tmp = os.path.join(HERE, "_tmp_fake_game.exe")
    with open(tmp, "wb") as f:
        f.write(b"MZ")
    old_known = frida_host.KNOWN_GAME_PATHS
    old_env = os.environ.pop("UCF_GAME", None)
    try:
        frida_host.KNOWN_GAME_PATHS = (tmp,)
        got = frida_host.resolve_game_path("NoSuchGame.exe")
        assert got and os.path.samefile(got, tmp), got
        print("  PASS 裸文件名 -> 在已知安装目录/UCF_GAME 里自动定位")

        frida_host.KNOWN_GAME_PATHS = ()
        assert frida_host.resolve_game_path("NoSuchGame.exe") is None
        print("  PASS 到处都找不到 -> 返回 None（打印三种指定方式，不抛 WinError）")
    finally:
        frida_host.KNOWN_GAME_PATHS = old_known
        if old_env is not None:
            os.environ["UCF_GAME"] = old_env
        try:
            os.remove(tmp)
        except OSError:
            pass


def main():
    print("进程选择逻辑测试（离线，无需 frida/游戏）")
    for fn in (test_pick_real_game_among_shells,
               test_all_shells_raises,
               test_no_candidate_raises,
               test_forced_pid_wins,
               test_wait_until_module_appears,
               test_shell_detection_for_kill_stale,
               test_resolve_game_path_existing_file,
               test_resolve_game_path_bare_name_and_missing):
        fn()
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
