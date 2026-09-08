import os
import shutil
import sys
import random
import B_PackTool_quickbms as pt

WATERMARK = """
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
           老 6 工 具 
    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·
"""

def search_pak_files(base_path):
    """仅搜索指定根目录下的.pak文件（不包括子文件夹）"""
    pak_files = []
    # 只遍历base_path根目录，不递归子文件夹
    for file in os.listdir(base_path):
        file_path = os.path.join(base_path, file)
        # 检查是否为文件且以.pak结尾
        if os.path.isfile(file_path) and file.endswith(".pak"):
            pak_files.append(file_path)
    return pak_files


def create_base_folders(base_path, folder_names):
    """创建基础文件夹（打包、解包）"""
    for folder_name in folder_names:
        folder_path = os.path.join(base_path, folder_name)
        if not os.path.exists(folder_path):
            os.makedirs(folder_path)


def clear_directory_by_recreate(dir_path):
    if not os.path.exists(dir_path):
        print(f"目录不存在: {dir_path}")
        return False

    try:
        # 删除整个目录
        shutil.rmtree(dir_path)
        # 重新创建空目录
        os.makedirs(dir_path)
        print(f"已清空目录: {dir_path}")
        return True
    except Exception as e:
        print(f"清空目录失败: {e}")
        return False


def main():
    base_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "老6自动")
    # 基础文件夹包含制作区
    base_folder_names = ["打包", "解包", "PAK"]
    create_base_folders(base_path, base_folder_names)
    # 2. 用户选择操作（保留交互）
    while True:
        print(WATERMARK)
        print("请选择操作：")

        # 打印标题
        print("╔════════════════════════════════╗")
        print("║        欢迎使用老6工具        ║")
        print("╚════════════════════════════════╝")

        # 打印菜单选项 仅保留1、2
        print("║ 1. 解包                        ║")
        print("║ 2. 打包                        ║")
        print("╚════════════════════════════════╝")

        action_choice = None
        try:
            action_choice = input("请输入使用的功能:").strip()
        except KeyboardInterrupt:
            print("用户中断了程序")

        # 仅保留1、2有效选项
        if action_choice not in ["1","2"]:
            print("无效的操作选择。")
            continue
            
        if action_choice == "1" or action_choice == "2":
            pak_files = search_pak_files(base_path)
            if not pak_files:
                print(f"在 {base_path} 根目录下未找到任何.pak文件。")
                continue
                # 4. 用户选择要处理的pak文件
            print("找到以下.pak文件，请选择一个：")
            for idx, pak_file in enumerate(pak_files, start=1):
                print(f"{idx}. {pak_file}")

            try:
                choice = int(input("请输入数字选择一个文件："))
                selected_pak_file = pak_files[choice - 1]
            except (ValueError, IndexError):
                print("无效的选择，请重新输入。")
                continue
                # 5. 执行解包/打包

            if action_choice == "1":
                # 解包并获取路径和pak文件名
                file_name = os.path.splitext(os.path.basename(selected_pak_file))[0]  # 获取pak文件名（不含扩展名）
                unpack_path = os.path.join(base_path, "解包", file_name)
                if not os.path.exists(os.path.join(base_path, "打包")):
                    os.mkdir(os.path.join(base_path, "打包"))
                if not os.path.exists(os.path.join(base_path, "打包", file_name)):
                    os.mkdir(os.path.join(base_path, "打包", file_name))

                if not os.path.exists(os.path.join(base_path, "制作区")):
                    os.mkdir(os.path.join(base_path, "制作区"))
                if not os.path.exists(os.path.join(base_path, "制作区", file_name)):
                    os.mkdir(os.path.join(base_path, "制作区", file_name))
                tool = pt.UE4PakEngine(selected_pak_file)
                if tool.parse():
                    tool.extract(unpack_path)
            else:
                # 打包操作
                file_name = os.path.splitext(os.path.basename(selected_pak_file))[0]  # 获取pak文件名（不含扩展名）
                tool = pt.UE4PakEngine(selected_pak_file)
                repack_path = os.path.join(base_path, "打包", file_name)
                print(repack_path)
                if tool.parse():
                    tool.reimport(repack_path)


if __name__ == "__main__":
    main()
