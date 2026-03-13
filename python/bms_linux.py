'''
Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
Date         : 2025-08-21 10:10:50
LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
LastEditTime : 2025-11-28 17:12:21
FilePath     : /game_for_peace_unpacker/python/bms_linux.py
Description  : 

Copyright (c) 2025 by vitalchem, All Rights Reserved. 
'''
import os
import subprocess
import time
from glob import glob


pak_file = "./paks/game_patch_1.34.12.14514原厂（复件）.pak"

dat_dir = "./paks/dat_temp"

# 定义解包函数dat_temp
def jb():
    print(f"解包文件: {pak_file}")
    print(f"解包目录: {dat_dir}")
    # 执行解包命令（去掉 qemu-i386 不需要安卓模拟PC环境）
    command = [
        "./python/quickbms_linux",  # 直接运行 quickbms
        "./python/解包自制加注释.bms",  # 确保路径正确
        pak_file, 
        dat_dir
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
    # 执行打包命令
    command = [
        "./python/quickbms_linux", 
        "-w", "-r", "-r", 
        "./python/打包.bms", 
        pak_file, # 第一个参数：原始 PAK 文件
        dat_dir # 第二个参数：包含 .dat 文件的文件夹
    ]
    print("正在打包，请稍候...")
    result = subprocess.run(command)
    
    if result.returncode != 0:
        print(f"打包失败，错误码: {result.returncode}")
    else:
        print(f"打包成功: {dat_dir}")
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