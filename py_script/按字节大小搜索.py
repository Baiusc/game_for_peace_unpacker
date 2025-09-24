import os
from typing import List

def search_single_file_by_byte_size(file_path: str, src_dir: str) -> List[str]:
    """
    在 src_dir 中查找与 file_path 大小相同的文件
    """
    try:
        target_size = os.path.getsize(file_path)
    except OSError as e:
        print(f"无法获取文件大小: {file_path} -> {e}")
        return []

    matched_files = []
    for root, _, files in os.walk(src_dir):
        for f in files:
            src_file_path = os.path.join(root, f)
            try:
                if os.path.getsize(src_file_path) == target_size:
                    matched_files.append(src_file_path)
            except OSError:
                continue
    return matched_files

# ========== 配置参数 ==========
my_files_dir = "./release/RE枪补V3发布20250924/my_ue枪数n"  # 文件夹路径
my_file_suffix = (".uasset", ".uexp")  # 文件后缀
src_dir = "./paks/dat_原厂14283"  # 要搜索的目录
is_rename = True  # 是否重命名 my 文件

# ========== 遍历 my_files_dir 中符合后缀的文件 ==========
for root, _, files in os.walk(my_files_dir):
    for f in files:
        if not f.endswith(my_file_suffix):
            continue
        my_file_path = os.path.join(root, f)
        matched_files = search_single_file_by_byte_size(my_file_path, src_dir)
        for src_file in matched_files:
            src_basename = os.path.basename(src_file)
            my_basename = os.path.basename(my_file_path)
            file_size = os.path.getsize(src_file)
            print(f"my_file: {my_basename} | src_file: {src_basename} | src_dir: {os.path.dirname(src_file)} | size: {file_size} bytes")

            # ====== 重命名 my_file ======
            if is_rename:
                new_my_file_path = os.path.join(os.path.dirname(my_file_path), src_basename)
                try:
                    os.rename(my_file_path, new_my_file_path)
                    print(f"  [RENAMED] {my_basename} -> {src_basename}")
                    # 更新 my_file_path 为新路径，防止重复处理旧名
                    my_file_path = new_my_file_path
                except OSError as e:
                    print(f"  [ERROR] 重命名失败: {e}")
