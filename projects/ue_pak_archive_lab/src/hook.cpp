#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <TlHelp32.h>

// ======================================================================
// 全局配置与特征
// ======================================================================
uintptr_t g_TargetAddress = 0;

// 必须声明为全局或静态，保证指针在函数生命周期外依然有效
wchar_t g_FakePath[] = L"ShadowTrackerExtra/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/DP28/BP_Other_DP28.uasset";

// ======================================================================
// 1. VEH 异常处理函数 (我们的隐形 Hook Payload)
// ======================================================================
LONG WINAPI VEH_Handler(PEXCEPTION_POINTERS pExceptionInfo) {
    // 检查是否是我们设置的硬件断点触发的 (单步异常)
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        
        // 检查触发异常的地址是否是我们的目标 OpenRead 函数
        if (pExceptionInfo->ContextRecord->Rip == g_TargetAddress) {
            
            // 【核心劫持逻辑】
            // 在 x64 __fastcall 调用约定中：
            // RCX = _this (param_1)
            // RDX = Filename (param_2)
            // R8  = ReadFlags (param_3)
            wchar_t* filename = (wchar_t*)pExceptionInfo->ContextRecord->Rdx;

            // 极速裸指针匹配，绝不分配堆内存
            if (filename != nullptr && wcsstr(filename, L"CH_Base_SK_PhysicsAsset") != nullptr) {
                
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                std::wcout << L"\n[!] 触发 HWBP 硬件断点! 成功拦截变态骨骼!" << std::endl;
                std::wcout << L"    ├─ 引擎原始请求: " << filename << std::endl;
                
                // 【偷天换日】：直接修改 RDX 寄存器，让它指向我们的伪造路径！
                // 没有修改任何游戏代码，只是在这个瞬间骗过了 CPU
                pExceptionInfo->ContextRecord->Rdx = (DWORD64)g_FakePath;
                
                std::wcout << L"    └─ 寄存器 RDX 已重定向至: " << g_FakePath << std::endl;
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            }

            // 【放行机制】
            // 设置 EFlags 的 RF (Resume Flag) 标志位为 1。
            // 告诉 CPU：“我知道这里有个断点，但这次请放行，执行完这条指令后再继续生效。”
            pExceptionInfo->ContextRecord->EFlags |= (1 << 16); 
            
            // 告诉操作系统：异常我已经处理好了，请继续执行游戏
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    // 如果不是我们的异常，扔给游戏的崩溃报告器处理
    return EXCEPTION_CONTINUE_SEARCH;
}

// ======================================================================
// 2. 线程遍历与硬件断点挂载 (DR0)
// ======================================================================
void SetHardwareBreakpoint() {
    DWORD currentPID = GetCurrentProcessId();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        
        if (Thread32First(hSnapshot, &te)) {
            do {
                // 找到属于当前游戏进程的所有线程
                if (te.th32OwnerProcessID == currentPID) {
                    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (hThread) {
                        SuspendThread(hThread); // 必须先挂起线程才能改寄存器
                        
                        CONTEXT ctx;
                        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                        if (GetThreadContext(hThread, &ctx)) {
                            // 将目标地址写入 DR0 寄存器
                            ctx.Dr0 = g_TargetAddress;
                            // 配置 DR7 寄存器：启用 DR0 的局部断点 (第0位)，设置为执行时触发
                            ctx.Dr7 &= ~(0xF0000); // 清除 16-19 位
                            ctx.Dr7 |= 1;          // 设置 L0 为 1
                            
                            SetThreadContext(hThread, &ctx);
                        }
                        
                        ResumeThread(hThread); // 恢复线程
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
}

// ======================================================================
// 3. 守护线程 (UE4 会频繁创建新线程进行异步加载，必须持续挂载)
// ======================================================================
DWORD WINAPI HWBP_MonitorThread(LPVOID lpReserved) {
    while (true) {
        SetHardwareBreakpoint();
        Sleep(100); // 每 100ms 扫描一次新线程并挂上硬件断点
    }
    return 0;
}

// ======================================================================
// 4. 核心初始化
// ======================================================================
DWORD WINAPI MainThread(LPVOID lpReserved) {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    std::wcout.imbue(std::locale("chs"));

    std::cout << "    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·\n";
    std::cout << "     UE4 硬件断点(HWBP) 无痕劫持控制台 \n";
    std::cout << "    ·  ˚  ✦  ˚  ·  ˚  ✦  ·  ˚  ✦  ˚  ·\n\n";

    HMODULE hModule = GetModuleHandle(NULL);
    uintptr_t baseAddress = (uintptr_t)hModule;
    std::cout << "[*] 游戏主模块基址: 0x" << std::hex << uppercase << baseAddress << std::endl;

    // 填入你历经千辛万苦挖出来的偏移量！
    uintptr_t openRead_offset = 0x18ddac0; 
    g_TargetAddress = baseAddress + openRead_offset;
    std::cout << "[*] 目标 OpenRead 绝对内存地址: 0x" << g_TargetAddress << std::endl;

    // 1. 注册 VEH 异常处理函数 (放在处理链的最前端)
    PVOID vehHandle = AddVectoredExceptionHandler(1, VEH_Handler);
    if (!vehHandle) {
        std::cout << "[❌] VEH 异常处理器注册失败！" << std::endl;
        return FALSE;
    }

    // 2. 启动守护线程，为所有游戏线程挂载硬件断点
    CreateThread(nullptr, 0, HWBP_MonitorThread, nullptr, 0, nullptr);

    std::cout << "[✅] HWBP 无痕劫持已部署完毕！无视 .text 完整性校验！" << std::endl;
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}