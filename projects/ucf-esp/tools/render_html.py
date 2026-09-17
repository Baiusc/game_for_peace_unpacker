#!/usr/bin/env python3
# UCF-ESP 离线预览：把投影后的 marks 渲染成静态 SVG 文件。
# 纯标准库，沙箱/本机均可运行，便于在没有游戏进程时核对投影结果。

import html

COLOR = {
    "local": "#39d353",     # 绿：本地玩家
    "teammate": "#58a6ff",  # 蓝：队友
    "enemy": "#f85149",     # 红：敌方
}


def render_preview(marks, w, h, out_path, vp=None):
    """把 marks 画到 SVG，并写出到 out_path。"""
    parts = []
    parts.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" font-family="monospace">'
    )
    parts.append(f'<rect width="{w}" height="{h}" fill="#0b0e14"/>')
    # 屏幕中心十字
    parts.append(f'<line x1="{w/2}" y1="0" x2="{w/2}" y2="{h}" stroke="#1c2333" stroke-width="1"/>')
    parts.append(f'<line x1="0" y1="{h/2}" x2="{w}" y2="{h/2}" stroke="#1c2333" stroke-width="1"/>')

    drawn = 0
    for mk in marks:
        if mk.screen is None or not mk.on_screen:
            continue
        x, y = mk.screen
        color = COLOR.get(mk.kind, "#ffffff")
        bw, bh = 40, 60
        bx, by = x - bw / 2, y - bh / 2
        parts.append(
            f'<rect x="{bx:.1f}" y="{by:.1f}" width="{bw}" height="{bh}" '
            f'fill="none" stroke="{color}" stroke-width="2"/>'
        )
        if mk.hp is not None and mk.max_hp:
            ratio = max(0.0, min(1.0, mk.hp / mk.max_hp))
            parts.append(f'<rect x="{bx:.1f}" y="{by-8:.1f}" width="{bw}" height="4" fill="#333"/>')
            parts.append(f'<rect x="{bx:.1f}" y="{by-8:.1f}" width="{bw*ratio:.1f}" height="4" fill="{color}"/>')
        label = f"{mk.kind} t{mk.team}"
        if mk.hp is not None:
            label += f" {mk.hp}/{mk.max_hp}"
        if mk.is_dead:
            label += " DEAD"
        parts.append(
            f'<text x="{x:.1f}" y="{by-12:.1f}" fill="{color}" font-size="11" '
            f'text-anchor="middle">{html.escape(label)}</text>'
        )
        drawn += 1

    parts.append("</svg>")
    svg = "\n".join(parts)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n' + svg)
    return drawn
