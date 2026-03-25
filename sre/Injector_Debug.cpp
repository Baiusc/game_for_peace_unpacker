/*
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2026-03-18 10:01:36
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2026-03-18 10:04:30
 * @FilePath     : /game_for_peace_unpacker/sre/Injector_Debug.cpp
 * @Description  : 
 * 
 * Copyright (c) 2026 by vitalchem, All Rights Reserved. 
 */
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

// 引入 Blackbone 核心头文件
#include <BlackBone/Process/Process.h>
#include <BlackBone/DriverControl/DriverControl.h>
// // 不要试图把debug和release文件放在同一个项目里同时编译（会报 main 重复定义的错误）。你可以右键其中一个文件，选择 属性 -> 从生成中排除 -> 是，从而优雅地在两个版本之间切换。
using namespace blackbone;

// 辅助函数：根据进程名获取 PID
DWORD GetTargetProcessId(const std::wstring& processName) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (processName == pe32.szExeFile) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

// 调试版入口：带控制台
int main() {
    // 设置控制台支持中文和颜色
    std::wcout.imbue(std::locale("chs"));
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
    
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"  [DEV] Blackbone 内核注入器 (调试版) " << std::endl;
    std::wcout << L"========================================" << std::endl;

    // 1. 连接底层内核驱动
    std::wcout << L"[*] 正在尝试连接 BlackBoneDrv.sys..." << std::endl;
    NTSTATUS status = Driver().EnsureLoaded();
    if (!NT_SUCCESS(status)) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::wcout << L"[❌] 致命错误：无法连接到内核驱动！请确认是否已使用 KDU 强载！错误码: 0x" << std::hex << status << std::endl;
        system("pause");
        return -1;
    }
    std::wcout << L"[✅] 内核驱动连接成功！" << std::endl;

    std::wstring targetProcessName = L"wgprojectm.exe";
    std::wstring dllPath = L"C:\\Windows\\System32\\spool\\drivers\\color\\MyHook.dll"; 

    std::wcout << L"[*] 等待目标进程: " << targetProcessName << L" 启动..." << std::endl;

    // 2. 扫描循环
    while (true) {
        DWORD pid = GetTargetProcessId(targetProcessName);
        if (pid != 0) {
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            std::wcout << L"[+] 发现目标进程！PID: " << std::dec << pid << std::endl;
            std::wcout << L"[*] 暂停 3 秒等待游戏引擎底层初始化..." << std::endl;
            Sleep(3000);

            std::wcout << L"[*] 开始执行 Ring 0 Manual Map 降维打击..." << std::endl;
            
            // 3. 执行注入
            NTSTATUS injStatus = Driver().MmapDll(pid, dllPath, WipeHeader);
            if (NT_SUCCESS(injStatus)) {
                std::wcout << L"[✅] 注入完美成功！HWBP 拦截器已部署！" << std::endl;
            } else {
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::wcout << L"[❌] 注入失败！错误码: 0x" << std::hex << injStatus << std::endl;
            }

            // 4. 清理退场
            Driver().Unload();
            std::wcout << L"[*] 驱动连接已断开，按任意键退出..." << std::endl;
            system("pause");
            break; 
        }
        Sleep(1000); // 没发现就每秒扫一次
    }
    return 0;
}