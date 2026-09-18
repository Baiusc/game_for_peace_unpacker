#include <Windows.h>
#include <string>
#include "utils.hpp"
#include "skStr.h"
#include "protects.h"
#include "VMProtect/VMProtectsdk.h"
#include <thread>
#include "Driver/driver.hpp"
#include "Game/globals.hpp"
#include "Overlay/render.hpp"
#include "Game/cheat.hpp"
#include "Overlay/menu.hpp"
#include "Game/driver.hpp"
#include "protect2/xorstr.hpp"
#include "colorcmd.h"
#include "kdmapper/kdmapper.hpp"
#include "utils.hpp"
#include "skStr.h"

#pragma comment(lib,"VMProtectSDK64.lib")
#pragma comment(lib, "ntdll.lib")

std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
const std::string compilation_date = (std::string)skCrypt(__DATE__);
const std::string compilation_time = (std::string)skCrypt(__TIME__);


int runPE64(
    LPPROCESS_INFORMATION lpPI,
    LPSTARTUPINFO lpSI,
    LPVOID lpImage,
    LPWSTR wszArgs,
    SIZE_T szArgs
)
{
    WCHAR wszFilePath[MAX_PATH];
    if (!GetModuleFileName(
        NULL,
        (LPSTR)wszFilePath,
        sizeof wszFilePath
    ))
    {
        retur﻿#include <Windows.h>
#include <string>
#include "utils.hpp"
#include "skStr.h"
#include "protects.h"
#include "VMProtect/VMProtectsdk.h"
#include <thread>
#include "Driver/driver.hpp"
#include "Game/globals.hpp"
#include "Overlay/render.hpp"
#include "Game/cheat.hpp"
#include "Overlay/menu.hpp"
#include "Game/driver.hpp"
#include "protect2/xorstr.hpp"
#include "colorcmd.h"
#include "kdmapper/kdmapper.hpp"
#include "utils.hpp"
#include "skStr.h"

#pragma comment(lib,"VMProtectSDK64.lib")
#pragma comment(lib, "ntdll.lib")

std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
const std::string compilation_date = (std::string)skCrypt(__DATE__);
const std::string compilation_time = (std::string)skCrypt(__TIME__);


int runPE64(
    LPPROCESS_INFORMATION lpPI,
    LPSTARTUPINFO lpSI,
    LPVOID lpImage,
    LPWSTR wszArgs,
    SIZE_T szArgs
)
{
    WCHAR wszFilePath[MAX_PATH];
    if (!GetModuleFileName(
        NULL,
        (LPSTR)wszFilePath,
        sizeof wszFilePath
    ))
    {
        return -1;
    }
    WCHAR wszArgsBuffer[MAX_PATH + 2048];
    ZeroMemory(wszArgsBuffer, sizeof wszArgsBuffer);
    SIZE_T length = wcslen(wszFilePath);
    memcpy(
        wszArgsBuffer,
        wszFilePath,
        length * sizeof(WCHAR)
    );
    wszArgsBuffer[length] = ' ';
    memcpy(
        wszArgsBuffer + length + 1,
        wszArgs,
        szArgs
    );
    PIMAGE_DOS_HEADER lpDOSHeader =
        reinterpret_cast<PIMAGE_DOS_HEADER>(lpImage);
    PIMAGE_NT_HEADERS lpNTHeader =
        reinterpret_cast<PIMAGE_NT_HEADERS>(
            reinterpret_cast<DWORD64>(lpImage) + lpDOSHeader->e_lfanew
            );
    if (lpNTHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        return -2;
    }
    if (!CreateProcess(
        NULL,
        (LPSTR)wszArgsBuffer,
        NULL,
        NULL,
        TRUE,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        lpSI,
        lpPI
    ))
    {
        return -3;
    }
    CONTEXT stCtx;
    ZeroMemory(&stCtx, sizeof stCtx);
    stCtx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(lpPI->hThread, &stCtx))
    {
        TerminateProcess(
            lpPI->hProcess,
            -4
        );
        return -4;
    }
    LPVOID lpImageBase = VirtualAllocEx(
        lpPI->hProcess,
        reinterpret_cast<LPVOID>(lpNTHeader->OptionalHeader.ImageBase),
        lpNTHeader->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );
    if (lpImageBase == NULL)
    {
        TerminateProcess(
            lpPI->hProcess,
            -5
        );
        return -5;
    }
    if (!WriteProcessMemory(
        lpPI->hProcess,
        lpImageBase,
        lpImage,
        lpNTHeader->OptionalHeader.SizeOfHeaders,
        NULL
    ))
    {
        TerminateProcess(
            lpPI->hProcess,
            -6
        );
        return -6;
    }

    for (
        SIZE_T iSection = 0;
        iSection < lpNTHeader->FileHeader.NumberOfSections;
        ++iSection
        )
    {
        PIMAGE_SECTION_HEADER stSectionHeader =
            reinterpret_cast<PIMAGE_SECTION_HEADER>(
                reinterpret_cast<DWORD64>(lpImage) +
                lpDOSHeader->e_lfanew +
                sizeof(IMAGE_NT_HEADERS64) +
                sizeof(IMAGE_SECTION_HEADER) * iSection
                );
        if (!WriteProcessMemory(
            lpPI->hProcess,
            reinterpret_cast<LPVOID>(
                reinterpret_cast<DWORD64>(lpImageBase) +
                stSectionHeader->VirtualAddress
                ),
            reinterpret_cast<LPVOID>(
                reinterpret_cast<DWORD64>(lpImage) +
                stSectionHeader->PointerToRawData
                ),
            stSectionHeader->SizeOfRawData,
            NULL
        ))
        {
            TerminateProcess(
                lpPI->hProcess,
                -7
            );
            return -7;
        }
    }
    if (!WriteProcessMemory(
        lpPI->hProcess,
        reinterpret_cast<LPVOID>(
            stCtx.Rdx + sizeof(LPVOID) * 2
            ),
        &lpImageBase,
        sizeof(LPVOID),
        NULL
    ))
    {
        TerminateProcess(
            lpPI->hProcess,
            -8
        );
        return -8;
    }
    stCtx.Rcx = reinterpret_cast<DWORD64>(lpImageBase) +
        lpNTHeader->OptionalHeader.AddressOfEntryPoint;
    if (!SetThreadContext(
        lpPI->hThread,
        &stCtx
    ))
    {
        TerminateProcess(
            lpPI->hProcess,
            -9
        );
        return -9;
    }
    if (!ResumeThread(lpPI->hThread))
    {
        TerminateProcess(
            lpPI->hProcess,
            -10
        );
        return -10;
    }
    return 0;
}

int GetProcessID(const char* procname) {

    HANDLE hSnapshot;
    PROCESSENTRY32 pe;
    int pid = 0;
    BOOL hResult;

    // snapshot of all processes in the system
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) return 0;

    // initializing size: needed for using Process32First
    pe.dwSize = sizeof(PROCESSENTRY32);

    // info about first process encountered in a system snapshot
    hResult = Process32First(hSnapshot, &pe);

    // retrieve information about the processes
    // and exit if unsuccessful
    while (hResult) {
        // if we find the process: return process ID
        if (strcmp(procname, pe.szExeFile) == 0) {
            pid = pe.th32ProcessID;
            break;
        }
        hResult = Process32Next(hSnapshot, &pe);
    }

    // closes an open handle (CreateToolhelp32Snapshot)
    CloseHandle(hSnapshot);
    return pid;
}
auto render() -> void
{
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX9_NewFrame();
    ImGui::NewFrame();
    drawmenu();
    CheatLoop();
    ImGui::EndFrame();
    p_Device->SetRenderState(D3DRS_ZENABLE, false);
    p_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    p_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
    p_Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    if (p_Device->BeginScene() >= 0)
    {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        p_Device->EndScene();
    }

    HRESULT result = p_Device->Present(NULL, NULL, NULL, NULL);

    if (result == D3DERR_DEVICELOST && p_Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        p_Device->Reset(&d3dpp);
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

void startOverlayProcess() {
    SetupWindow();
    DirectXInit(MyWnd);
    static RECT old_rc;
    ZeroMemory(&Message, sizeof(MSG));

    while (Message.message != WM_QUIT) {
        if (PeekMessage(&Message, MyWnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        HWND hwnd_active = GetForegroundWindow();

        if (hwnd_active == GameWnd) {
            HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
            SetWindowPos(MyWnd, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        RECT rc;
        POINT xy;
        ZeroMemory(&rc, sizeof(RECT));
        ZeroMemory(&xy, sizeof(POINT));
        GetClientRect(GameWnd, &rc);
        ClientToScreen(GameWnd, &xy);
        rc.left = xy.x;
        rc.top = xy.y;
        ImGuiIO& io = ImGui::GetIO();
        io.ImeWindowHandle = GameWnd;
        POINT p;
        GetCursorPos(&p);
        io.MousePos.x = p.x - xy.x;
        io.MousePos.y = p.y - xy.y;

        if (GetAsyncKeyState(0x1)) {
            io.MouseDown[0] = true;
            io.MouseClicked[0] = true;
            io.MouseClickedPos[0].x = io.MousePos.x;
            io.MouseClickedPos[0].x = io.MousePos.y;
        }
        else {
            io.MouseDown[0] = false;
        }

        if (GetAsyncKeyState(0x2)) {
            io.MouseDown[1] = true;
            io.MouseClicked[1] = true;
            io.MouseClickedPos[1].x = io.MousePos.x;
            io.MouseClickedPos[1].x = io.MousePos.y;
        }
        else {
            io.MouseDown[1] = false;
        }
        render();
        Sleep(4);
    }
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanuoD3D();
    DestroyWindow(MyWnd);
}


void StartLooop() {


    system("cls");


    HANDLE iqvw64e_device_handle = intel_driver::Load();
    if (!kdmapper::MapDriver(iqvw64e_device_handle, rawData)) {
        intel_driver::ClearMmUnloadedDrivers(iqvw64e_device_handle);
        intel_driver::ClearKernelHashBucketList(iqvw64e_device_handle);
        intel_driver::ClearPiDDBCacheTable(iqvw64e_device_handle);
        intel_driver::Unload(iqvw64e_device_handle);
    }
    intel_driver::Unload(iqvw64e_device_handle);

    system("cls");
    if (mem::find_driver()) {
        system("cls");
    }
    else {
        system("cls");
        system("color c");
        std::cout << red << (std::string)skCrypt("\n\n Driver error...\n");
        Sleep(2000);
        exit(0);
    }

    while (Entryhwnd == NULL)
    {
        std::cout << yellow << skCrypt("open valorant!\r");

        Sleep(1);
        processid = GetProcessID(skCrypt("VALORANT-Win64-Shipping.exe"));
        Entryhwnd = get_process_wnd(processid);
        Sleep(1);
    }
    std::cout << green << skCrypt("wait!\n");
    system("cls");
    if (mem::find_process(skCrypt("VALORANT-Win64-Shipping.exe")))
    {
        virtualaddy = retrieve_guarded();
        mem::find_image();
    }



    std::cout << white << skCrypt("Menu Key:") << yellow << skCrypt(" INSERT\n");
    HANDLE hdl = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(startOverlayProcess), nullptr, NULL, nullptr);
    CloseHandle(hdl);


    std::thread(CacheGame).detach();
}
std::string getFirstHddSerialNumber() {
    //get a handle to the first physical drive
    HANDLE h = CreateFileW(L"\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return {};
    //an std::unique_ptr is used to perform cleanup automatically when returning (i.e. to avoid code duplication)
    std::unique_ptr<std::remove_pointer<HANDLE>::type, void(*)(HANDLE)> hDevice{ h, [](HANDLE handle) {CloseHandle(handle); } };
    //initialize a STORAGE_PROPERTY_QUERY data structure (to be used as input to DeviceIoControl)
    STORAGE_PROPERTY_QUERY storagePropertyQuery{};
    storagePropertyQuery.PropertyId = StorageDeviceProperty;
    storagePropertyQuery.QueryType = PropertyStandardQuery;
    //initialize a STORAGE_DESCRIPTOR_HEADER data structure (to be used as output from DeviceIoControl)
    STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader{};
    //the next call to DeviceIoControl retrieves necessary size (in order to allocate a suitable buffer)
    //call DeviceIoControl and return an empty std::string on failure
    DWORD dwBytesReturned = 0;
    if (!DeviceIoControl(hDevice.get(), IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        &storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER), &dwBytesReturned, NULL))
        return {};
    //allocate a suitable buffer
    const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
    std::unique_ptr<BYTE[]> pOutBuffer{ new BYTE[dwOutBufferSize]{} };
    //call DeviceIoControl with the allocated buffer
    if (!DeviceIoControl(hDevice.get(), IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
        pOutBuffer.get(), dwOutBufferSize, &dwBytesReturned, NULL))
        return {};
    //read and return the serial number out of the output buffer
    STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(pOutBuffer.get());
    const DWORD dwSerialNumberOffset = pDeviceDescriptor->SerialNumberOffset;
    if (dwSerialNumberOffset == 0) return {};
    const char* serialNumber = reinterpret_cast<const char*>(pOutBuffer.get() + dwSerialNumberOffset);
    return serialNumber;
}

int main()
{
    std::thread anti(SpyPrograms);
    std::thread anti2(findwind);
    std::thread anti3(vmdedect);
    driverdetect();
    VMProtectBeginMutation("bgsHmN1bsM");
    VMProtectIsDebuggerPresent("CheckKernelMode");
    VMProtectIsDebuggerPresent("cfasgqwq");
    void VMProtectBeginVirtualization(const char* bWFpbmN1bmM);
    void VMProtectBeginMutation(const char* bWFpbmN1bmM);
    bool VMProtectIsValidImageCRC(void);
    void VMProtectBeginVirtualizationLockByKey(const char* bWFpbmN1bmM);
    DebugControl();
    DebugControl2();
    setlocale(LC_ALL, "English");
    StartLooop();
    system("pause");
}

std::string tm_to_readable_time(tm ctx) {
    char buffer[80];

    strftime(buffer, sizeof(buffer), "%a %m/%d/%y %H:%M:%S %Z", &ctx);

    return std::string(buffer);
}

static std::time_t string_to_timet(std::string timestamp) {
    auto cv = strtol(timestamp.c_str(), NULL, 10); // long

    return (time_t)cv;
}

static std::tm timet_to_tm(time_t timestamp) {
    std::tm context;

    localtime_s(&context, &timestamp);

    return context;
}
