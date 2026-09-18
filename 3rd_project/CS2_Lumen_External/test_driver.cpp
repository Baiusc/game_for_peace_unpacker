#include <windows.h>
#include <iostream>
#include <cstdint>

#define DRIVER_MAGIC 0x2F93416ULL
#define OP_READ 0

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

int main() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        std::cout << "Failed to open reg key" << std::endl;
        return 1;
    }

    uint64_t dummy = 0;
    driver_request_t req = {};
    req.caller_pid = (HANDLE)GetCurrentProcessId();
    req.target_pid = (HANDLE)GetCurrentProcessId(); // Test on ourselves
    req.magic = DRIVER_MAGIC;
    req.operation = OP_READ;
    req.address = (UINT64)&dummy; // Valid address in our process
    req.size = sizeof(dummy);
    req.buffer = reinterpret_cast<UINT64>(&dummy);

    LSTATUS r = RegSetValueExA(hKey, "r6c", 0, REG_BINARY, reinterpret_cast<const BYTE*>(&req), sizeof(req));
    RegCloseKey(hKey);

    std::cout << "RegSetValueExA returned: " << r << " (0x" << std::hex << r << ")" << std::dec << std::endl;
    
    if (r == 0) {
        std::cout << "Driver NOT intercepted (returned success/actual registry write)" << std::endl;
    } else if (r == 283) {
        std::cout << "Driver intercepted (ERROR_ALERTED)" << std::endl;
    } else {
        std::cout << "Unknown result" << std::endl;
    }

    return 0;
}
