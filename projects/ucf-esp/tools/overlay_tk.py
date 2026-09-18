#!/usr/bin/env python3
# UCF-ESP 实时叠加层（Windows + tkinter）。
#
# 这一版把「框位置不对 / 画到别的窗口上 / 闪烁」三个问题一起修掉，
# 下面每一条都是实测证据换来的，改代码前先读完：
#
# 1)【DPI：位置全错的头号原因】
#    宿主进程默认是 DPI-**unaware**。实测本机 4K + 250% 缩放：未声明时
#    GetSystemMetrics 只返回 1536x864，而屏幕实际是 3840x2160 —— 也就是说
#    Tk 窗口是"逻辑像素"布局，再被 Windows 整体放大 2.5 倍。于是 800x600 的画布
#    在屏幕上变成 2000x1500，画出来的框实测约 100x150（= 40x60 x 2.5），
#    位置也整体乘了 2.5。**必须先 SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)**，
#    之后所有取到的坐标才是物理像素，和光标/截图同一套坐标系。
#
# 2)【映射：帧坐标 ≠ 屏幕坐标】
#    投影出来的是"帧坐标系"（Screen.get_width/height，本游戏实测 800x600），
#    而游戏窗口在全屏 4K 上是 **2880x2160 居中、左右各留黑边**：实测截图的
#    黑列范围 0..476 / 3360..3836，即显示区 (480, 0, 2880, 2160)，相对 800x600
#    是 3.6 倍等比缩放。所以必须按客户区做**映射**，且宽高比不一致时要
#    letterbox（等比 + 居中）而不是 stretch —— 实测 letterbox 算出来的
#    (480,0,2880,2160)/3.6 与截图像素完全吻合。
#
# 3)【窗口定位】按 pid 找 UnityWndClass 顶层窗口。不用 FindWindowW 按类名
#    （系统里可能有别的 Unity 窗口，实测抓到过无关的 320x240 窗口），
#    也不用 EnumWindows 回调（64 位 ctypes 回调易 OverflowError）。
#    遍历用 GetTopWindow + GetWindow(GW_HWNDNEXT)，无回调。
#
# 4)【别画在别的窗口上】游戏不在前台 / 最小化 -> withdraw() 隐藏。
#
# 5)【闪烁】不要每帧 canvas.delete("all")：删除到重建之间会露出一帧全黑，
#    游戏在前台跑高帧率时就是肉眼可见的闪烁。改成复用 canvas item 只改坐标，
#    并且内容没变化（坐标四舍五入到 0.1px 相同）时直接跳过重绘。
#    另外两条同样致命的闪烁源：
#      a) 重申 topmost 用 Tk 的 lift()/attributes("-topmost") 会闪一下，
#         还会抢走前台焦点 —— 而"游戏是前台才显示"的判断随即变 False，
#         于是隐藏自己 → 焦点回到游戏 → 再显示，2 秒一轮的闪烁。
#         改用 SetWindowPos(SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE) 原地重申。
#      b) 前台判断必须**排除自己的句柄**：叠加层是 topmost，偶尔会被系统
#         当成前台窗口；不排除就会自己判自己出局，同样是周期性闪烁。
#      c) 隐藏加 0.35s 迟滞，避免切菜单/系统通知抢焦点时「藏-显-藏」抖动。
#
# 6)【别挡住鼠标】Tk 的 `-transparentcolor` 只设 LWA_COLORKEY：**纯黑**区域点击才穿透，
#    我们画的框/文字是红蓝绿，那些像素会拦住鼠标 —— 游戏里会莫名"点不动"。
#    必须补 WS_EX_TRANSPARENT（整窗不吃鼠标）+ WS_EX_NOACTIVATE（永不抢焦点）。
#
# 7)【线程】tkinter 只保证**创建它的线程**可用。宿主的 frida 消息回调、看门狗
#    都在别的线程上，所以它们不能直接碰 canvas/geometry。宿主只调
#    `push()` / `request_clear()` 入队，真正的 Tk 操作全在 `_tick()`（主线程）里做。

import ctypes
import queue
import time
import tkinter as tk
from ctypes import wintypes

COLOR = {
    "local": "#39d353",
    "teammate": "#58a6ff",
    "enemy": "#f85149",
}

UNITY_WINDOW_CLASS = "UnityWndClass"
FIT_MODES = ("auto", "stretch", "letterbox")

# 帧坐标系里的框尺寸（像素），绘制时按映射比例缩放，才能和游戏内 HUD 同比例
BOX_W, BOX_H = 40, 60
BASE_FONT = 11
TOPMAST_REASSERT_S = 2.0
PROBE_INTERVAL_S = 0.5

GW_HWNDNEXT = 2
GA_ROOT = 2

# 原位重申 topmost 用：不动位置/尺寸、不激活、不重绘 —— 比 Tk 的
# lift()/attributes("-topmost") 安静得多（后者会闪烁，还可能抢走前台焦点，
# 进而让 is_foreground(游戏) 变 False → 自己把自己隐藏，形成 2s 周期的闪烁）。
HWND_TOPMOST = -1
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010
SWP_NOOWNERZORDER = 0x0200
SWP_NOSENDCHANGING = 0x0400
SWP_SHOWWINDOW = 0x0040

# 隐藏加迟滞：短暂失焦不立刻藏，避免"藏-显-藏"高频抖动
HIDE_DELAY_S = 0.35

# Tk 主线程定时器：消费帧队列 + 帧断流清屏 + 定期重申置顶。
# 旧值是 100ms（≈10Hz）——这是“框跟不住游戏”的头号原因：游戏跑 60fps，
# 叠加层却每 100ms 才重绘一次。改成 16ms（≈60Hz）即可与游戏同步；
# 真机若还嫌不够可再降到 8，但 itemconfigure/coords 的 Tcl 往返会随之变多。
# 注意：这只是“重绘上限”。真正的数据来自 frida 的 --interval（默认 50ms=20Hz），
# 想让叠加层真的到 60fps，也要把宿主的采样间隔一起调小（会增加游戏读内存开销）。
TICK_MS = 16
STALE_CLEAR_S = 1.2

_DPI_CTX_PER_MONITOR_V2 = -4
_dpi_state = None


def _u32():
    return ctypes.windll.user32


def system_dpi():
    try:
        return int(_u32().GetDpiForSystem())
    except Exception:
        return 0


def enable_dpi_awareness():
    """声明 DPI 感知；**必须**在创建任何窗口之前调用。幂等，返回说明串供日志用。"""
    global _dpi_state
    if _dpi_state is not None:
        return _dpi_state
    u = _u32()
    try:
        u.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
        u.SetProcessDpiAwarenessContext.restype = wintypes.BOOL
        if u.SetProcessDpiAwarenessContext(
                ctypes.c_void_p(_DPI_CTX_PER_MONITOR_V2 & ((1 << 64) - 1))):
            _dpi_state = f"per-monitor-v2 (系统 DPI {system_dpi()})"
            return _dpi_state
    except Exception:
        pass
    try:
        if ctypes.windll.shcore.SetProcessDpiAwareness(2) == 0:
            _dpi_state = f"per-monitor (系统 DPI {system_dpi()})"
            return _dpi_state
    except Exception:
        pass
    try:
        if u.SetProcessDPIAware():
            _dpi_state = f"system-aware (系统 DPI {system_dpi()})"
            return _dpi_state
    except Exception:
        pass
    _dpi_state = "unaware（窗口会被系统缩放，框会偏，请检查 Windows 兼容性设置）"
    return _dpi_state


# --------------------------------------------------------------------- 窗口探测
def _is_window(hwnd):
    try:
        return bool(_u32().IsWindow(hwnd))
    except Exception:
        return False


def client_rect(hwnd):
    """客户区的屏幕矩形（物理像素），返回 (x, y, w, h)；拿不到返回 None。"""
    try:
        u = _u32()
        r = wintypes.RECT()
        if not u.GetClientRect(hwnd, ctypes.byref(r)):
            return None
        p = wintypes.POINT(0, 0)
        u.ClientToScreen(hwnd, ctypes.byref(p))   # 客户区左上角 -> 屏幕坐标
        w, h = r.right - r.left, r.bottom - r.top
        if w <= 0 or h <= 0:
            return None
        return (int(p.x), int(p.y), int(w), int(h))
    except Exception:
        return None


def find_game_hwnd(pid=None, class_name=UNITY_WINDOW_CLASS):
    """按 pid 找游戏主窗口：Z 序里客户区最大的那个 UnityWndClass 顶层窗口。"""
    try:
        u = _u32()
    except Exception:
        return None
    try:
        u.GetTopWindow.restype = wintypes.HWND
        u.GetWindow.restype = wintypes.HWND
        u.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
        best, best_area = None, 0
        hwnd = u.GetTopWindow(None)
        guard = 0
        while hwnd and guard < 4000:
            guard += 1
            try:
                if u.IsWindowVisible(hwnd):
                    buf = ctypes.create_unicode_buffer(256)
                    u.GetClassNameW(hwnd, buf, 256)
                    if class_name is None or buf.value == class_name:
                        wpid = wintypes.DWORD()
                        u.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
                        if pid is None or wpid.value == int(pid):
                            r = client_rect(hwnd)
                            if r and r[2] * r[3] > best_area:
                                best, best_area = hwnd, r[2] * r[3]
            except Exception:
                pass
            hwnd = u.GetWindow(hwnd, GW_HWNDNEXT)
        return best
    except Exception:
        return None


def is_foreground(hwnd, ignore=()):
    """游戏窗口（或其根窗口）是否是前台窗口。

    ignore：要忽略的句柄（我们自己的叠加层）。叠加层是 topmost，偶尔会被系统
    当成前台窗口 —— 若不排除，`is_foreground(游戏)` 会瞬间变 False，
    于是隐藏自己 → 前台又回到游戏 → 再显示，形成周期性闪烁。
    """
    if not hwnd:
        return False
    try:
        u = _u32()
        fg = u.GetForegroundWindow()
        if not fg:
            return False
        ignore = {int(h) for h in ignore if h}
        if int(fg) in ignore:
            return True                 # 自己在前台 == 游戏仍是我们关注的焦点
        if fg == hwnd:
            return True
        u.GetAncestor.restype = wintypes.HWND
        u.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        root = u.GetAncestor(fg, GA_ROOT)
        if root and int(root) in ignore:
            return True
        return root == hwnd
    except Exception:
        return False


def is_minimized(hwnd):
    try:
        return bool(_u32().IsIconic(hwnd))
    except Exception:
        return False


def root_hwnd(hwnd):
    """取顶层窗口句柄（Tk 的 winfo_id 返回的是子 HWND，需要往上找）。"""
    if not hwnd:
        return None
    try:
        u = _u32()
        u.GetAncestor.restype = wintypes.HWND
        u.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        return u.GetAncestor(hwnd, GA_ROOT) or hwnd
    except Exception:
        return hwnd


def set_topmost(hwnd):
    """原地重申置顶：不动位置/尺寸、不激活、不发送重绘。返回是否成功。"""
    if not hwnd:
        return False
    try:
        u = _u32()
        u.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int,
                                   ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
        u.SetWindowPos.restype = wintypes.BOOL
        flags = (SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
                 | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING)
        return bool(u.SetWindowPos(wintypes.HWND(hwnd), wintypes.HWND(HWND_TOPMOST),
                                   0, 0, 0, 0, flags))
    except Exception:
        return False


def set_click_through(hwnd, no_activate=True):
    """让叠加层不接收鼠标、也永不获得焦点。

    Tk 的 `-transparentcolor` 只设了 LWA_COLORKEY：**只有纯黑区域**的点击会穿透，
    而我们画的框/文字是红蓝绿，那些像素照样会拦住鼠标 —— 打游戏时会发现
    偶尔点不动（点到框上了）。所以要补 WS_EX_TRANSPARENT：整窗不接收鼠标。

    WS_EX_NOACTIVATE 则从根上消除"叠加层抢前台"的可能，
    这正是周期性闪烁的成因之一。
    """
    if not hwnd:
        return False
    try:
        u = _u32()
        GWL_EXSTYLE, WS_EX_TRANSPARENT, WS_EX_LAYERED, WS_EX_NOACTIVATE = \
            -20, 0x00000020, 0x00080000, 0x08000000
        u.GetWindowLongW.restype = ctypes.c_long
        u.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
        u.SetWindowLongW.restype = ctypes.c_long
        u.SetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_long]
        ex = u.GetWindowLongW(wintypes.HWND(hwnd), GWL_EXSTYLE)
        want = ex | WS_EX_TRANSPARENT | WS_EX_LAYERED
        if no_activate:
            want |= WS_EX_NOACTIVATE
        if want == ex:
            return True
        u.SetWindowLongW(wintypes.HWND(hwnd), GWL_EXSTYLE, want)
        return True
    except Exception:
        return False


def game_client_rect(class_name=UNITY_WINDOW_CLASS, window_name=None, pid=None):
    """兼容旧接口：找游戏窗口并返回客户区矩形。"""
    return client_rect(find_game_hwnd(pid, class_name))


# --------------------------------------------------------------------- 坐标映射
def compute_viewport(client, frame_w, frame_h, fit="auto"):
    """把帧坐标系 (0..frame_w, 0..frame_h) 映射到屏幕客户区。

    返回 dict:
      x, y, w, h —— 显示区在屏幕上的矩形（同时也用作叠加层窗口 geometry）
      sx, sy     —— 帧坐标 -> 物理像素 的缩放
      mode       —— 实际采用的方式（stretch / letterbox）

    auto：宽高比一致（±2%）时按客户区拉伸；不一致时按 letterbox 等比居中 ——
    本游戏 800x600(4:3) 全屏到 3840x2160(16:9) 就是后一种，
    实测显示区 (480,0,2880,2160) scale 3.6，与截图黑边位置完全一致。
    """
    cx, cy, cw, ch = [int(v) for v in client]
    fw, fh = max(1, int(frame_w)), max(1, int(frame_h))
    mode = fit if fit in ("stretch", "letterbox") else "auto"
    if mode == "auto":
        same = abs((cw / ch) - (fw / fh)) / (fw / fh) <= 0.02
        mode = "stretch" if same else "letterbox"
    if mode == "letterbox":
        s = min(cw / fw, ch / fh)
        w, h = int(round(fw * s)), int(round(fh * s))
        x, y = cx + (cw - w) // 2, cy + (ch - h) // 2
        sx = sy = s
    else:
        x, y, w, h = cx, cy, cw, ch
        sx, sy = cw / fw, ch / fh
    return {"x": x, "y": y, "w": w, "h": h, "sx": sx, "sy": sy, "mode": mode}


def map_point(vp, x, y):
    """帧坐标 -> 叠加层画布坐标（画布原点就是显示区左上角）。"""
    return (x * vp["sx"], y * vp["sy"])


def to_screen(vp, x, y):
    """帧坐标 -> 屏幕物理像素坐标（排查用）。"""
    return (vp["x"] + x * vp["sx"], vp["y"] + y * vp["sy"])


def viewport_text(vp, frame_w=0, frame_h=0, client=None, dpi=""):
    """一行人类可读的对齐诊断，出问题时看这行就够了。"""
    if not vp:
        return "（尚未对齐）"
    s = (f"帧 {frame_w}x{frame_h} → 显示区 {vp['w']}x{vp['h']}@({vp['x']},{vp['y']}) "
         f"缩放 {vp['sx']:.2f}x [{vp['mode']}]")
    if client:
        s += f" | 客户区 {client[2]}x{client[3]}@({client[0]},{client[1]})"
    if dpi:
        s += f" | DPI {dpi}"
    return s


# --------------------------------------------------------------------- 叠加窗口
class OverlayWindow:
    def __init__(self, pid=None, fit="auto", rect=None, clip=True):
        self.pid = int(pid) if pid else None
        self.fit = fit if fit in FIT_MODES else "auto"
        self.clip = bool(clip)          # True = 只在游戏前台时显示
        self.manual_rect = tuple(int(v) for v in rect) if rect else None

        self.hwnd = None
        self.client = None
        self.frame = (800, 600)
        self.vp = None

        self._own_hwnd = None
        self._hide_at = None            # 失焦后延迟隐藏的截止时间（迟滞）
        self._last_draw = 0.0           # 最近一次收到帧的时间（供 _tick 判断流）
        self._q = queue.Queue()         # 宿主消息线程 -> Tk 主线程的帧投递队列
        self._clear_req = False
        self._next_probe = 0.0
        self._next_topmost = 0.0
        self._shown = None
        self._warned = {}
        self._pool = []
        self._sig = None
        # 样式缓存：canvas item 的 id 稳定，按 id 存上一次 itemconfigure 的参数。
        # 位置每帧都在变（必须 coords），但颜色/线宽/字号常常不变 —— 跳过无变化的
        # itemconfigure 能省掉大量 Tcl 往返，这是 tkinter 叠加层的主要开销来源。
        self._style_cache = {}
        self._status_printed = False
        # 叠加层主循环帧率（仅诊断用，便于确认 TICK_MS 改动确实生效）
        self._frames = 0
        self._fps_t = time.time()
        self._fps = 0.0

        enable_dpi_awareness()          # 必须在 tk.Tk() 之前
        self.root = tk.Tk()
        self.root.overrideredirect(True)                      # 去边框
        self.root.attributes("-topmost", True)                # 置顶
        self.root.attributes("-transparentcolor", "black")    # 黑色透明 + 点击穿透
        self.root.geometry("1x1+0+0")
        self.canvas = tk.Canvas(self.root, bg="black", width=1, height=1,
                                highlightthickness=0)
        self.canvas.pack()
        self.root.withdraw()            # 拿到第一帧再显示，避免左上角闪一下
        self.root.update()
        try:
            self._own_hwnd = root_hwnd(self.root.winfo_id())
        except Exception:
            self._own_hwnd = None

    # ------------------------------------------------------------ 窗口探测/跟随
    def _probe(self, force=False):
        now = time.time()
        if not force and now < self._next_probe:
            return
        self._next_probe = now + PROBE_INTERVAL_S

        if self.manual_rect is not None:
            # 显示区由用户指定，但**仍然**去找游戏窗口：
            # 前台判断（游戏不在前台就隐藏）靠它，不然回到桌面也会一直画。
            self.client = self.manual_rect
            if not _is_window(self.hwnd):
                self.hwnd = find_game_hwnd(self.pid)
        else:
            if not _is_window(self.hwnd):
                self.hwnd = find_game_hwnd(self.pid)
                if self.hwnd is None and not self._warned.get("hwnd"):
                    self._warned["hwnd"] = True
                    print("[overlay] 没找到 Unity 游戏窗口（按 1:1 退化绘制）；"
                          "可用 --overlay-rect x,y,w,h 手动指定显示区")
            r = client_rect(self.hwnd) if self.hwnd else None
            if r:
                self.client = r

    def _should_show(self):
        if not self.clip:
            return True
        if not self.hwnd:
            return True                 # 找不到窗口时照画，别让用户对着空屏发呆
        if is_minimized(self.hwnd) or not _u32().IsWindowVisible(self.hwnd):
            return False
        # ignore 里放的是叠加层自己的句柄：它偶尔会成为前台窗口，
        # 不排除的话会自己把自己判出局，产生周期性闪烁。
        return is_foreground(self.hwnd, ignore=(self._own_hwnd,))

    def _set_shown(self, show):
        if self._shown == show:
            return
        self._shown = show
        try:
            if show:
                self.root.deiconify()
                # 整窗不接收鼠标、永不获得焦点（否则会挡住游戏里的点击）
                set_click_through(self._own_hwnd or self.root.winfo_id())
                # 用原生 SetWindowPos 置顶：不抢焦点、不触发重绘
                if not set_topmost(self._own_hwnd or self.root.winfo_id()):
                    self.root.attributes("-topmost", True)
            else:
                self.root.withdraw()
        except Exception as e:
            if not self._warned.get("showhide"):
                self._warned["showhide"] = True
                print("[overlay] 显示/隐藏窗口失败（后续不再打印）:", e)

    def _apply_geometry(self):
        vp = self.vp
        try:
            self.root.geometry(f"{vp['w']}x{vp['h']}+{vp['x']}+{vp['y']}")
            self.canvas.config(width=vp["w"], height=vp["h"])
            self.root.update_idletasks()
        except Exception as e:
            if not self._warned.get("geom"):
                self._warned["geom"] = True
                print("[overlay] 设置窗口几何失败（后续不再打印）:", e)

    # ------------------------------------------------------------ 绘制
    def _slot(self, i):
        while len(self._pool) <= i:
            self._pool.append((
                self.canvas.create_rectangle(0, 0, 0, 0, outline="#fff", width=2,
                                             state="hidden"),
                self.canvas.create_rectangle(0, 0, 0, 0, outline="", fill="#333",
                                             state="hidden"),
                self.canvas.create_rectangle(0, 0, 0, 0, outline="", fill="#fff",
                                             state="hidden"),
                self.canvas.create_text(0, 0, text="", fill="#fff", anchor="s",
                                        font=("monospace", BASE_FONT), state="hidden"),
            ))
        return self._pool[i]

    def clear(self):
        """清掉所有标记（不在对局 / 帧断流时调用，避免留下残影）。"""
        if self._sig == ():
            return
        for ids in self._pool:
            for it in ids:
                self.canvas.itemconfigure(it, state="hidden")
        self._sig = ()
        self._style_cache.clear()      # 下次重画时强制重新 itemconfigure
        try:
            self.root.update_idletasks()
        except Exception:
            pass

    def _cfg(self, item, **kw):
        """带缓存的 itemconfigure：参数和上次一样就跳过，省 Tcl 往返。"""
        iid = int(item)
        prev = self._style_cache.get(iid)
        if prev == kw:
            return
        self._style_cache[iid] = dict(kw)
        self.canvas.itemconfigure(item, **kw)

    def _draw(self, marks):
        vp = self.vp
        drawable = [m for m in marks
                    if m.on_screen and m.screen is not None and not m.is_dead]
        # 远的先画、近的后画（后画的盖在上面），本地玩家永远最上
        drawable.sort(key=lambda m: (m.kind == "local", -(m.dist or 0.0)))

        sig = tuple((round(m.screen[0], 1), round(m.screen[1], 1), m.kind,
                     m.hp, m.max_hp) for m in drawable)
        if sig == self._sig:
            return                  # 没有变化就不重绘 —— 闪烁的主要来源之一
        self._sig = sig

        sx, sy = vp["sx"], vp["sy"]
        scale = (sx + sy) / 2.0
        bw, bh = max(6.0, BOX_W * sx), max(9.0, BOX_H * sy)
        lw = max(1, min(6, int(round(2 * scale))))
        bar_h = max(2.0, 4 * sy)
        fsz = int(max(7, min(40, round(BASE_FONT * scale))))

        for idx, mk in enumerate(drawable):
            cxp, cyp = map_point(vp, mk.screen[0], mk.screen[1])
            color = COLOR.get(mk.kind, "#ffffff")
            bx, by = cxp - bw / 2.0, cyp - bh / 2.0
            box, hpbg, hpbar, txt = self._slot(idx)
            # 位置每帧都变 -> coords 必调；样式常不变 -> _cfg 会跳过无变化的 itemconfigure
            self._cfg(box, state="normal", outline=color, width=lw)
            self.canvas.coords(box, bx, by, bx + bw, by + bh)

            if mk.hp is not None and mk.max_hp:
                ratio = max(0.0, min(1.0, float(mk.hp) / float(mk.max_hp)))
                self._cfg(hpbg, state="normal")
                self.canvas.coords(hpbg, bx, by - bar_h * 2, bx + bw, by - bar_h)
                self._cfg(hpbar, state="normal", fill=color)
                self.canvas.coords(hpbar, bx, by - bar_h * 2, bx + bw * ratio, by - bar_h)
            else:
                self._cfg(hpbg, state="hidden")
                self._cfg(hpbar, state="hidden")

            label = f"{mk.kind} t{mk.team}"
            if mk.hp is not None:
                label += f" {mk.hp}/{mk.max_hp}"
            if mk.dist is not None:
                label += f" {mk.dist:.0f}m"
            self._cfg(txt, state="normal", fill=color,
                      font=("monospace", fsz), text=label)
            self.canvas.coords(txt, cxp, by - bar_h * 2.6)

        for idx in range(len(drawable), len(self._pool)):
            for it in self._pool[idx]:
                self._cfg(it, state="hidden")

        try:
            self.root.update_idletasks()
        except Exception:
            pass

    # ------------------------------------------------------------ 每帧入口
    def update(self, marks, frame_w=None, frame_h=None):
        fw = int(frame_w or self.frame[0])
        fh = int(frame_h or self.frame[1])
        if (fw, fh) != self.frame:
            self.frame = (fw, fh)
            self.vp = None

        self._probe()
        now = time.time()
        self._last_draw = now          # 有帧就算活着（_tick 靠它判断断流）
        if self._shown and now >= self._next_topmost:
            self._next_topmost = now + TOPMAST_REASSERT_S
            # 游戏（Unity 全屏）也会抢 topmost，要定期重申；但必须用**不激活**
            # 的原生调用 —— Tk 的 lift()/attributes("-topmost") 会闪一下，
            # 还会把前台焦点抢过来，触发上面的 _should_show 反向判断。
            if not set_topmost(self._own_hwnd or self.root.winfo_id()):
                try:
                    self.root.attributes("-topmost", True)
                except Exception:
                    pass

        if not self._should_show():
            # 迟滞：短暂失焦（弹设置菜单、系统通知抢焦点）先别急着藏，
            # 否则会「藏-显-藏」高频抖动，看起来就是闪烁。
            if self._shown:
                if self._hide_at is None:
                    self._hide_at = now + HIDE_DELAY_S
                elif now >= self._hide_at:
                    self._hide_at = None
                    self._set_shown(False)
            return
        self._hide_at = None
        self._set_shown(True)

        client = self.client or (0, 0, self.frame[0], self.frame[1])
        vp = compute_viewport(client, self.frame[0], self.frame[1], self.fit)
        if self.vp != vp:
            self.vp = vp
            self._apply_geometry()
            self._sig = None
        self._draw(marks)

    # ------------------------------------------------------------ 跨线程入口
    # tkinter **只保证创建它的线程可用**：宿主的 frida 消息回调跑在别的线程上，
    # 直接在里面调 canvas.* / geometry() 属于未定义行为（可能偶发崩溃、
    # 也可能表现为画面撕裂/闪烁）。所以宿主只把数据丢进队列，
    # 真正的 Tk 操作统一放到主线程的 _tick 里做。
    def push(self, marks, frame_w=None, frame_h=None):
        """（任意线程）投递一帧；积压时丢弃旧帧，只保留最新。"""
        item = (marks, frame_w, frame_h)
        try:
            self._q.put_nowait(item)
        except queue.Full:
            try:
                self._q.get_nowait()
                self._q.put_nowait(item)
            except Exception:
                pass

    def request_clear(self):
        """（任意线程）请求清屏；实际清屏在 Tk 主线程执行。"""
        self._clear_req = True

    def _drain(self):
        """（Tk 主线程）取出最新一帧并绘制。"""
        latest = None
        while True:
            try:
                latest = self._q.get_nowait()
            except queue.Empty:
                break
        if self._clear_req:
            self._clear_req = False
            self.clear()
        if latest is not None:
            # 收到帧就先记时：即便 update 因"还没拿到客户区"提前返回，
            # 也算链路是活的，不该被 _tick 判成断流清屏。
            self._last_draw = time.time()
            self.update(*latest)

    # ------------------------------------------------------------ 其它
    def _tick(self):
        """Tk 主线程上的定时器：消费帧队列 + 帧断流清屏 + 重申置顶。

        **所有 Tk 调用都必须在这里做**。宿主的 frida 消息回调与看门狗都在
        别的线程上，跨线程碰 canvas 不是线程安全的（以前确实这么干过）。
        """
        now = time.time()
        self._frames += 1
        if now - self._fps_t >= 1.0:
            self._fps = self._frames / (now - self._fps_t)
            self._frames = 0
            self._fps_t = now
        self._drain()
        if self._last_draw and now - self._last_draw > STALE_CLEAR_S:
            self.clear()        # 帧断流（回菜单 / agent 卡住）-> 抹掉残影
        if self._shown and now >= self._next_topmost:
            self._next_topmost = now + TOPMAST_REASSERT_S
            if not set_topmost(self._own_hwnd or self.root.winfo_id()):
                try:
                    self.root.attributes("-topmost", True)
                except Exception:
                    pass
        try:
            self.root.after(TICK_MS, self._tick)
        except Exception:
            pass

    def status(self):
        return (viewport_text(self.vp, self.frame[0], self.frame[1],
                              self.client, _dpi_state or "未设置")
                + f" | 重绘 {self._fps:.0f}Hz")

    def mainloop(self):
        self._tick()            # 起来就跑，负责帧断流清屏 + 定期重申置顶
        self.root.mainloop()
