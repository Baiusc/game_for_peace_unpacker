'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-25 16:22:15
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-09-25 16:24:22
FilePath     : /game_for_peace_unpacker/py_script/检查新旧dat名称大小是否全等.py
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
    对 my_dir 中的每个 .dat，检查 src_dir 中是否存在相同字节大小且文件名相同的文件。
    打印整齐的匹配/未匹配信息，并在文件名不匹配时显示匹配大小的 src 文件。
    """
    if not os.path.isdir(my_dir):
        print(f"错误: my_dir 不存在: {my_dir}")
        return
    if not os.path.isdir(src_dir):
        print(f"错误: src_dir 不存在: {src_dir}")
        return

    print(f"正在扫描 src_dir 并建立大小索引...")
    src_index = build_size_index(src_dir)
    my_files = list_my_files(my_dir)

    if not my_files:
        print("my_dir 中没有发现 .dat 文件。")
        return

    matched_count = 0
    unmatched_count = 0

    print("\n" + "="*80)
    print("检测结果（按 my_dir 文件分组）")
    print("="*80)

    for name, path, size in my_files:
        matches = src_index.get(size, [])
        # 只保留同名文件
        exact_matches = [m for m in matches if os.path.basename(m) == name]

        if exact_matches:
            matched_count += 1
            total = len(exact_matches)
            to_show = exact_matches if total <= max_show_matches else exact_matches[:max_show_matches]
            for m in to_show:
                print(f"my_dir_file: {name} | src_file: {os.path.basename(m)} | src_dir: {os.path.dirname(m)} | size: {size} bytes")
            if total > max_show_matches:
                print(f"... 其余 {total - max_show_matches} 个匹配文件未显示")
        else:
            unmatched_count += 1
            if matches:
                print(f"my_dir_file: {name} | 文件名不匹配 | 匹配大小文件数: {len(matches)} | size: {size} bytes")
                # 打印所有匹配大小但文件名不同的文件
                for m in matches:
                    print(f"    匹配大小但文件名不同 -> src_file: {os.path.basename(m)} | src_dir: {os.path.dirname(m)}")
            else:
                print(f"my_dir_file: {name} | 未找到匹配的 src 文件 | size: {size} bytes")

    print("\n" + "="*80)
    print(f"总计：my_dir 中文件数 {len(my_files)}；匹配数 {matched_count}；未匹配数 {unmatched_count}")
    print("="*80)


if __name__ == "__main__":
    my_dir = "./release/RE枪补V3发布20250924/my_dat枪数量22没改6把栓狙"
    src_dir = "./paks/dat_temp/file_0"
    check_my_files(my_dir, src_dir)
