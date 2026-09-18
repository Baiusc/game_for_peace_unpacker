#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <string>

// ────────────────────────────────────────────────────────────────
//  r6-driver kernel interface (registry-callback bypass)
//  Based on: tools/reversed_src/r6-driver/inc/shared.h
// ────────────────────────────────────────────────────────────────

#define DRIVER_MAGIC        0x2F93416ULL
#define OP_READ             0
#define OP_WRITE            1
#define OP_GET_MODULE_BASE  2

#pragma pack(push, 1)
struct driver_request_t {
    HANDLE   caller_pid;
    HANDLE   target_pid;
    UINT64   magic;
    char*    driver_path;
    UINT64   address;
    UINT64   size;
    UINT64   buffer;
    UINT64   reserved;
    char*    module_name;
    ULONG    operation;
    ULONG    padding;
    void*    output_buffer;
};
#pragma pack(pop)

typedef struct _module_base_response {
    UINT64 field_00;
    UINT64 field_08;
    UINT64 base_addr;
    UINT64 field_18;
    UINT64 field_20;
    UINT64 field_28;
} module_base_response;

// ────────────────────────────────────────────────────────────────
//  KernelDriver — singleton that talks to the loaded r6-driver
// ────────────────────────────────────────────────────────────────
class KernelDriver {
    HANDLE target_pid = nullptr;
    bool connected    = false;

    bool SendRequest(driver_request_t& req) const {
        req.caller_pid = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(GetCurrentProcessId()));
        
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services", 0,
                          KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            return false;

        LSTATUS r = RegSetValueExA(hKey, "r6c", 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(&req), sizeof(req));
        RegCloseKey(hKey);

        // Driver returns STATUS_ALERTED (0x101) which maps to ERROR_ALERTED (283)
        return r == ERROR_SUCCESS || r == 283;
    }

public:
    static KernelDriver& Get() {
        static KernelDriver inst;
        return inst;
    }

    // Try to connect - returns true if driver responds correctly
    bool Connect(DWORD pid) {
        target_pid = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid));

        uint64_t dummy = 0;
        driver_request_t req = {};
        req.target_pid  = target_pid;
        req.magic       = DRIVER_MAGIC;
        req.operation   = OP_READ;
        req.address     = 0x1000; // Non-null test address
        req.size        = sizeof(dummy);
        req.buffer      = reinterpret_cast<UINT64>(&dummy);

        connected = SendRequest(req);
        return connected;
    }

    bool IsConnected() const { return connected; }

    template<typename T>
    T Read(uintptr_t address) const {
        T value = {};
        Read(address, &value, sizeof(T));
        return value;
    }

    bool Read(uintptr_t address, void* buffer, size_t size) const {
        driver_request_t req = {};
        req.target_pid  = target_pid;
        req.magic       = DRIVER_MAGIC;
        req.operation   = OP_READ;
        req.address     = address;
        req.size        = size;
        req.buffer      = reinterpret_cast<UINT64>(buffer);
        return SendRequest(req);
    }

    template<typename T>
    bool Write(uintptr_t address, const T& value) const {
        return Write(address, &value, sizeof(T));
    }

    bool Write(uintptr_t address, const void* buffer, size_t size) const {
        driver_request_t req = {};
        req.target_pid  = target_pid;
        req.magic       = DRIVER_MAGIC;
        req.operation   = OP_WRITE;
        req.address     = address;
        req.size        = size;
        req.buffer      = reinterpret_cast<UINT64>(buffer);
        return SendRequest(req);
    }

    uintptr_t GetModuleBase(const std::string& modName) const {
        module_base_response resp = {};
        char nameBuf[256] = {};
        modName.copy(nameBuf, sizeof(nameBuf) - 1);

        driver_request_t req = {};
        req.target_pid    = target_pid;
        req.magic         = DRIVER_MAGIC;
        req.operation     = OP_GET_MODULE_BASE;
        req.module_name   = nameBuf;
        req.output_buffer = &resp;
        SendRequest(req);
        return static_cast<uintptr_t>(resp.base_addr);
    }
};
