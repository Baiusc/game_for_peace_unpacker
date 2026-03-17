#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
auto_skin_final.py
功能：
 1. 直接处理指定 dat 文件
 2. 先做一次 swap 美化
 3. 再批量替换 ID → 405017
 4. 打印实时进度
"""

import re
import mmap
from pathlib import Path
from typing import List, Tuple

# 天线美化。旧版 BattleItem
SWAP_CONFIG_OLD = [
    (404007, 413739),   # 竞技裤(黑)-创建角色已占用 <=> 幽焰骑士2 413619  
    (405011, 413740),   # 棕色高帮运动鞋-创建角色已占用 <=> 幽焰骑士3 413740
    (812018, 413818),    # 默认鞋子(女) ↔  瑞拉1
    (812019, 413819),    # 默认鞋子(男) ↔ 瑞拉2
    (812020, 413820),    # 默认鞋子(通用) ↔ 瑞拉3 
    (405001, 413655),    # 运动鞋(白) ↔ 宇宙1 金蛇镇世-烛九3
    (404110, 413656),     # 牛仔裤(蓝) ↔ 宇宙2 初号机款机体服
    (403251, 413657)    # T恤(白)(新)(创建角色使用) <=> 宇宙3
]

# 天线美化。防弹衣和背包挂件  AvatarBPTable
SWAP_CONFIG_ARMOR = [
    (802397, 423054),    # 背包挂件-扫描仪 802397 <=> 暮色玫影3 423054
    (503001, 413619),    # 1级甲 <=> 幽焰骑士1 413619
    (503002, 413497),    # 2级甲 <=> 赵云2 413497
    (503003, 413498)     # 3级甲 <=> 赵云3 413498
]

# 天线美化。衣服 裤子 鞋子   BattleItem
SWAP_CONFIG_CLOTH = [
    (404007, 413739),   # 竞技裤(黑)-创建角色已占用 <=> 幽焰骑士2 413739  
    (405011, 413740),   # 棕色高帮运动鞋-创建角色已占用 <=> 幽焰骑士3 413740
    (403251, 413657)     # T恤(白)(新)(创建角色使用) <=> 宇宙3
]

# ============ 工具函数 ============

def swap_in_dat(dat_path: Path, swaps: List[Tuple[int, int]]):
    """
    在 dat 文件中进行 ID 对调（swap）
    """
    print(f"[STEP] 开始 swap 操作 -> {dat_path}")
    with dat_path.open("r+b") as fp:
        data = fp.read()
        for old_val, new_val in swaps:
            old_str = str(old_val).encode()
            new_str = str(new_val).encode()
            # 匹配前后固定长度 只匹配第一个搜索结果
            pattern_old = rb'(.{8})' + re.escape(old_str) + rb'(.{10})'
            pattern_new = rb'(.{8})' + re.escape(new_str) + rb'(.{10})'

            # DEBUG 找到所有匹配
            matches_old = list(re.finditer(pattern_old, data, flags=re.DOTALL))
            matches_new = list(re.finditer(pattern_new, data, flags=re.DOTALL))
            # DEBUG 打印所有匹配上下文
            print(f"  [INFO] old ID {old_val} 匹配块:")
            for m in matches_old:
                print(f"    offset={m.start():06}  block={to_hex(m.group(0))}")

            print(f"  [INFO] new ID {new_val} 匹配块:")
            for m in matches_new:
                print(f"    offset={m.start():06}  block={to_hex(m.group(0))}")

            # 搜索块
            m_old = re.search(pattern_old, data)
            m_new = re.search(pattern_new, data)
            if not m_old or not m_new:
                print(f"  [WARN] 未找到 ID {old_val} 或 {new_val} 的数据块")
                continue
            block_old = m_old.group(0)
            block_new = m_new.group(0)
            data = data.replace(block_old, b"__TMP__")
            data = data.replace(block_new, block_old)
            data = data.replace(b"__TMP__", block_new)
            print(f"  [OK] 已交换 ID {old_val} ↔ {new_val}")
        fp.seek(0)
        fp.write(data)
        fp.truncate()
    print("[DONE] swap 完成\n")

def to_hex(b: bytes) -> str:
    """字节转十六进制字符串，每字节两位，用空格分隔"""
    return " ".join(f"{x:02X}" for x in b)
def patch_ids(dat_path: Path, replacements: List[Tuple[str, str]]):
    """
    批量替换 dat 文件中的 ID，并加固定上下文字节（前3字节00，后1字节00）
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    print(f"[STEP] 开始批量替换 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_str, new_str in replacements:
                # 转成字节
                old_id_bytes = str(old_str).encode("utf-8")
                new_id_bytes = str(new_str).encode("utf-8")

                # 上下文字节
                prefix = b"\x00\x00\x00"
                suffix = b"\x00"

                # 整个匹配块
                old_bytes = prefix + old_id_bytes + suffix
                new_bytes = prefix + new_id_bytes + suffix

                if len(old_bytes) != len(new_bytes):
                    print(f"  [WARN] 跳过长度不同的替换 {old_str} → {new_str}")
                    continue

                pos = 0
                replaced = 0
                while True:
                    pos = mm.find(old_bytes, pos)
                    if pos == -1:
                        break

                    # 打印上下文：前12字节+ID+后12字节
                    start = max(pos - 12, 0)
                    end = min(pos + len(old_bytes) + 12, mm.size())
                    context_bytes = mm[start:end]
                    # print(f"  [MATCH] offset={pos:06} | 12+ID+12: {to_hex(context_bytes)}")

                    # 替换
                    mm[pos:pos+len(old_bytes)] = new_bytes
                    replaced += 1
                    pos += len(old_bytes)

                if replaced > 1:
                    print(f"  [OK] {old_str} ({to_hex(old_bytes)}) → {new_str} ({to_hex(new_bytes)}) 替换 {replaced} 次")
        finally:
            mm.close()
    print("[DONE] 批量替换完成\n")

def patch_ptrs_by_ids(dat_path: Path, replacements: List[Tuple[str, str]]):
    """
    批量替换 dat 文件中的指针：
    根据 (old_id, new_id) 配置，将 old_id 的指针替换为 new_id 的指针
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    # 上下文字节
    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始批量替换指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in replacements:
                old_id_bytes = old_id.encode("utf-8")
                new_id_bytes = new_id.encode("utf-8")

                # 构造块
                old_block = prefix + old_id_bytes + suffix
                new_block = prefix + new_id_bytes + suffix

                # 先找到 new_id 的指针
                pos_new = mm.find(new_block)
                if pos_new == -1:
                    print(f"  [WARN] 未找到 new_id={new_id}")
                    continue

                new_ptr_start = pos_new + len(new_block)
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                print(f"  [INFO] new_id={new_id} 指针={to_hex(new_ptr)} (offset={new_ptr_start})")

                # 替换 old_id 的所有指针
                pos = 0
                replaced = 0
                while True:
                    pos = mm.find(old_block, pos)
                    if pos == -1:
                        break

                    old_ptr_start = pos + len(old_block)
                    old_ptr = mm[old_ptr_start:old_ptr_start+5]

                    # 打印上下文
                    start = max(pos - 12, 0)
                    end = min(old_ptr_start + 5 + 12, mm.size())
                    context_bytes = mm[start:end]
                    print(f"  [MATCH] old_id={old_id} offset={pos:06} | {to_hex(context_bytes)}")

                    # 替换指针
                    mm[old_ptr_start:old_ptr_start+5] = new_ptr
                    replaced += 1
                    pos = old_ptr_start + 5

                if replaced > 0:
                    print(f"  [OK] old_id={old_id} ({to_hex(old_ptr)}) → new_id={new_id} ({to_hex(new_ptr)}) 替换 {replaced} 次")
        finally:
            mm.close()
    print("[DONE] 批量指针替换完成\n")
def swap_id_and_ptr_in_dat(dat_path: Path, swaps: List[Tuple[str, str]], need_print: bool = False):
    """
    在 dat 文件中互换 old_id 和 new_id 的 ID 与指针
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始互换 ID+指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in swaps:
                old_block = prefix + old_id.encode("utf-8") + suffix
                new_block = prefix + new_id.encode("utf-8") + suffix

                pos_old = mm.find(old_block)
                pos_new = mm.find(new_block)
                if pos_old == -1 or pos_new == -1:
                    print(f"  [WARN] 未找到 old_id={old_id} 或 new_id={new_id}")
                    continue

                old_ptr_start = pos_old + len(old_block)
                new_ptr_start = pos_new + len(new_block)
                old_ptr = mm[old_ptr_start:old_ptr_start+5]
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                # 构造完整块（ID+指针）
                block_old = old_block + old_ptr
                block_new = new_block + new_ptr

                # 长度必须相等才能交换
                if len(block_old) != len(block_new):
                    print(f"  [WARN] old_id={old_id}, new_id={new_id} 块长度不同，跳过")
                    continue
                
                # --- 新增的上下文打印逻辑 ---
                if need_print:
                    # 定义上下文字节数
                    context_bytes = 24
                    # 打印 old_id 块的上下文：前12字节+ID+指针+后12字节
                    start_old = max(pos_old - context_bytes, 0)
                    end_old = min(pos_old + len(block_old) + context_bytes, mm.size())
                    context_old = mm[start_old:end_old]
                    print(f"  [OLD_CTX] offset={pos_old:06} | 12+ID+PTR+12: {to_hex(context_old)}")

                    # 打印 new_id 块的上下文：前12字节+ID+指针+后12字节
                    start_new = max(pos_new - context_bytes, 0)
                    end_new = min(pos_new + len(block_new) + context_bytes, mm.size())
                    context_new = mm[start_new:end_new]
                    print(f"  [NEW_CTX] offset={pos_new:06} | 12+ID+PTR+12: {to_hex(context_new)}")
                # --------------------------

                # 逐字节互换
                mm[pos_old:pos_old+len(block_old)] = block_new
                mm[pos_new:pos_new+len(block_new)] = block_old

                print(f"  [OK] 已互换 old_id={old_id}, new_id={new_id} (包含指针)")
        finally:
            mm.close()
    print("[DONE] 互换 ID+指针 完成\n")

def swap_ptr_in_dat(dat_path: Path, swaps: List[Tuple[str, str]]):
    """
    在 dat 文件中互换 old_id 和 new_id 的指针（仅交换指针，不动 ID）
    """
    def to_hex(b: bytes) -> str:
        return " ".join(f"{x:02X}" for x in b)

    prefix = b"\x00\x00\x00"
    suffix = b"\x00"

    print(f"[STEP] 开始互换指针 -> {dat_path}")
    with dat_path.open("r+b") as f:
        mm = mmap.mmap(f.fileno(), 0)
        try:
            for old_id, new_id in swaps:
                old_block = prefix + old_id.encode("utf-8") + suffix
                new_block = prefix + new_id.encode("utf-8") + suffix

                pos_old = mm.find(old_block)
                pos_new = mm.find(new_block)
                if pos_old == -1 or pos_new == -1:
                    print(f"  [WARN] 未找到 old_id={old_id} 或 new_id={new_id}")
                    continue

                old_ptr_start = pos_old + len(old_block)
                new_ptr_start = pos_new + len(new_block)
                old_ptr = mm[old_ptr_start:old_ptr_start+5]
                new_ptr = mm[new_ptr_start:new_ptr_start+5]

                if len(old_ptr) != len(new_ptr):
                    print(f"  [WARN] old_id={old_id}, new_id={new_id} 指针长度不同，跳过")
                    continue

                # 交换指针
                mm[old_ptr_start:old_ptr_start+5] = new_ptr
                mm[new_ptr_start:new_ptr_start+5] = old_ptr

                print(f"  [OK] 已互换 old_id={old_id}, new_id={new_id} 的指针")
        finally:
            mm.close()
    print("[DONE] 互换指针完成\n")

# ============ 主流程 ============
def main():
    is_need_print = True # 是否需要打印上下文

    # 直接指定要处理的 dat 文件
    path_BattleItem = Path("./release/RE天线V4发布20250930/BattleItem.uasset") #  BattleItem 衣服 裤子 鞋子 
    SWAP_CONFIG_CLOTH_str = [(str(old), str(new)) for old, new in SWAP_CONFIG_CLOTH] 
    swap_id_and_ptr_in_dat(path_BattleItem, SWAP_CONFIG_CLOTH_str, is_need_print) 

    path_AvatarBPTable = Path("./release/RE天线V4发布20250930/AvatarBPTable.uasset") # AvatarBPTable 防弹衣和背包挂件
    SWAP_CONFIG_ARMOR_str = [(str(old), str(new)) for old, new in SWAP_CONFIG_ARMOR] 
    swap_id_and_ptr_in_dat(path_AvatarBPTable, SWAP_CONFIG_ARMOR_str, is_need_print) 

    print("[ALL DONE] 处理完成！")

if __name__ == "__main__":
    main()
