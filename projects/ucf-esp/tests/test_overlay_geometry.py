#!/usr/bin/env python3
# 叠加层几何映射与显示策略的离线测试（不需要游戏、不需要显示器）。
#
# 覆盖的都是"实测踩过坑"的点：
#   A. 帧坐标 -> 屏幕坐标的映射（DPI 250% + 4:3 游戏全屏到 16:9 的黑边）
#   B. 叠加层窗口几何是否等于映射出的显示区
#   C. 前台判断必须排除叠加层自己的句柄，否则会周期性闪烁
#   D. 隐藏要有迟滞，不能一失焦就藏
#   E. 重申 topmost 必须走原生不激活调用（Tk 的 lift 会闪 + 抢焦点）
#
# 用假 Tk / 假 user32 跑，因此可以在无窗口环境执行。

import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(PROJ, "tools"))

import overlay_tk as O  # noqa: E402

FAILED = []


def expect(cond, msg):
    if not cond:
        FAILED.append(msg)
        print(f"  FAIL {msg}")


# --------------------------------------------------------------------- 假 Tk
class FakeCanvas:
    def __init__(self):
        self.size = None
        self.items = {}
        self._n = 0

    def pack(self):
        pass

    def config(self, **kw):
        self.size = (kw.get("width"), kw.get("height"))

    def _new(self, *a, **k):
        self._n += 1
        self.items[self._n] = dict(k)
        return self._n

    create_rectangle = _new
    create_text = _new

    def itemconfigure(self, i, **k):
        if i in self.items:
            self.items[i].update(k)

    def coords(self, i, *a):
        pass

    def delete(self, *a):
        pass


class FakeRoot:
    def __init__(self):
        self.geo = None
        self.state = "normal"
        self.lifted = False

    def overrideredirect(self, *a):
        pass

    def attributes(self, *a):
        pass

    def geometry(self, g):
        self.geo = g

    def withdraw(self):
        self.state = "withdrawn"

    def deiconify(self):
        self.state = "normal"

    def update(self):
        pass

    def update_idletasks(self):
        pass

    def winfo_id(self):
        return 0x1111

    def lift(self):
        self.lifted = True


class FakeUser32:
    """只实现 overlay_tk 用到的几个调用。"""

    def __init__(self):
        self.fg = 0
        self.minimized = False
        self.visible = True

    def GetForegroundWindow(self):
        return self.fg

    def IsIconic(self, hwnd):
        return self.minimized

    def IsWindowVisible(self, hwnd):
        return self.visible

    def GetAncestor(self, hwnd, flag):
        return hwnd


GAME_HWND = 0x600D
OWN_HWND = 0x0BAD
OTHER_HWND = 0x9999
CLIENT = (0, 0, 3840, 2160)      # 4K 全屏，客户区 == 屏幕


def install_fakes():
    O.tk = types.SimpleNamespace(Tk=FakeRoot, Canvas=lambda *a, **k: FakeCanvas())
    O.enable_dpi_awareness = lambda: "fake-dpi"
    O.root_hwnd = lambda h: OWN_HWND
    # 注意：必须真的判空 —— 恒返回 True 会让 _probe 误以为 hwnd 有效而跳过查找
    O._is_window = lambda h: h is not None
    O.find_game_hwnd = lambda pid=None, class_name=None: GAME_HWND
    O.client_rect = lambda hwnd: CLIENT
    fake = FakeUser32()
    fake.fg = GAME_HWND          # 默认场景：游戏在前台
    O._u32 = lambda: fake
    topmost_calls = []
    O.set_topmost = lambda hwnd: (topmost_calls.append(hwnd), True)[1]
    click_calls = []
    O.set_click_through = lambda hwnd, no_activate=True: (
        click_calls.append((hwnd, no_activate)), True)[1]
    return fake, topmost_calls, click_calls


def make_overlay(**kw):
    kw.setdefault("pid", 4242)
    kw.setdefault("fit", "auto")
    return O.OverlayWindow(**kw)


def expire_hysteresis(w):
    """模拟"迟滞时间已过"：不能改 HIDE_DELAY_S —— _hide_at 早就按旧值算好了。"""
    w._hide_at = 0.0


# --------------------------------------------------------------------- A. 映射
def test_viewport_matches_measured_screenshot():
    """核心场景：4K 全屏 + 帧 800x600 -> 显示区 (480,0,2880,2160)，等比 3.6x。

    480 / 2880 这两个数是**从用户截图里量出来的**：
    黑列范围 0..476 与 3360..3836。
    """
    vp = O.compute_viewport(CLIENT, 800, 600, "auto")
    expect((vp["x"], vp["y"], vp["w"], vp["h"]) == (480, 0, 2880, 2160),
           f"显示区应为 (480,0,2880,2160)，实得 {(vp['x'], vp['y'], vp['w'], vp['h'])}")
    expect(abs(vp["sx"] - 3.6) < 1e-9 and abs(vp["sy"] - 3.6) < 1e-9,
           f"缩放应为 3.6x，实得 {vp['sx']}/{vp['sy']}")
    expect(vp["mode"] == "letterbox", f"宽高比不同应走 letterbox，实得 {vp['mode']}")
    print("  PASS 4K 全屏 + 800x600 -> (480,0,2880,2160) 等比 3.6x letterbox")


def test_viewport_same_aspect_is_stretch():
    """客户区与帧同宽高比时不该加黑边，直接铺满（偏移必须为 0）。"""
    for client, fw, fh, scale in (
        ((100, 50, 800, 600), 800, 600, 1.0),        # 原生 800x600 窗口化
        ((0, 0, 1600, 1200), 800, 600, 2.0),         # 2x
        ((0, 0, 1920, 1440), 1600, 1200, 1.2),       # 非整数倍
    ):
        vp = O.compute_viewport(client, fw, fh, "auto")
        expect(vp["mode"] == "stretch", f"{client} 同比例应 stretch，实得 {vp['mode']}")
        expect((vp["x"], vp["y"]) == (client[0], client[1]),
               f"stretch 时原点应为客户区原点，实得 ({vp['x']},{vp['y']})")
        expect(abs(vp["sx"] - scale) < 1e-6 and abs(vp["sy"] - scale) < 1e-6,
               f"{client} 缩放应为 {scale}，实得 {vp['sx']}")
    print("  PASS 同宽高比 -> stretch（不产生黑边，原点 = 客户区原点）")


def test_viewport_auto_threshold():
    """auto 只在宽高比差 >2% 时才 letterbox —— 否则轻微差异会莫名留边。"""
    vp = O.compute_viewport((0, 0, 1000, 750), 800, 600, "auto")   # 恰好 4:3
    expect(vp["mode"] == "stretch", "完全同比例必须 stretch")
    vp = O.compute_viewport((0, 0, 1010, 750), 800, 600, "auto")   # 差 1.3%
    expect(vp["mode"] == "stretch", f"1.3% 差异应容忍为 stretch，实得 {vp['mode']}")
    vp = O.compute_viewport((0, 0, 1080, 750), 800, 600, "auto")   # 差 8%
    expect(vp["mode"] == "letterbox", f"8% 差异应 letterbox，实得 {vp['mode']}")
    print("  PASS auto 阈值：≤2% 视为同比例，>2% 才 letterbox")


def test_forced_modes_respected():
    """--overlay-fit stretch/letterbox 要能强制覆盖自动判断。"""
    vp = O.compute_viewport((0, 0, 1920, 1080), 800, 600, "stretch")
    expect(vp["mode"] == "stretch" and vp["sx"] == 2.4 and vp["sy"] == 1.8,
           f"强制 stretch 应为 2.4/1.8，实得 {vp['sx']}/{vp['sy']}")
    vp = O.compute_viewport((0, 0, 1920, 1080), 1600, 1200, "letterbox")
    # s = min(1920/1600, 1080/1200) = min(1.2, 0.9) = 0.9
    expect((vp["x"], vp["y"], vp["w"], vp["h"]) == (240, 0, 1440, 1080),
           f"强制 letterbox 应为 (240,0,1440,1080)，实得 "
           f"{(vp['x'], vp['y'], vp['w'], vp['h'])}")
    print("  PASS 强制 stretch / letterbox 均被遵守")


def test_map_and_to_screen_agree():
    """画布坐标 + 显示区原点 == 屏幕坐标；屏幕中心必须落在显示区中心。"""
    vp = O.compute_viewport(CLIENT, 800, 600, "auto")
    expect(O.map_point(vp, 400, 300) == (1440.0, 1080.0),
           f"帧中心应映射到画布 (1440,1080)，实得 {O.map_point(vp, 400, 300)}")
    expect(O.to_screen(vp, 400, 300) == (1920.0, 1080.0),
           f"帧中心应映射到屏幕 (1920,1080)，实得 {O.to_screen(vp, 400, 300)}")
    for x, y in ((0, 0), (800, 600), (123.4, 567.8)):
        mx, my = O.map_point(vp, x, y)
        sx, sy = O.to_screen(vp, x, y)
        expect(abs(sx - (vp["x"] + mx)) < 1e-9 and abs(sy - (vp["y"] + my)) < 1e-9,
               f"map_point 与 to_screen 在 ({x},{y}) 不一致")
    print("  PASS map_point / to_screen 自洽，帧中心落在显示区中心")


# --------------------------------------------------------------------- B~E 窗口
def test_window_geometry_follows_viewport():
    install_fakes()
    w = make_overlay()
    expect(w.root.state == "withdrawn", "第一帧之前必须保持隐藏（否则左上角会闪一下）")
    expect(w._own_hwnd == OWN_HWND, f"应记录自身句柄，实得 {w._own_hwnd}")

    w.update([], 800, 600)
    expect(w.root.state == "normal", "有帧且游戏在前台时应显示")
    expect(w.root.geo == "2880x2160+480+0",
           f"窗口几何应为显示区 2880x2160+480+0，实得 {w.root.geo}")
    expect(w.canvas.size == (2880, 2160), f"画布应为 2880x2160，实得 {w.canvas.size}")
    print("  PASS 窗口几何/画布尺寸 == 映射出的显示区（不再按客户区原尺寸瞎铺）")


def test_foreground_ignores_own_window():
    """叠加层自己是 topmost，偶尔会被系统当成前台窗口。

    若不排除自身句柄，is_foreground(游戏) 会瞬间 False -> 隐藏 -> 焦点回游戏 ->
    再显示，形成周期性闪烁。这正是"游戏内闪烁"的一个真凶。
    """
    fake = install_fakes()[0]
    w = make_overlay()
    w.update([], 800, 600)
    expect(w._shown is True, "初始应显示")

    fake.fg = OWN_HWND          # 前台变成"我们自己"
    # 直接调底层判断，确认它把自身句柄视为"游戏仍在前台"
    expect(O.is_foreground(GAME_HWND, ignore=(OWN_HWND,)) is True,
           "前台是自己的句柄时应判定为仍在前台（否则会自我隐藏）")
    expect(O.is_foreground(GAME_HWND, ignore=()) is False,
           "不排除自身时确实会误判 —— 这条用来证明上面那条测试有区分力")
    w.update([], 800, 600)
    expect(w._shown is True, "前台是自己时不得隐藏（否则闪烁）")

    fake.fg = OTHER_HWND            # 前台真是别的程序
    expect(O.is_foreground(GAME_HWND, ignore=(OWN_HWND,)) is False,
           "前台是别的程序时应判定为不在前台")
    print("  PASS 前台判断排除自身句柄（并验证不排除确实会误判）")


def test_hide_has_hysteresis():
    """失焦不能立刻藏，要有迟滞；迟滞过后必须藏。"""
    fake = install_fakes()[0]
    w = make_overlay()
    w.update([], 800, 600)
    expect(w._shown is True, "初始应显示")

    fake.fg = OTHER_HWND
    before = O.time.time()
    w.update([], 800, 600)
    expect(w._shown is True, "刚失焦就隐藏 —— 切菜单/通知抢焦点时会抖动")
    expect(w._hide_at is not None, "失焦后应开始计时（迟滞）")
    expect(abs((w._hide_at - before) - O.HIDE_DELAY_S) < 0.2,
           f"迟滞时长应约为 {O.HIDE_DELAY_S}s，实得 {w._hide_at - before:.3f}s")

    expire_hysteresis(w)
    w.update([], 800, 600)          # 到点，允许隐藏
    expect(w._shown is False, "迟滞到期后应隐藏")
    expect(w.root.state == "withdrawn", "隐藏应走 withdraw，而不是留个空窗")

    fake.fg = GAME_HWND             # 回到游戏 -> 立刻恢复
    w.update([], 800, 600)
    expect(w._shown is True and w._hide_at is None, "回到游戏应立即恢复显示并清掉迟滞")
    print("  PASS 隐藏有 0.35s 迟滞；回到游戏立即恢复")


def test_minimized_hides():
    fake = install_fakes()[0]
    w = make_overlay()
    w.update([], 800, 600)
    expect(w._shown is True, "初始应显示")

    fake.minimized = True
    expire_hysteresis(w)
    w.update([], 800, 600)
    expect(w._shown is False, "最小化时必须隐藏")

    fake.minimized = False
    fake.visible = False
    w.update([], 800, 600)
    expect(w._shown is False, "窗口不可见时必须隐藏")
    print("  PASS 最小化 / 窗口不可见时隐藏")


def test_topmost_reassert_is_native_and_no_lift():
    """重申 topmost 必须用原生不激活调用：Tk 的 lift()/attributes 会闪 + 抢焦点。"""
    _, calls, _ = install_fakes()
    w = make_overlay()
    w.update([], 800, 600)
    w._next_topmost = 0.0           # 强制走一次重申
    w.update([], 800, 600)
    w._next_topmost = 0.0
    w.update([], 800, 600)
    expect(len(calls) > 0, "重申 topmost 应调用原生 set_topmost")
    expect(all(h == OWN_HWND for h in calls),
           f"应针对自身句柄重申，实得 {calls}")
    expect(w.root.lifted is False, "不得使用 Tk lift()（会闪烁并抢焦点）")
    print(f"  PASS 重申 topmost 走原生调用 ({len(calls)} 次)，未使用 lift()")


def test_click_through_applied():
    """显示时必须让整窗不接收鼠标。

    Tk 的 `-transparentcolor` 只设 LWA_COLORKEY —— 只有纯黑像素点击才穿透，
    而框和文字是红蓝绿，会拦住游戏里的鼠标（表现为"偶尔点不动"）。
    """
    _, _, clicks = install_fakes()
    w = make_overlay()
    w.update([], 800, 600)
    expect(w._shown is True, "初始应显示")
    expect(len(clicks) > 0, "显示时必须调用 set_click_through")
    expect(all(h == OWN_HWND for h, _ in clicks),
           f"应针对自身句柄设置，实得 {clicks}")
    expect(all(na for _, na in clicks), "应同时置 WS_EX_NOACTIVATE（永不抢焦点）")
    print(f"  PASS 显示时整窗鼠标穿透 + 禁止激活（{len(clicks)} 次）")


def test_push_is_thread_safe_queue():
    """宿主在 frida 消息线程上调 push，真正绘制必须发生在主线程 _drain。

    tkinter 只保证创建它的线程可用 —— 跨线程直接调 canvas.* 是未定义行为。
    """
    fake = install_fakes()[0]
    w = make_overlay()
    # 还没 drain，队列里堆着，主线程什么都还没做
    w.push(["m1"], 800, 600)
    w.push(["m2"], 800, 600)
    w.push(["m3"], 800, 600)
    expect(w._q.qsize() == 3, f"三次 push 应入队 3 项，实得 {w._q.qsize()}")
    expect(w._last_draw == 0.0, "push 不得直接触发绘制（那会在错误线程里跑）")

    drawn = []
    w.update = lambda marks, fw=None, fh=None: drawn.append((marks, fw, fh))
    w._drain()
    expect(len(drawn) == 1, f"drain 只应绘制一次（丢弃积压的旧帧），实得 {len(drawn)}")
    expect(drawn[0][0] == ["m3"], f"应绘制最新一帧，实得 {drawn[0][0]}")
    expect(w._q.qsize() == 0, "drain 后队列应为空")

    w.push(["x"], 800, 600)
    w.request_clear()
    cleared = []
    w.clear = lambda: cleared.append(1)
    w._drain()
    expect(len(cleared) == 1 and w._clear_req is False,
           "request_clear 应被主线程消费且只生效一次")
    print("  PASS push/request_clear 走队列，绘制与清屏只在主线程发生")


def test_tick_drains_queue():
    """_tick（主线程定时器）应消费队列、断流清屏、重申置顶。"""
    fake, calls, _ = install_fakes()
    w = make_overlay()
    w.root.after = lambda ms, fn: None      # 不真的排下一次，避免递归
    drawn = []
    w.update = lambda marks, fw=None, fh=None: drawn.append(marks)
    w.push(["m1"], 800, 600)
    w._tick()
    expect(drawn == [["m1"]], f"第一次 _tick 应消费队列并绘制，实得 {drawn}")
    expect(w._last_draw > 0, "_tick 后应记录最近收帧时间")

    # 断流：把最近收帧时间推老 -> 应清屏
    cleared = []
    w.clear = lambda: cleared.append(1)
    w._last_draw = 0.0            # 0 表示"还没有过帧"，不该清
    w._tick()
    expect(cleared == [], "_last_draw 为 0（从未收帧）时不得清屏")
    w._last_draw = O.time.time() - O.STALE_CLEAR_S - 1
    w._tick()
    expect(len(cleared) == 1, f"帧断流超过 {O.STALE_CLEAR_S}s 应清屏，实得 {len(cleared)}")
    print("  PASS _tick 消费队列 + 断流清屏（首次启动不清）")


def test_push_drops_oldest_when_backlogged():
    """队列有上限，积压时丢旧帧而不是无限涨。"""
    install_fakes()
    w = make_overlay()
    w._q = __import__("queue").Queue(maxsize=4)
    for i in range(20):
        w.push([f"m{i}"], 800, 600)
    expect(w._q.qsize() <= 4, f"队列不应超过上限，实得 {w._q.qsize()}")
    last = None
    while not w._q.empty():
        last = w._q.get_nowait()
    expect(last is not None and last[0] == ["m19"], f"最新一帧必须保留，实得 {last}")
    print("  PASS 队列积压时丢旧帧、保留最新")


def test_no_clip_always_shows():
    """--no-overlay-clip：不做前台判断，始终显示（多屏调试时用）。"""
    fake = install_fakes()[0]
    w = make_overlay(clip=False)
    fake.fg = OTHER_HWND
    w.update([], 800, 600)
    expect(w._shown is True, "clip=False 时应无视前台状态始终显示")
    print("  PASS --no-overlay-clip 时不判断前台")


def test_manual_rect_overrides_probe():
    """找不到游戏窗口时的兜底：--overlay-rect x,y,w,h 手动指定显示区。

    用了手动显示区**也要**去找游戏窗口 —— 前台判断靠它，
    否则切到桌面后框会一直挂在屏幕上。
    """
    install_fakes()
    w = make_overlay(rect=(480, 0, 2880, 2160), fit="stretch")
    w.update([], 800, 600)
    expect(w.client == (480, 0, 2880, 2160),
           f"应采信 manual_rect，实得 {w.client}")
    expect(w.root.geo == "2880x2160+480+0",
           f"几何应为 (480,0,2880,2160)，实得 {w.root.geo}")
    expect(w.hwnd == GAME_HWND,
           f"手动显示区时仍要定位游戏窗口（前台判断用），实得 hwnd={w.hwnd}")
    print("  PASS --overlay-rect 生效，且仍会定位游戏窗口")


def main():
    print("叠加层几何映射 / 显示策略测试（离线，假 Tk + 假 user32）")
    for fn in (test_viewport_matches_measured_screenshot,
               test_viewport_same_aspect_is_stretch,
               test_viewport_auto_threshold,
               test_forced_modes_respected,
               test_map_and_to_screen_agree,
               test_window_geometry_follows_viewport,
               test_foreground_ignores_own_window,
               test_hide_has_hysteresis,
               test_minimized_hides,
               test_topmost_reassert_is_native_and_no_lift,
               test_click_through_applied,
               test_push_is_thread_safe_queue,
               test_tick_drains_queue,
               test_push_drops_oldest_when_backlogged,
               test_no_clip_always_shows,
               test_manual_rect_overrides_probe):
        fn()
    if FAILED:
        print(f"\n{len(FAILED)} 项失败：")
        for m in FAILED:
            print("  -", m)
        return 1
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
