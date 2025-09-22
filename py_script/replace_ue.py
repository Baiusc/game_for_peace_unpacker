'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-21 10:50:00
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-09-21 09:55:16
FilePath     : /game_for_peace_unpacker/py_script/replace_ue.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import shutil
from dataclasses import dataclass

@dataclass
class FileInfo:
    """用于存储文件信息的简单数据结构。"""
    name: str
    path: str
    size: int

def find_files(start_dir):
    """
    递归搜索指定目录下的.uasset和.uexp文件。
    返回一个字典，键为文件名，值为FileInfo实例。
    如果存在同名但不同大小的文件，会打印警告。
    """
    files_dict = {}
    print(f"正在搜索目录 '{start_dir}'...")

    if not os.path.exists(start_dir) or not os.path.isdir(start_dir):
        print(f"错误: 目录 '{start_dir}' 不存在或不是一个目录。")
        return files_dict

    for root, dirs, files in os.walk(start_dir):
        for filename in files:
            if filename.endswith(('.uasset', '.uexp')):
                full_path = os.path.join(root, filename)
                try:
                    file_size = os.path.getsize(full_path)
                    
                    if filename in files_dict and files_dict[filename].size != file_size:
                        print(f"警告: 文件 '{filename}' 在不同位置存在不同大小的版本。")
                        print(f"  旧文件: {files_dict[filename].path} ({files_dict[filename].size} 字节)")
                        print(f"  新文件: {full_path} ({file_size} 字节)")
                        continue # 跳过处理此文件，以防混淆
                    
                    files_dict[filename] = FileInfo(name=filename, path=full_path, size=file_size)
                    
                except OSError as e:
                    print(f"处理文件 '{full_path}' 时出错: {e}")
    
    return files_dict

def replace_files(new_files_dir, old_files_dir):
    """
    将新目录中的文件替换旧目录中同名且同大小的文件。
    """
    print("\n--- 开始文件替换流程 ---")
    
    # 1. 搜索新文件
    new_files = find_files(new_files_dir)
    if not new_files:
        print("在新文件目录中未找到可供替换的 .uasset 或 .uexp 文件。")
        return
        
    # 2. 搜索旧文件
    old_files = find_files(old_files_dir)
    if not old_files:
        print("在旧文件目录中未找到任何 .uasset 或 .uexp 文件。")
        return

    # 3. 执行替换操作
    replacements_made = 0
    for filename, new_file_info in new_files.items():
        if filename in old_files:
            old_file_info = old_files[filename]
            
            # 检查文件大小是否匹配
            if new_file_info.size == old_file_info.size:
                print(f"\n找到匹配文件: '{filename}'")
                print(f"  新文件路径: {new_file_info.path}")
                print(f"  旧文件路径: {old_file_info.path}")
                print(f"  文件大小: {new_file_info.size} 字节")
                
                try:
                    # 先删除旧文件
                    os.remove(old_file_info.path)
                    # 使用shutil.copy2以保留元数据
                    shutil.copy2(new_file_info.path, old_file_info.path)
                    print(f"✅ 成功替换: {old_file_info.path}")
                    replacements_made += 1
                except Exception as e:
                    print(f"❌ 替换文件 '{filename}' 时出错: {e}")
            else:
                print(f"\n文件 '{filename}' 大小不匹配，跳过替换。")
                print(f"  新文件大小: {new_file_info.size} 字节")
                print(f"  旧文件大小: {old_file_info.size} 字节")

    print("\n--- 文件替换流程结束 ---")
    print(f"总共成功替换了 {replacements_made} 个文件。")


if __name__ == "__main__":
    # 请在这里设置您的目录路径
    new_ue_file_dir = "./dat_edit/ue_roi原厂"  # 新文件的根目录
    old_ue_tree_dir = "./dat_edit/ue_asset_patch_14269_20250921原厂"   # 旧文件的根目录

    replace_files(new_ue_file_dir, old_ue_tree_dir)