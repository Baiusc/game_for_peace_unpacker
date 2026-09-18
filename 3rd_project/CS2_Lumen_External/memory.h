#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string_view>
#include <string>
#include "driver.h"

namespace Settings { extern bool kernel_bypass; }

class Memory
{
private:
    DWORD processId = 0;
    void* processHandle = nullptr;

public:
    Memory(const std::string_view processName) noexcept
    {
        PROCESSENTRY32W entry = { };
        entry.dwSize = sizeof(PROCESSENTRY32W);

        const auto snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        if (::Process32FirstW(snapShot, &entry))
        {
            do
            {
                char exeFileA[MAX_PATH];
                ::WideCharToMultiByte(CP_ACP, 0, entry.szExeFile, -1, exeFileA, MAX_PATH, NULL, NULL);
                if (!processName.compare(exeFileA))
                {
                    processId = entry.th32ProcessID;
                    processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
                    break;
                }
            } while (::Process32NextW(snapShot, &entry));
        }

        if (snapShot)
            ::CloseHandle(snapShot);
    }

    ~Memory()
    {
        if (processHandle)
            ::CloseHandle(processHandle);
    }

    const std::uintptr_t GetModuleAddress(const std::string_view moduleName) const noexcept
    {
        if (Settings::kernel_bypass && KernelDriver::Get().IsConnected())
        {
            return KernelDriver::Get().GetModuleBase(std::string(moduleName));
        }

        MODULEENTRY32W entry = { };
        entry.dwSize = sizeof(MODULEENTRY32W);

        const auto snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

        std::uintptr_t result = 0;

        if (::Module32FirstW(snapShot, &entry))
        {
            do
            {
                char moduleNameA[MAX_PATH];
                ::WideCharToMultiByte(CP_ACP, 0, entry.szModule, -1, moduleNameA, MAX_PATH, NULL, NULL);
                if (!moduleName.compare(moduleNameA))
                {
                    result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (::Module32NextW(snapShot, &entry));
        }

        if (snapShot)
            ::CloseHandle(snapShot);

        return result;
    }

    template <typename T>
    constexpr const T Read(const std::uintptr_t& address) const noexcept
    {
        if (Settings::kernel_bypass && KernelDriver::Get().IsConnected())
        {
            return KernelDriver::Get().Read<T>(address);
        }

        T value = { };
        ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
        return value;
    }

    template <typename T>
    constexpr void Write(const std::uintptr_t& address, const T& value) const noexcept
    {
        if (Settings::kernel_bypass && KernelDriver::Get().IsConnected())
        {
            KernelDriver::Get().Write<T>(address, value);
            return;
        }

        ::WriteProcessMemory(processHandle, reinterpret_cast<void*>(address), &value, sizeof(T), NULL);
    }

    bool ReadBytes(const std::uintptr_t& address, void* buffer, size_t size) const noexcept
    {
        if (Settings::kernel_bypass && KernelDriver::Get().IsConnected())
        {
            return KernelDriver::Get().Read(address, buffer, size);
        }

        return ::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), buffer, size, NULL);
    }

    bool WriteBytes(const std::uintptr_t& address, const void* buffer, size_t size) const noexcept
    {
        if (Settings::kernel_bypass && KernelDriver::Get().IsConnected())
        {
            return KernelDriver::Get().Write(address, buffer, size);
        }

        return ::WriteProcessMemory(processHandle, reinterpret_cast<void*>(address), buffer, size, NULL);
    }
};
