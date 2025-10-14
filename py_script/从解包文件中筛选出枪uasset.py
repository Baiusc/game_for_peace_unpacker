'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-23 09:33:20
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-10-13 15:16:50
FilePath     : /game_for_peace_unpacker/py_script/从解包文件中筛选出枪uasset.py
Description  : 从解包文件中筛选符合规则的枪械 uasset 文件，并可选择复制到目标目录

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import shutil
from typing import List

def match_rules(filename: str) -> bool:
    """
    判断文件名是否符合指定规则：
    1. 文件后缀是 .uasset 或 .uexp
    2. 文件名以 BP_ 开头
    3. 文件名中的 "_" 总数为 1 或 2
    4. 文件名中不包含 "Mag"
    """
    name, ext = os.path.splitext(filename)

    if ext.lower() not in (".uasset", ".uexp"):
        return False
    if not name.startswith("BP_"):
        return False
    if name.count("_") not in (1, 2):
        return False
    if "Mag" in name:
        return False

    return True


def find_matching_files(src_ue_dir: str) -> List[str]:
    """递归遍历 src_ue_dir，筛选出符合规则的文件"""
    matched_files = []
    for root, _, files in os.walk(src_ue_dir):
        for file in files:
            if match_rules(file):
                matched_files.append(os.path.join(root, file))
    return matched_files


if __name__ == "__main__":
    src_ue_dir = "./paks/ShadowTrackerExtra_patch_14323原厂"
    is_print_uasset_only = True    # True=只打印 复制 移动 .uasset 
    is_copy_and_move = True        # True=复制符合规则的文件到 src_ue_dir

    results = find_matching_files(src_ue_dir)

    print("符合规则的文件：")
    for f in results:
        if is_print_uasset_only and not f.lower().endswith(".uasset"):
            continue

        if is_copy_and_move:
            dest = os.path.join(src_ue_dir, os.path.basename(f))
            if not os.path.exists(dest):
                shutil.copy2(f, dest)
                print(f"已复制: {f} -> {dest}")
            else:
                print(f"已存在(跳过): {dest}")
        else:
            print(f)
