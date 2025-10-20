'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-09-28 15:32:10
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-10-20 17:00:34
FilePath     : /game_for_peace_unpacker/py_script/搜索str关键词.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
from typing import List

def print_text_context(file_path: str, content: str, match_pos: int, match_len: int, context_len: int):
    """打印匹配字符串的上下文"""
    start = max(0, match_pos - context_len)
    end = min(len(content), match_pos + match_len + context_len)

    context = content[start:end]
    marked_context = (
        context[:match_pos-start] +
        f"【{context[match_pos-start:match_pos-start+match_len]}】" +
        context[match_pos-start+match_len:]
    )

    print(f"\n▼ 匹配内容: {content[match_pos:match_pos+match_len]}")
    print(f"▼ 文件位置: {file_path}")
    print(f"▼ 偏移地址: {match_pos}")
    print("▼ 上下文内容:")
    print("-" * 80)
    for i in range(0, len(marked_context), 80):
        print(marked_context[i:i+80])
    print("-" * 80)


def search_files(src_dir: str, my_file_suffix: str, my_str: str, context_len: int = 64):
    """搜索目录下指定后缀文件中是否包含目标字符串，并打印上下文"""
    print(f"正在扫描目录 [{src_dir}]，文件后缀= [{my_file_suffix}]，目标字符串= [{my_str}]")

    match_count = 0
    for root, _, files in os.walk(src_dir):
        for file in files:
            if not file.endswith(my_file_suffix):
                continue
            file_path = os.path.join(root, file)
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                pos = content.find(my_str)
                while pos != -1:
                    if match_count == 0:
                        print(f"\n▷ 发现匹配文件: {file_path}")
                    print_text_context(file_path, content, pos, len(my_str), context_len)
                    match_count += 1
                    pos = content.find(my_str, pos + len(my_str))

            except Exception as e:
                print(f"【错误】读取文件 {file_path} 失败: {e}")

    print("\n扫描完成")
    print(f"共找到 {match_count} 处匹配内容")


if __name__ == "__main__":
    # 示例参数
    SRC_DIR = "./paks/dat_obb14210原厂" # 搜索目录
    SRC_DIR = "./paks/dat_temp"
    MY_FILE_SUFFIX = ".dat"          # 文件后缀
    MY_STR = "SkeletalBodySetup"     # 搜索字符串
    MY_STR = "BaseColor"     # 搜索字符串
    MY_STR = "dea0b64523798a67a16b4d9240d94c4a4a7c6b78"     # 搜索字符串
    CONTEXT_LEN = 64 * 4                 # 上下文长度（字符数）

    search_files(SRC_DIR, MY_FILE_SUFFIX, MY_STR, CONTEXT_LEN)
