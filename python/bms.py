'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-08-21 10:10:50
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-09-02 12:00:05
FilePath     : /game_for_peace_unpacker/python/bms.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import subprocess
import time
from glob import glob

pak_file = "./paks/神_PC_0.2范围_Ak_MK14_P90_午后.pak"
unpack_dir = "./unpack/dat_temp"
repack_dir = "./repack"

# 定义解包函数
def jb():
    print(f"解包文件: {pak_file}")
    print(f"解包目录: {unpack_dir}")
    # 执行解包命令（去掉 qemu-i386 不需要安卓模拟PC环境）
    command = [
        "./python/quickbms",  # 直接运行 quickbms
        "./python/解包.bms",  # 确保路径正确
        pak_file, 
        unpack_dir
    ]
    print("正在解包，请稍候...")
    result = subprocess.run(command)
    
    if result.returncode != 0:
        print(f"解包失败，错误码: {result.returncode}")
    else:
        print(f"解包成功: {pak_file}")
    time.sleep(1)

# 定义打包函数
def db():
    print("请选择要打包的 PAK 文件：")
    print("0. 操作取消")
    pak_files = glob("/sdcard/Download/江川三合一路径/PAK/*.pak")
    for i, file in enumerate(pak_files, start=1):
        print(f"{i}. {os.path.basename(file)}")
    
    while True:
        try:
            choice = int(input("请输入选项: "))
            if choice == 0:
                print("操作取消，正在返回...")
                time.sleep(1)
                return
            elif 1 <= choice <= len(pak_files):
                selected_file = pak_files[choice - 1]
                break
            else:
                print("无效选择，请重新输入。")
        except ValueError:
            print("无效输入，请输入数字。")
    
    # 执行打包命令
    output_dir = "/sdcard/Download/江川三合一路径/打包"
    command = [
        "qemu-i386", 
        "打解包/quickbms", 
        "-w", "-r", "-r", 
        "打解包/打包.bms", 
        selected_file, 
        output_dir
    ]
    print("正在打包，请稍候...")
    result = subprocess.run(command)
    
    if result.returncode != 0:
        print(f"打包失败，错误码: {result.returncode}")
    else:
        print(f"打包成功: {selected_file}")
    time.sleep(1)

# 主菜单
def main_menu():
    while True:
        print("欢迎使用 PAK 文件工具")
        print("1. 解包 PAK 文件")
        print("2. 打包 PAK 文件")
        print("0. 退出")
        choice = input("请选择操作: ")
        
        if choice == "1":
            jb()
        elif choice == "2":
            db()
        elif choice == "0":
            print("退出程序")
            break
        else:
            print("无效选择，请重新输入。")

if __name__ == "__main__":
    main_menu()