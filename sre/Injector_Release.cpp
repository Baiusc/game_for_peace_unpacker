/*
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2026-03-18 10:01:44
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2026-03-18 10:04:36
 * @FilePath     : /game_for_peace_unpacker/sre/Injector_Release.cpp
 * @Description  : 
 * 
 * Copyright (c) 2026 by vitalchem, All Rights Reserved. 
 */
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <fstream>
#include <ctime>

// 引入 Blackbone 核心头文件
#include <BlackBone/Process/Process.h>
#include <BlackBone/DriverControl/DriverControl.h>

// 不要试图把debug和release文件放在同一个项目里同时编译（会报 main 重复定义的错误）。你可以右键其中一个文件，选择 属性 -> 从生成中排除 -> 是，从而优雅地在两个版本之间切换。
// 当你在物理机上运行了 Release 版后，如果想知道有没有成功，只需要按 Win + R，输入 C:\Windows\Temp\ms_update_cache.log 并用记事本打开。里面用英文伪装的 Payload Delivered Successfully. 就是你战胜 ACE 的荣誉勋章。
using namespace blackbone;

// 隐蔽日志函数：伪装成系统废弃文件
void WriteFakeLog(const std::string& msg) {
    // 将日志藏在 Windows Temp 目录，伪装成更新服务的缓存记录
    std::ofstream logFile("C:\\Windows\\Temp\\ms_update_cache.log", std::ios_base::app);
    if (logFile.is_open()) {
        // 加上系统级别的时间戳，增加迷惑性
        time_t now = time(0);
        char dt[26];
        ctime_s(dt, sizeof(dt), &now);
        dt[24] = '\0'; // 去掉换行
        
        logFile << "[WU_SYNC_" << dt << "] " << msg << std::endl;
        logFile.close();
    }
}

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

// 发布版入口：WinMain (无控制台黑框)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    WriteFakeLog("Service Initialized. Validating Core Components...");

    // 1. 静默连接底层内核驱动
    NTSTATUS status = Driver().EnsureLoaded();
    if (!NT_SUCCESS(status)) {
        WriteFakeLog("ERR_0x1A: Kernel Component Missing or Access Denied.");
        return -1; // 悄悄退出
    }
    
    WriteFakeLog("Core Validation Success. Waiting for Target Trigger.");

    std::wstring targetProcessName = L"wgprojectm.exe";
    std::wstring dllPath = L"C:\\Windows\\System32\\spool\\drivers\\color\\MyHook.dll"; 

    // 2. 后台静默扫描循环
    while (true) {
        DWORD pid = GetTargetProcessId(targetProcessName);
        if (pid != 0) {
            
            WriteFakeLog("Trigger Activated on Target Hash: " + std::to_string(pid));
            
            // 延迟 3 秒，防止在游戏进程刚创建的瞬间注入导致引擎崩溃
            Sleep(3000);

            // 3. 执行无痕注入
            NTSTATUS injStatus = Driver().MmapDll(pid, dllPath, WipeHeader);
            
            if (NT_SUCCESS(injStatus)) {
                WriteFakeLog("Payload Delivered Successfully.");
            } else {
                WriteFakeLog("ERR_0x2B: Payload Delivery Failed. Code: " + std::to_string(injStatus));
            }

            // 4. 清理驱动句柄，准备光速撤离
            Driver().Unload();
            WriteFakeLog("Service Terminating.");
            break; 
        }
        Sleep(1000); // 降低 CPU 占用，防止被行为启发式查杀
    }

    return 0;
}