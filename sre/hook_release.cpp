#include <windows.h>
#include <TlHelp32.h>
#include <fstream>

// ======================================================================
// 全局配置与特征
// ======================================================================
uintptr_t g_TargetAddress = 0;

// 魔改骨骼数据所在的资产路径字符串，必须声明为全局或静态，保证指针在函数生命周期外依然有效
wchar_t g_FakePath[] = L"ShadowTrackerExtra/Content/Arts_PlayerBluePrints/Weapon/MainWeapon/Other/DP28/BP_Other_DP28.uasset";

// ======================================================================
// 1. VEH 异常处理函数 (我们的隐形 Hook Payload)
// ======================================================================
LONG WINAPI VEH_Handler(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
    {

        if (pExceptionInfo->ContextRecord->Rip == g_TargetAddress)
        {

            wchar_t *filename = (wchar_t *)pExceptionInfo->ContextRecord->Rdx;

            // 极速裸指针匹配，绝不分配堆内存
            if (filename != nullptr && wcsstr(filename, L"CH_Base_SK_PhysicsAsset") != nullptr)
            {

                // ==========================================
                // 1. 获取毫秒级精准时间戳 (Windows API)
                // ==========================================
                SYSTEMTIME st;
                GetLocalTime(&st); // 获取本地时间

                // 格式化时间为宽字符串 (例如: 2026-03-17 19:30:15.123)
                wchar_t timeBuffer[64];
                swprintf_s(timeBuffer, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
                           st.wYear, st.wMonth, st.wDay,
                           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

                // ==========================================
                // 2. 将时间戳和拦截到的原文件名写入隐蔽日志
                // ==========================================
                // 使用 std::wofstream 追加模式 (std::ios::app) 写入宽字符
                std::wofstream logFile(L"C:\\Windows\\Temp\\ms_update_cache.log", std::ios::app);
                if (logFile.is_open())
                {
                    logFile << timeBuffer << L" find target file path string: " << filename << std::endl;
                    logFile.close();
                }

                // ==========================================
                // 3. 执行你的“偷天换日” Hook 逻辑
                // ==========================================
                // 假设 g_FakePath 已经定义好
                pExceptionInfo->ContextRecord->Rdx = (DWORD64)g_FakePath;

                // 发出两声短促高音反馈 (可选，不需要可删除)
                Beep(1500, 50);
                Sleep(50);
                Beep(1500, 50);
            }

            // 放行机制
            pExceptionInfo->ContextRecord->EFlags |= (1 << 16);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ======================================================================
// 2. 线程遍历与硬件断点挂载 (DR0)
// ======================================================================
void SetHardwareBreakpoint()
{
    DWORD currentPID = GetCurrentProcessId();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);

        if (Thread32First(hSnapshot, &te))
        {
            do
            {
                if (te.th32OwnerProcessID == currentPID)
                {
                    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (hThread)
                    {
                        SuspendThread(hThread);

                        CONTEXT ctx;
                        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                        if (GetThreadContext(hThread, &ctx))
                        {
                            ctx.Dr0 = g_TargetAddress;
                            ctx.Dr7 &= ~(0xF0000);
                            ctx.Dr7 |= 1;
                            SetThreadContext(hThread, &ctx);
                        }

                        ResumeThread(hThread);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
}

// ======================================================================
// 3. 守护线程 (持续挂载硬件断点)
// ======================================================================
DWORD WINAPI HWBP_MonitorThread(LPVOID lpReserved)
{
    while (true)
    {
        SetHardwareBreakpoint();
        Sleep(100);
    }
    return 0;
}

// ======================================================================
// 4. 核心初始化 (完全静默)
// ======================================================================
DWORD WINAPI MainThread(LPVOID lpReserved)
{
    // 【清理现场】删除了所有 AllocConsole 和 std::cout

    HMODULE hModule = GetModuleHandle(NULL);
    uintptr_t baseAddress = (uintptr_t)hModule;

    // 你的目标偏移量
    uintptr_t openRead_offset = 0x18ddac0;
    g_TargetAddress = baseAddress + openRead_offset;

    // 注册 VEH
    PVOID vehHandle = AddVectoredExceptionHandler(1, VEH_Handler);
    if (!vehHandle)
    {
        // 注册失败，发出一声长低音提示你
        Beep(500, 1000);
        return FALSE;
    }

    // 启动守护线程
    CreateThread(nullptr, 0, HWBP_MonitorThread, nullptr, 0, nullptr);

    // 成功部署，发出一声清脆的高音
    Beep(2000, 100);

    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}