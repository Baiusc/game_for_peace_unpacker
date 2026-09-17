#!/usr/bin/env python3
# UCF-ESP 实时叠加层骨架（Windows, tkinter）。
#
# 真实运行时由 frida_host 调用：每收到一帧就用 OverlayWindow.update(marks) 重绘。
# 这是一个"透明 + 置顶 + 点击穿透"的窗口，上面只画玩家框/血条，不拦截鼠标。
#
# 注意：
#  - tkinter 在 Windows 自带，无需额外安装。
#  - 沙箱无显示器，不会实例化；本文件仅在被 frida_host import 时按需加载。
#  - 透明靠 -transparentcolor "black"：画布底色设为 black 即被抠掉，
#    只有 stroke/fill 的彩色线条可见。

import tkinter as tk

COLOR = {
    "local": "#39d353",
    "teammate": "#58a6ff",
    "enemy": "#f85149",
}

# Unity 桌面端主窗口的固定窗口类名（UnityWndClass），用它比遍历窗口可靠得多。
UNITY_WINDOW_CLASS = "UnityWndClass"


def game_client_rect(class_name=UNITY_WINDOW_CLASS, window_name=None):
    """找游戏主窗口客户区的屏幕矩形，返回 (x, y, w, h)；拿不到就返回 None。

    为什么不用 EnumWindows + 回调：ctypes 回调在 64 位 Python 下极易
    OverflowError（本项目已在低级键盘钩子上栽过一次），FindWindowW 无回调更安全。
    """
    try:
        import ctypes
        from ctypes import wintypes
    except Exception:
        return None
    try:
        user32 = ctypes.windll.user32
        user32.FindWindowW.restype = wintypes.HWND
        user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
        hwnd = user32.FindWindowW(class_name, window_name)
        if not hwnd:
            return None
        r = wintypes.RECT()
        if not user32.GetClientRect(hwnd, ctypes.byref(r)):
            return None
        p = wintypes.POINT(0, 0)
        user32.ClientToScreen(hwnd, ctypes.byref(p))   # 客户区左上角 -> 屏幕坐标
        w, h = r.right - r.left, r.bottom - r.top
        if w <= 0 or h <= 0:
            return None
        return (int(p.x), int(p.y), int(w), int(h))
    except Exception:
        return None


class OverlayWindow:
    def __init__(self, w=1280, h=720, align=True):
        self.w, self.h = int(w), int(h)
        self.anchor = (0, 0)          # 游戏窗口客户区左上角（拿不到就退化到 0,0）
        self.anchor_locked = False    # 已按帧分辨率确认过锚点，不再改
        self.applied = False          # geometry 是否已按真实尺寸落过一次
        self.root = tk.Tk()
        self.root.overrideredirect(True)                      # 去边框
        self.root.attributes("-topmost", True)                # 置顶
        self.root.attributes("-transparentcolor", "black")    # 黑色透明
        self.root.geometry(f"{self.w}x{self.h}+0+0")
        self.canvas = tk.Canvas(self.root, bg="black", width=self.w, height=self.h,
                                highlightthickness=0)
        self.canvas.pack()
        self.root.update()
        if align:
            self._pending_rect = game_client_rect()
        else:
            self._pending_rect = None

    # ---------------------------------------------------------------- 尺寸/位置
    def _resolve_anchor(self, w, h):
        """确认游戏窗口锚点：只有当客户区尺寸与帧分辨率一致时才采信它的位置。

        为什么必须核对尺寸：FindWindowW 只返回 Z 序最上面的一个 UnityWndClass 窗口，
        系统里可能有别的 Unity 窗口（实测抓到过 320x240 的），直接拿它的位置会错位。
        """
        if self.anchor_locked or not getattr(self, "_pending_rect", None):
            return
        rx, ry, rw, rh = self._pending_rect
        # 只认尺寸严格相等的窗口。
        # 不要退化成“宽高比一致”：实测抓到过无关的 320x240 Unity 窗口，
        # 它和 800x600 的宽高比都是 1.333，靠比例判断会采信错误的锚点。
        if abs(rw - w) <= 2 and abs(rh - h) <= 2:
            self.anchor = (rx, ry)
            self.anchor_locked = True

    def fit(self, w=None, h=None):
        """每帧调用。画布尺寸**必须**等于帧分辨率，否则投影坐标对不上。

        注意方向别搞反：投影用的是 frame 的 width/height，所以画布尺寸必须跟它一致；
        游戏窗口客户区只用来提供**位置**（游戏实测 800x600，硬编码 1280x720 会放大 1.6 倍）。
        """
        w = int(w or self.w)
        h = int(h or self.h)
        self._resolve_anchor(w, h)
        if self.applied and (w, h) == (self.w, self.h):
            return False
        self.w, self.h = w, h
        self.root.geometry(f"{w}x{h}+{self.anchor[0]}+{self.anchor[1]}")
        self.canvas.config(width=w, height=h)
        self.applied = True
        self.root.update()
        return True

    # ---------------------------------------------------------------- 绘制

    def update(self, marks):
        """marks: esp_core.Mark 列表。只画屏幕内、未死亡的实体。"""
        self.canvas.delete("all")
        for mk in marks:
            if not mk.on_screen or mk.screen is None or mk.is_dead:
                continue
            x, y = mk.screen
            color = COLOR.get(mk.kind, "#ffffff")
            bw, bh = 40, 60
            bx, by = x - bw / 2, y - bh / 2
            self.canvas.create_rectangle(bx, by, bx + bw, by + bh,
                                         outline=color, width=2)
            if mk.hp is not None and mk.max_hp:
                ratio = max(0.0, min(1.0, mk.hp / mk.max_hp))
                self.canvas.create_rectangle(bx, by - 8, bx + bw, by - 4,
                                             outline="", fill="#333")
                self.canvas.create_rectangle(bx, by - 8, bx + bw * ratio, by - 4,
                                             outline="", fill=color)
            label = f"{mk.kind} t{mk.team}"
            if mk.hp is not None:
                label += f" {mk.hp}/{mk.max_hp}"
            self.canvas.create_text(x, by - 12, text=label, fill=color,
                                    font=("monospace", 11), anchor="s")
        self.root.update_idletasks()
        self.root.update()

    def mainloop(self):
        self.root.mainloop()
