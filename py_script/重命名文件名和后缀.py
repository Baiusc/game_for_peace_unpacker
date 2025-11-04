'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-11-04 09:30:40
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-11-04 10:06:24
FilePath     : /game_for_peace_unpacker/py_script/重命名文件名和后缀.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import shutil
from typing import List

# -------------------------- rename_dat2ue (保持不变，已满足您的成对重命名需求) --------------------------
def rename_dat2ue(src_dir: str, src_suffix: str, list_new_suffix: List[str], is_auto_rename: bool):
    """
    将 src_dir 中所有符合 src_suffix 的文件按数字顺序排序，
    并按成对的方式重命名。每对中的两个文件将共用较小的文件名数字作为基础文件名。
    例如：001.dat, 002.dat -> 001.uasset, 001.uexp
    注意：操作为 os.rename，不改变文件总数。要求 len(list_new_suffix) 必须为 2。
    """
    if not os.path.isdir(src_dir):
        print(f"错误：目录不存在 -> {src_dir}")
        return
    
    # 检查后缀列表长度，确保可以成对处理
    if len(list_new_suffix) != 2:
        print("错误：此重命名模式要求 list_new_suffix 必须包含两个后缀，例如 ['uasset', 'uexp']。")
        return

    # 1. 查找和排序文件
    files = []
    for f in os.listdir(src_dir):
        if f.lower().endswith(src_suffix.lower()):
            try:
                # 提取数字部分并去除后缀
                base_num_str = f[:-len(src_suffix)]
                base_num_str = base_num_str.strip()
                num = int(base_num_str)
                files.append((num, f, base_num_str))
            except ValueError:
                print(f"跳过无法解析数字的文件名: {f}")
                continue

    # 按提取的数字排序
    files.sort(key=lambda x: x[0])

    if not files:
        print(f"在 {src_dir} 中未找到后缀为 {src_suffix} 的文件。")
        return
    
    print("\n" + "="*60)
    print(f"开始成对重命名文件（自动重命名模式: {'启用' if is_auto_rename else '禁用'}）")
    print(f"重命名规则: N.dat, (N+1).dat -> N.{list_new_suffix[0]}, N.{list_new_suffix[1]}")
    print("="*60)
    
    # 2. 成对处理文件
    # i 从 0 开始，步长为 2
    for i in range(0, len(files) - 1, 2):
        file1 = files[i]
        file2 = files[i+1]
        
        # 基础文件名：使用较小的文件名数字（file1）
        base_num_str = file1[2]
        
        # 定义两个新的文件名和路径
        new_names = [f"{base_num_str}.{list_new_suffix[0]}", 
                     f"{base_num_str}.{list_new_suffix[1]}"]
        
        file_pairs = [(file1[1], new_names[0]), (file2[1], new_names[1])]
        
        # 3. 执行重命名操作 (对 file1 和 file2 依次进行)
        for old_name, new_name in file_pairs:
            old_path = os.path.join(src_dir, old_name)
            new_path = os.path.join(src_dir, new_name)

            if not os.path.exists(old_path):
                continue 

            if os.path.exists(new_path):
                print(f"[跳过] 目标文件已存在: {new_name}")
                continue
                
            if is_auto_rename:
                try:
                    os.rename(old_path, new_path)
                    print(f"[已重命名] {old_name} -> {new_name}")
                except OSError as e:
                    print(f"[错误] 无法重命名 {old_name} 为 {new_name}: {e}")
            else:
                print(f"[预览] 将重命名: {old_name} -> {new_name}")
                
    # 处理最后一个文件，如果文件总数为奇数
    if len(files) % 2 != 0:
        odd_file = files[-1]
        print(f"[警告] 最后一个文件 {odd_file[1]} 为单数，已跳过处理。")

    print("="*60)
    print("处理完成。")
    print("="*60)


# -------------------------- rename_ue2dat (已修改，实现反向重命名) --------------------------
def rename_ue2dat(src_dir: str, src_suffixes: List[str], is_auto_rename: bool):
    """
    将 src_dir 中所有符合 src_suffixes 的文件按文件名数字部分排序，
    并重命名为原始编号的连续 .dat 文件。
    例如：001.uasset 001.uexp 002.uasset 002.uexp -> 001.dat 002.dat 003.dat 004.dat
    注意：操作为 os.rename，不改变文件总数。
    """
    if not os.path.isdir(src_dir):
        print(f"错误：目录不存在 -> {src_dir}")
        return

    # 1. 查找和排序文件
    files = []
    for f in os.listdir(src_dir):
        
        # 记录原始后缀，用于判断是 uasset 还是 uexp
        original_suffix = None
        for suffix in src_suffixes:
            if f.lower().endswith(suffix.lower()):
                original_suffix = suffix.lower()
                break
        
        if original_suffix:
            try:
                # 提取数字部分并去除后缀
                base_num_str = f[:-len(original_suffix)]
                base_num_str = base_num_str.strip() # 修复：确保数字纯净
                num = int(base_num_str)
                # 存储原始数字、文件名、数字字符串和原始后缀
                files.append((num, f, base_num_str, original_suffix)) 
            except ValueError:
                print(f"跳过无法解析数字的文件名: {f}")
                continue

    # 按文件名中的数字排序
    files.sort(key=lambda x: x[0])

    if not files:
        print(f"在 {src_dir} 中未找到后缀为 {src_suffixes} 的文件。")
        return

    print("\n" + "="*60)
    print(f"开始将 {len(files)} 个文件重命名回原始编号的 .dat 文件（自动重命名模式: {'启用' if is_auto_rename else '禁用'}）")
    print("="*60)

    
    # 2. 执行重命名
    for i, (num, old_name, base_num_str, original_suffix) in enumerate(files):
        old_path = os.path.join(src_dir, old_name)
        
        # --- 关键逻辑：确定新的 dat 文件编号 ---
        new_dat_num = num
        
        # 如果是成对文件中的第二个（即 uexp），则编号加 1
        # 注意：这里假设 uasset/uexp 是成对出现的，并且 uasset 的文件编号 N 后面紧跟着 uexp 的文件编号 N
        if original_suffix == '.uexp' or original_suffix == 'uexp':
             new_dat_num = num + 1
        
        # 确保新文件名的数字格式与原始 base_num_str 的长度一致
        new_base_num_str = str(new_dat_num).zfill(len(base_num_str))
        new_name = f"{new_base_num_str}.dat" 
        new_path = os.path.join(src_dir, new_name)

        if os.path.exists(new_path):
            print(f"[跳过] 目标文件已存在: {new_name}")
            continue

        if is_auto_rename:
            try:
                os.rename(old_path, new_path) # 使用 os.rename 实现原地重命名
                print(f"[已重命名] {old_name} -> {new_name}")
            except OSError as e:
                print(f"[错误] 无法重命名 {old_name} 为 {new_name}: {e}")
        else:
            print(f"[预览] 将重命名: {old_name} -> {new_name}")

    print("="*60)
    print("处理完成。")
    print("="*60)


if __name__ == "__main__":

    is_auto_rename = False # 默认设为 False 进行预览

    src_dir = "./release/RE武器发布/my_ue" 
    rename_dat2ue(src_dir, ".dat", [".uasset", ".uexp"], is_auto_rename)

    src_dir = "./release/RE武器发布/my_dat" 
    rename_ue2dat(src_dir, [".uasset", ".uexp"], is_auto_rename)