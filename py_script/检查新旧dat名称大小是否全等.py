'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-23 08:54:54
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-09-23 08:54:55
FilePath     : /game_for_peace_unpacker/py_script/比较dat字节大小.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
from typing import Dict, List, Tuple

def build_size_index(src_dir: str) -> Dict[int, List[str]]:
    """
    扫描 src_dir，建立 {文件大小: [文件路径, ...]} 的索引
    """
    size_map: Dict[int, List[str]] = {}
    for root, _, files in os.walk(src_dir):
        for f in files:
            if not f.endswith('.dat'):
                continue
            path = os.path.join(root, f)
            try:
                sz = os.path.getsize(path)
            except OSError as e:
                print(f"无法读取文件大小: {path} -> {e}")
                continue
            size_map.setdefault(sz, []).append(path)
    return size_map

def list_my_files(my_dir: str) -> List[Tuple[str, str, int]]:
    """
    列出 my_dir 中所有 .dat 文件：返回 (文件名, 路径, 大小)
    """
    results: List[Tuple[str, str, int]] = []
    for root, _, files in os.walk(my_dir):
        for f in files:
            if not f.endswith('.dat'):
                continue
            path = os.path.join(root, f)
            try:
                sz = os.path.getsize(path)
            except OSError as e:
                print(f"无法读取文件大小: {path} -> {e}")
                continue
            results.append((f, path, sz))
    return results

def check_my_files(my_dir: str, src_dir: str, max_show_matches: int = 200):
    """
    对 my_dir 中的每个 .dat，检查 src_dir 中是否存在相同字节大小的文件。
    输出分组结果并统计匹配/未匹配数。
    max_show_matches: 若单个大小在 src_dir 中匹配数过多，限制打印数量（防止刷屏）。
    """
    if not os.path.isdir(my_dir):
        print(f"错误: my_dir 不存在: {my_dir}")
        return
    if not os.path.isdir(src_dir):
        print(f"错误: src_dir 不存在: {src_dir}")
        return

    print(f"正在扫描 src_dir 并建立大小索引（可能需要一点时间，如果文件很多）...")
    src_index = build_size_index(src_dir)
    my_files = list_my_files(my_dir)

    if not my_files:
        print("my_dir 中没有发现 .dat 文件。")
        return

    matched_count = 0
    unmatched_count = 0

    print("\n" + "="*80)
    print("检测结果（按 my_dir 中的每个文件分组）")
    print("="*80)

    for name, path, size in my_files:
        print(f"\n--- my 文件: {path}  ({size} 字节) ---")
        matches = src_index.get(size)
        if matches:
            matched_count += 1
            total = len(matches)
            print(f"在 src_dir 中找到 {total} 个大小相等的 .dat 文件：")
            # 防止输出过多：限制为 max_show_matches 条显示
            to_show = matches if total <= max_show_matches else matches[:max_show_matches]
            for m in to_show:
                print(f"  {m}  ({size} 字节)")
            if total > max_show_matches:
                print(f"  ... 其余 {total - max_show_matches} 个未显示")
        else:
            unmatched_count += 1
            print("在 src_dir 中未找到大小相等的 .dat 文件。")

    print("\n" + "="*80)
    print(f"总计：my_dir 中文件数 {len(my_files)}；匹配数 {matched_count}；未匹配数 {unmatched_count}")
    print("="*80)

if __name__ == "__main__":
    my_dir = "./release/RE枪补V3发布20250923/my_dat枪数量18"
    src_dir = "./paks/dat_原厂14283/file_0"
    check_my_files(my_dir, src_dir)
