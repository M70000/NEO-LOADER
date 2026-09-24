#pragma once

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <cstdint>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

typedef HMODULE(WINAPI* f_LoadLibraryA)(const char* lpLibFileName);
typedef FARPROC(WINAPI* f_GetProcAddress)(HMODULE hModule, const char* lpProcName);
typedef BOOL(WINAPI* f_DLL_ENTRY_POINT)(void* hDll, DWORD dwReason, void* pReserved);
typedef BOOLEAN(WINAPI* f_RtlAddFunctionTable)(PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount, DWORD64 BaseAddress);

enum class InjectionStatus : uint32_t {
    Waiting = 0,
    Executing,
    Finished,
    Error
};

struct ShellcodeData {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    f_RtlAddFunctionTable pRtlAddFunctionTable;

    uintptr_t pDllBase;
    uintptr_t EntryPoint;

    uintptr_t RelocDir;
    uintptr_t ImportDir;
    uintptr_t ExceptionDir;
    uintptr_t ExceptionSize;
    uintptr_t TLSDir;

    InjectionStatus Status;
    uint32_t ErrorCode;
};

struct ProcessEntry {
    DWORD pid;
    std::wstring name;
    std::wstring arch;
};

namespace Memory {
    // Process discovery and management
    DWORD GetPID(const wchar_t* exeName);
    bool IsProcess64(DWORD pid);
    bool EnableDebugPrivilege();
    std::vector<ProcessEntry> GetProcessList();

    // Manual Map Injection (Exclusively Manual Map - LoadLibrary removed)
    bool InjectDLL(DWORD pid, const std::string& dllPath, std::string& outLog);
    bool InjectDLLFromMemory(DWORD pid, const std::vector<BYTE>& dllBytes, std::string& outLog);
}

void RelocateImage(PBYTE buffer, uintptr_t targetBase);
void __stdcall UniversalShellcode(ShellcodeData* pData);
void __stdcall UniversalShellcodeEnd();
