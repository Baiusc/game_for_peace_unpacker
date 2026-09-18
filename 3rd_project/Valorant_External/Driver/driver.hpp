#pragma once
#include <TlHelp32.h>
#include <cstddef>
#include <fstream>
#include <atlbase.h>
#include <atlconv.h>
ptrdiff_t virtualaddy;
ptrdiff_t virtualbase;

std::string randomisim(size_t length);

#define readio CTL_CODE(FILE_DEVICE_UNKNOWN, 0xa69aB5dE, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define baseadress CTL_CODE(FILE_DEVICE_UNKNOWN, 0xfA5b9b46, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

std::string overlayname = randomisim(12);
std::string overlayname2 = randomisim(12);
std::string drivername = std::string(skCrypt("\\\\.\\ViNmuk1BrLnJ"));
typedef struct _read {
	ULONGLONG address;
	ULONGLONG buffer;
	INT32 process_id;
	BOOLEAN write;
	PVOID fake;
	ULONGLONG size;
} readstr, * readw;

typedef struct _ba {
	ULONGLONG* address;
	INT32 process_id;
} ba, * pba;

namespace mem {
	HANDLE driver_handle;
	INT32 process_id;

	bool find_driver() {
		CA2W unicodeStr(drivername.c_str());
		driver_handle = CreateFileW(unicodeStr, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

		if (!driver_handle || (driver_handle == INVALID_HANDLE_VALUE))
			return false;

		return true;
	}


	void write_the_memory(PVOID address, PVOID buffer, DWORD size) {
		_read arguments = { 0 };

		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = TRUE;

		DeviceIoControl(driver_handle, readio, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}

	void read_physical(PVOID address, PVOID buffer, DWORD size) {
		_read arguments = { 0 };

		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = FALSE;

		DeviceIoControl(driver_handle, readio, &arguments, sizeof(a#pragma once
#include <TlHelp32.h>
#include <cstddef>
#include <fstream>
#include <atlbase.h>
#include <atlconv.h>
ptrdiff_t virtualaddy;
ptrdiff_t virtualbase;

std::string randomisim(size_t length);

#define readio CTL_CODE(FILE_DEVICE_UNKNOWN, 0xa69aB5dE, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define baseadress CTL_CODE(FILE_DEVICE_UNKNOWN, 0xfA5b9b46, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

std::string overlayname = randomisim(12);
std::string overlayname2 = randomisim(12);
std::string drivername = std::string(skCrypt("\\\\.\\ViNmuk1BrLnJ"));
typedef struct _read {
	ULONGLONG address;
	ULONGLONG buffer;
	INT32 process_id;
	BOOLEAN write;
	PVOID fake;
	ULONGLONG size;
} readstr, * readw;

typedef struct _ba {
	ULONGLONG* address;
	INT32 process_id;
} ba, * pba;

namespace mem {
	HANDLE driver_handle;
	INT32 process_id;

	bool find_driver() {
		CA2W unicodeStr(drivername.c_str());
		driver_handle = CreateFileW(unicodeStr, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

		if (!driver_handle || (driver_handle == INVALID_HANDLE_VALUE))
			return false;

		return true;
	}


	void write_the_memory(PVOID address, PVOID buffer, DWORD size) {
		_read arguments = { 0 };

		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = TRUE;

		DeviceIoControl(driver_handle, readio, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}

	void read_physical(PVOID address, PVOID buffer, DWORD size) {
		_read arguments = { 0 };

		arguments.address = (ULONGLONG)address;
		arguments.buffer = (ULONGLONG)buffer;
		arguments.size = size;
		arguments.process_id = process_id;
		arguments.write = FALSE;

		DeviceIoControl(driver_handle, readio, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}
	void find_image() {
		uintptr_t image_address = { NULL };
		_ba arguments = { NULL };

		arguments.process_id = process_id;
		arguments.address = (ULONGLONG*)&virtualbase;

		DeviceIoControl(driver_handle, baseadress, &arguments, sizeof(arguments), nullptr, NULL, NULL, NULL);
	}

	INT32 find_process(LPCTSTR process_name) {
		PROCESSENTRY32 pt;
		HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		pt.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hsnap, &pt)) {
			do {
				if (!lstrcmpi(pt.szExeFile, process_name)) {
					CloseHandle(hsnap);
					process_id = pt.th32ProcessID;
					return pt.th32ProcessID;
				}
			} while (Process32Next(hsnap, &pt));
		}
		CloseHandle(hsnap);

		return { NULL };
	}
}

template <typename T>
T readguarded(uintptr_t src, size_t size = sizeof(T))
{
	T buffer;
	mem::read_physical((PVOID)src, &buffer, sizeof(T));
	uintptr_t val = virtualaddy + (*(uintptr_t*)&buffer & 0xFFFFFF);
	return *(T*)&val;
}
template <typename T>
T readv(uint64_t address) {
	T buffer{ };
	mem::read_physical((PVOID)address, &buffer, sizeof(T));
	return buffer;
}

static bool isguarded(uintptr_t pointer)
{
	static constexpr uintptr_t filter = 0xFFFFFFF000000000;
	uintptr_t result = pointer & filter;
	return result == 0x8000000000 || result == 0x10000000000;
}

template <typename T>
T write(uint64_t address, T buffer) {

	mem::write_the_memory((PVOID)address, &buffer, sizeof(T));
	return buffer;
}

template <typename T>
T read(uint64_t src)
{
	T buffer = readv< T >(src);

	if (isguarded((uintptr_t)readv< uintptr_t >(src)))
	{
		return readguarded< T >(src);
	}

	return buffer;
}
typedef struct _SYSTEM_BIGPOOL_ENTRY
{
	union {
		PVOID VirtualAddress;
		ULONG_PTR NonPaged : 1;
	};
	ULONG_PTR SizeInBytes;
	union {
		UCHAR Tag[4];
		ULONG TagUlong;
	};
} SYSTEM_BIGPOOL_ENTRY, * PSYSTEM_BIGPOOL_ENTRY;

typedef struct _SYSTEM_BIGPOOL_INFORMATION {
	ULONG Count;
	SYSTEM_BIGPOOL_ENTRY AllocatedInfo[ANYSIZE_ARRAY];
} SYSTEM_BIGPOOL_INFORMATION, * PSYSTEM_BIGPOOL_INFORMATION;

typedef enum _SSYSTEM_INFORMATION_CLASS {
	SystemBigPoolInformation = 0x42
} SSYSTEM_INFORMATION_CLASS;

typedef NTSTATUS(WINAPI* pNtQuerySystemInformation)(
	IN _SSYSTEM_INFORMATION_CLASS SystemInformationClass,
	OUT PVOID                   SystemInformation,
	IN ULONG                    SystemInformationLength,
	OUT PULONG                  ReturnLength
	);

auto query_bigpools() -> PSYSTEM_BIGPOOL_INFORMATION
{
	static const pNtQuerySystemInformation NtQuerySystemInformation =
		(pNtQuerySystemInformation)GetProcAddress(GetModuleHandleA(skCrypt("ntdll.dll")), skCrypt("NtQuerySystemInformation"));

	DWORD length = 0;
	DWORD size = 0;
	LPVOID heap = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0);
	heap = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, heap, 0xFF);
	NTSTATUS ntLastStatus = NtQuerySystemInformation(SystemBigPoolInformation, heap, 0x30, &length);
	heap = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, heap, length + 0x1F);
	size = length;
	ntLastStatus = NtQuerySystemInformation(SystemBigPoolInformation, heap, size, &length);

	return reinterpret_cast<PSYSTEM_BIGPOOL_INFORMATION>(heap);
}
auto retrieve_guarded() -> uintptr_t
{
	auto pool_information = query_bigpools();
	uintptr_t guarded = 0;

	if (pool_information)
	{
		auto count = pool_information->Count;
		for (auto i = 0ul; i < count; i++)
		{
			SYSTEM_BIGPOOL_ENTRY* allocation_entry = &pool_information->AllocatedInfo[i];
			const auto virtual_address = (PVOID)((uintptr_t)allocation_entry->VirtualAddress & ~1ull);
			if (allocation_entry->NonPaged && allocation_entry->SizeInBytes == 0x200000)
				if (guarded == 0 && allocation_entry->TagUlong == 'TnoC')
					guarded = reinterpret_cast<uintptr_t>(virtual_address);
		}
	}

	return guarded;
}


