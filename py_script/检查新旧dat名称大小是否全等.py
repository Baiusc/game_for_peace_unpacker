import os
import shutil 
from typing import Dict, List, Tuple

def build_size_index(src_dir: str) -> Dict[int, List[str]]:
    """
    扫描 src_dir，建立 {文件大小: [文件路径, ...]} 的索引
    """
    size_map: Dict[int, List[str]] = {}
    for root, _, files in os.walk(src_dir):
        for f in files:
            # 统一使用小写进行匹配，以确保不区分大小写
            if not f.lower().endswith(('.dat', '.uasset', '.uexp')):
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
            # 统一使用小写进行匹配，以确保不区分大小写
            if not f.lower().endswith(('.dat', '.uasset', '.uexp')):
                continue
            path = os.path.join(root, f)
            try:
                sz = os.path.getsize(path)
            except OSError as e:
                print(f"无法读取文件大小: {path} -> {e}")
                continue
            results.append((f, path, sz))
    return results

def check_my_files(my_dir: str, src_dir: str, is_auto_rename: bool, max_show_matches: int = 200):
    """
    对 my_dir 中的每个 .dat，检查 src_dir 中是否存在相同字节大小且文件名相同的文件。
    如果 is_auto_rename 为 True，将自动重命名 my_dir 中匹配大小但文件名不同的文件。
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

    # =================================================================
    # 新增排序逻辑：按文件名的数字大小倒序 (从大到小) 排序
    # =================================================================
    def get_numeric_name(file_tuple: Tuple[str, str, int]) -> int:
        # file_tuple[0] 是文件名，例如 '00000230.dat'
        name = file_tuple[0]
        try:
            # 移除后缀，转换为整数进行比较
            return int(name.replace('.dat', ''))
        except ValueError:
            # 如果文件名不符合数字格式，返回 0 或其他值确保其被处理
            return 0 
    
    my_files.sort(key=get_numeric_name, reverse=True)
    # =================================================================

    matched_count = 0
    unmatched_count = 0
    renamed_count = 0

    print("\n" + "="*80)
    print("检测结果（按 my_dir 文件分组）")
    print(f"自动重命名模式: {'启用' if is_auto_rename else '禁用'}")
    print("================================================================================")

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
                # 存在大小匹配但文件名不同的文件
                print(f"my_dir_file: {name} | 文件名不匹配 | 匹配大小文件数: {len(matches)} | size: {size} bytes")
                
                # --- 自动重命名逻辑 ---
                if is_auto_rename:
                    src_match_path = matches[0]
                    new_name = os.path.basename(src_match_path)
                    
                    # 构造新的文件路径 (保持 my_dir 内部的目录结构)
                    new_path = os.path.join(os.path.dirname(path), new_name)
                    
                    if os.path.exists(new_path):
                         # 目标文件已存在，但由于是倒序处理，可能是之前的重命名操作遗留下的冲突
                         print(f"    [重命名失败] 目标文件已存在，请手动检查或清理目标目录: {new_path}")
                    else:
                        try:
                            os.rename(path, new_path)
                            renamed_count += 1
                            print(f"    [已重命名] {name} -> {new_name}")
                            # 注意: 重命名后，原文件已不存在，但为了打印信息，我们继续使用旧的 name 变量。
                        except OSError as e:
                            print(f"    [重命名错误] 无法重命名 {path} 为 {new_path}: {e}")

                # 打印所有匹配大小但文件名不同的文件（无论是否重命名）
                for m in matches:
                    print(f"    匹配大小文件 -> src_file: {os.path.basename(m)} | src_dir: {os.path.dirname(m)}")
            else:
                print(f"my_dir_file: {name} | 未找到匹配的 src 文件 | size: {size} bytes")

    print("\n" + "="*80)
    print(f"总计：my_dir 中文件数 {len(my_files)}；匹配数 {matched_count}；未匹配数 {unmatched_count}")
    if is_auto_rename:
        print(f"自动重命名文件数: {renamed_count}")
    print("="*80)


if __name__ == "__main__":
    is_auto_rename = False # 设置为 True 启用自动重命名，设置为 False 仅进行检查

    my_dir = "./release/RE枪补V5发布20250930/my_dat20251013"
    # my_dir = "./release//RE范围V1发布20251014/待打包dat"
    # my_dir = "./paks/dat_temp"
    # src_dir = "./paks/dat_patch_14323原厂"
    # src_dir = "./paks/dat_map_weapon14210原厂"
    src_dir = "./paks/ShadowTrackerExtra_patch_14323原厂"
    # src_dir = "./release//RE范围V1发布20251014/待打包uexp"
    # === 手机uexp解包打包 ===
    # src_dir = "/run/user/1000/gvfs/mtp:host=Xiaomi_MI_8_UD_92daeda3/内部存储设备/Download/UEXP三合一/UEXP解包"
    # ==========================
    check_my_files(my_dir, src_dir, is_auto_rename)