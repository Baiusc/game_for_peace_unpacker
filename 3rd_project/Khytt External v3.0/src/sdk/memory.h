#pragma once
#include <Windows.h>
// Include TlHelp32 after Windows
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>

namespace Memory {
inline HANDLE ProcessHandle = nullptr;
inline DWORD ProcessID = 0;
inline uintptr_t ClientBase = 0;

inline std::wstring ToWString(const std::string &str) {
  if (str.empty())
    return L"";
  int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
  std::wstring ws(size, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &ws[0], size);
  return ws;
}

inline bool Attach(const char *name) {
  PROCESSENTRY32W entry;
  entry.dwSize = sizeof(PROCESSENTRY32W);
  std::wstring ws = ToWString(name);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  if (snapshot == INVALID_HANDLE_VALUE)
    return false;

  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, ws.c_str()) == 0) {
        ProcessID = entry.th32ProcessID;
        ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessID);
        CloseHandle(snapshot);
        return true;
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return false;
}

inline uintptr_t GetModuleBase(const char *name) {
  if (ProcessID == 0)
    return 0;
  MODULEENTRY32W entry;
  entry.dwSize = sizeof(MODULEENTRY32W);
  std::wstring ws = ToWString(name);

  HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, ProcessID);
  if (snapshot == INVALID_HANDLE_VALUE)
    return 0;

  if (Module32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szModule, ws.c_str()) == 0) {
        CloseHandle(snapshot);
        return (uintptr_t)entry.modBaseAddr;
      }
    } while (Module32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return 0;
}

template <typename T> inline T Read(uintptr_t address) {
  T buffer = T{};
  if (ProcessHandle && address)
    ReadProcessMemory(ProcessHandle, (LPCVOID)address, &buffer, sizeof(T),
                      nullptr);
  return buffer;
}

inline bool ReadBytes(uintptr_t address, void *buffer, size_t size) {
  if (!ProcessHandle || !address)
    return false;
  return ReadProcessMemory(ProcessHandle, (LPCVOID)address, buffer, size,
                           nullptr);
}

template <typename T> inline bool Write(uintptr_t address, const T &value) {
  if (!ProcessHandle || !address)
    return false;
  return WriteProcessMemory(ProcessHandle, (LPVOID)address, &value, sizeof(T),
                            nullptr);
}

inline std::string ReadString(uintptr_t address, size_t size = 128) {
  std::vector<char> buffer(size);
  if (ProcessHandle && address)
    ReadProcessMemory(ProcessHandle, (LPCVOID)address, buffer.data(), size,
                      nullptr);
  return std::string(buffer.data());
}

inline bool Initialize() {
  if (!Attach("cs2.exe"))
    return false;
  ClientBase = GetModuleBase("client.dll");
  return (ClientBase != 0);
}
} // namespace Memory
