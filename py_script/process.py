'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-21 08:58:56
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-09-21 09:42:38
FilePath     : /game_for_peace_unpacker/py_script/process.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import shutil
from dataclasses import dataclass

# 定义一个数据类，用于存储文件信息，充当您的“结构体”
@dataclass
class FileMatch:
    ue_name: str     # 虚幻引擎文件的名称
    ue_dir: str      # 虚幻引擎文件所在的目录
    file_size: int   # 文件大小（字节）
    dat_name: str = None  # 匹配到的 .dat 文件名称
    dat_path: str = None  # 匹配到的 .dat 文件的完整路径

def find_ue_files(start_path):
    """
    递归搜索指定目录中的.uasset和.uexp文件。
    返回一个包含每个文件名称、目录和大小的 FileMatch 实例列表。

    Args:
        start_path (str): 开始搜索的根目录。

    Returns:
        list[FileMatch]: 包含 ue_name, ue_dir 和 file_size 的 FileMatch 实例列表。
    """
    ue_file_matches = []
    
    # 检查路径是否存在且为目录
    if not os.path.exists(start_path) or not os.path.isdir(start_path):
        print(f"错误：路径 '{start_path}' 不存在或不是一个目录。")
        return ue_file_matches

    print("正在搜索虚幻引擎文件 (.uasset, .uexp) 并收集文件信息...\n")
    
    for root, dirs, files in os.walk(start_path):
        for filename in files:
            if filename.endswith(('.uasset', '.uexp')):
                try:
                    full_path = os.path.join(root, filename)
                    file_size = os.path.getsize(full_path)
                    
                    # 创建 FileMatch 实例并添加到列表中
                    ue_file_matches.append(FileMatch(
                        ue_name=filename, 
                        ue_dir=root, 
                        file_size=file_size
                    ))
                    
                except OSError as e:
                    print(f"处理文件 '{full_path}' 时出错：{e}")
    
    return ue_file_matches

def find_files_by_size(start_path, ue_matches, suffix='.dat'):
    """
    根据输入的 ue_matches 列表，递归搜索指定目录中的文件，
    找到大小匹配的文件，并更新列表中的 dat_name 和 dat_path 字段。

    Args:
        start_path (str): 开始搜索的根目录。
        ue_matches (list[FileMatch]): 包含 ue_name, ue_dir 和 file_size 的 FileMatch 实例列表。
        suffix (str): 要搜索的文件后缀名，默认为 '.dat'。

    Returns:
        list[FileMatch]: 包含完整匹配信息的 FileMatch 实例列表。
    """
    if not ue_matches:
        print("未提供虚幻引擎文件信息，无法进行搜索。")
        return []

    # 将 ue_matches 转换为字典，以便通过文件大小快速查找
    size_to_ue = {match.file_size: match for match in ue_matches}
    found_matches = []
    
    print(f"\n正在搜索大小匹配且后缀为 '{suffix}' 的文件...\n")
    
    for root, dirs, files in os.walk(start_path):
        for filename in files:
            if filename.endswith(suffix):
                try:
                    full_path = os.path.join(root, filename)
                    file_size = os.path.getsize(full_path)
                    
                    # 检查文件大小是否在 ue_matches 列表中
                    if file_size in size_to_ue:
                        # 获取匹配的 FileMatch 实例
                        match = size_to_ue[file_size]
                        
                        # 更新 dat_name 和 dat_path 字段
                        match.dat_name = filename
                        match.dat_path = full_path
                        found_matches.append(match)
                        
                        print(f"找到匹配：")
                        print(f"UE 文件: {match.ue_name}")
                        print(f"UE 目录: {match.ue_dir}")
                        print(f".DAT 文件: {match.dat_name}")
                        print(f"大小: {match.file_size} 字节")
                        print("-" * 20)
                        
                        # 从字典中移除已匹配的项，以避免重复匹配
                        del size_to_ue[file_size]
                        
                except OSError as e:
                    print(f"处理文件 '{full_path}' 时出错：{e}")
                    
    return found_matches

def move_dat_files(matches):
    """
    将匹配到的 .dat 文件移动到相应的 UE 文件所在的目录。

    Args:
        matches (list[FileMatch]): 包含完整匹配信息的 FileMatch 实例列表。
    """
    if not matches:
        print("没有可移动的文件。")
        return

    print("\n=== 开始移动文件 ===")
    for match in matches:
        if match.dat_path and match.ue_dir:
            try:
                dest_path = os.path.join(match.ue_dir, match.dat_name)
                # 确保目标目录存在
                if not os.path.exists(match.ue_dir):
                    os.makedirs(match.ue_dir)
                
                shutil.move(match.dat_path, dest_path)
                print(f"已移动: {match.dat_name} -> {match.ue_dir}")
            except FileNotFoundError:
                print(f"错误: 文件 '{match.dat_path}' 不存在。")
            except Exception as e:
                print(f"移动文件 '{match.dat_name}' 时出错: {e}")
    print("=== 文件移动完成 ===")


def find_ue_files_in_src_dir(ue_src_dir, ue_matches):
    """
    在源目录中搜索与 ue_matches 列表中的文件名称和大小相匹配的 UE 文件。

    Args:
        ue_src_dir (str): 包含原始UE文件的源目录。
        ue_matches (list[FileMatch]): 包含要匹配的UE文件信息的列表。

    Returns:
        dict: 一个字典，键是UE文件的名称，值是一个包含其源路径和大小的元组。
    """
    ue_src_matches = {}
    if not ue_matches:
        print("未提供UE文件信息，无法进行搜索。")
        return ue_src_matches

    # 创建一个键为 (文件名, 文件大小) 的集合，以快速查找
    match_set = {(m.ue_name, m.file_size) for m in ue_matches}

    print(f"\n正在搜索源目录 '{ue_src_dir}' 中的UE文件...\n")
    for root, dirs, files in os.walk(ue_src_dir):
        for filename in files:
            if filename.endswith(('.uasset', '.uexp')):
                try:
                    full_path = os.path.join(root, filename)
                    file_size = os.path.getsize(full_path)
                    
                    if (filename, file_size) in match_set:
                        ue_src_matches[filename] = (full_path, file_size)
                        print(f"找到源文件: {filename}, 路径: {full_path}, 大小: {file_size} 字节")
                        
                except OSError as e:
                    print(f"处理文件 '{full_path}' 时出错: {e}")
    
    return ue_src_matches

def copy_ue_files(ue_src_matches, out_dir):
    """
    将 ue_src_matches 字典中的文件复制到指定的输出目录。

    Args:
        ue_src_matches (dict): 包含UE文件名称、源路径和大小的字典。
        out_dir (str): 目标输出目录。
    """
    if not ue_src_matches:
        print("没有可复制的UE文件。")
        return

    # 确保输出目录存在
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    print("\n=== 开始复制UE文件 ===")
    for filename, (src_path, size) in ue_src_matches.items():
        dest_path = os.path.join(out_dir, filename)
        try:
            shutil.copy2(src_path, dest_path)
            print(f"已复制: {filename} -> {out_dir}")
        except Exception as e:
            print(f"复制文件 '{filename}' 时出错: {e}")
    print("=== 文件复制完成 ===")

def main():
    """
    主函数，用于执行脚本。
    """
    # 示例用法
    # 假设这些目录是存在的
    ue_asset_dir = "./dat_edit/ue_asset_patch_14269_20250921原厂"
    dat_dir = "./dat_edit/dat_all_patch_14269_20250921原厂"
    ue_src_dir = "./dat_edit/ue_all原厂"  # 假设这是您要复制原始UE文件的源目录
    out_dir = "./dat_edit/ue_roi原厂"        # 假设这是您要放置复制文件的目标目录

    # 1. 查找 UE 文件并获取包含名称、目录和大小的列表
    ue_matches = find_ue_files(ue_asset_dir)
    
    # 2. 根据该列表查找匹配的 DAT 文件，并获取完整信息
    if ue_matches:
        complete_matches = find_files_by_size(dat_dir, ue_matches)
        
        # 打印最终结果摘要
        print("\n=== 最终匹配结果 ===")
        if complete_matches:
            for match in complete_matches:
                print(f"UE文件: {match.ue_name}, DAT文件: {match.dat_name}, 大小: {match.file_size} 字节, 目标目录: {match.ue_dir}")
            
            # 3. 移动文件
            move_dat_files(complete_matches)
        else:
            print("没有找到任何匹配项。")

    # --- 新增的流程 ---
    print("\n\n--- 原始UE文件复制流程 ---")
    if ue_matches:
        # 1. 搜索原始UE文件
        ue_src_matches = find_ue_files_in_src_dir(ue_src_dir, ue_matches)
        
        # 2. 复制文件
        copy_ue_files(ue_src_matches, out_dir)


if __name__ == "__main__":
    main()