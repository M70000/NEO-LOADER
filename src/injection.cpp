#include "injection.h"
#include <algorithm>
#include <sstream>

#pragma optimize("", off)
__declspec(noinline) void __stdcall UniversalShellcode(ShellcodeData* pData) {
    if (!pData || !pData->pDllBase) {
        if (pData) pData->Status = InjectionStatus::Error;
        return;
    }

    pData->Status = InjectionStatus::Executing;
    uintptr_t pBase = pData->pDllBase;

    // Process Import Directory
    if (pData->ImportDir) {
        auto* pImportDescr = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(pBase + pData->ImportDir);
        while (pImportDescr->Name) {
            char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
            HMODULE hMod = pData->pLoadLibraryA(szMod);
            if (!hMod) {
                pData->Status = InjectionStatus::Error;
                pData->ErrorCode = 1; // Failed to load dependency module
                return;
            }

            auto* pIAT = reinterpret_cast<PIMAGE_THUNK_DATA>(pBase + pImportDescr->FirstThunk);
            PIMAGE_THUNK_DATA pThunk = pIAT;
            if (pImportDescr->OriginalFirstThunk) {
                pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(pBase + pImportDescr->OriginalFirstThunk);
            }

            while (pThunk->u1.AddressOfData) {
                if (IMAGE_SNAP_BY_ORDINAL(pThunk->u1.Ordinal)) {
                    pIAT->u1.Function = (uintptr_t)pData->pGetProcAddress(hMod, (LPCSTR)(pThunk->u1.Ordinal & 0xFFFF));
                }
                else {
                    auto* pImportByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + pThunk->u1.AddressOfData);
                    pIAT->u1.Function = (uintptr_t)pData->pGetProcAddress(hMod, pImportByName->Name);
                }
                pThunk++;
                pIAT++;
            }
            pImportDescr++;
        }
    }

    // Process TLS Callbacks
    if (pData->TLSDir) {
        auto* pTLS = reinterpret_cast<PIMAGE_TLS_DIRECTORY>(pBase + pData->TLSDir);
        auto* pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
        if (pCallback) {
            while (*pCallback) {
                (*pCallback)(reinterpret_cast<void*>(pBase), DLL_PROCESS_ATTACH, nullptr);
                pCallback++;
            }
        }
    }

    // Register Exception Handling (x64 SEH)
    if (pData->pRtlAddFunctionTable && pData->ExceptionDir && pData->ExceptionSize) {
        auto* pFuncTable = reinterpret_cast<PRUNTIME_FUNCTION>(pBase + pData->ExceptionDir);
        DWORD count = (DWORD)(pData->ExceptionSize / sizeof(RUNTIME_FUNCTION));
        pData->pRtlAddFunctionTable(pFuncTable, count, (DWORD64)pBase);
    }

    // Call DLL Entry Point
    if (pData->EntryPoint) {
        auto fEntryPoint = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pData->EntryPoint);
        fEntryPoint(reinterpret_cast<void*>(pBase), DLL_PROCESS_ATTACH, nullptr);
    }

    pData->Status = InjectionStatus::Finished;
}

__declspec(noinline) void __stdcall UniversalShellcodeEnd() {
}
#pragma optimize("", on)

static void* ResolveFunction(void* ptr) {
    unsigned char* b = (unsigned char*)ptr;
    if (b[0] == 0xE9) {
        int rel = *(int*)(b + 1);
        return (void*)(b + 5 + rel);
    }
    if (b[0] == 0xFF && b[1] == 0x25) {
        int disp = *(int*)(b + 2);
        void** target = (void**)(b + 6 + disp);
        return *target;
    }
    return ptr;
}

void RelocateImage(PBYTE buffer, uintptr_t targetBase) {
    auto* pDosHdr = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer);
    auto* pNt     = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer + pDosHdr->e_lfanew);
    auto* pOpt    = &pNt->OptionalHeader;

    uintptr_t delta = targetBase - (uintptr_t)pOpt->ImageBase;
    if (delta == 0) return;

    auto relocDir = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size == 0) return;

    auto* pReloc    = reinterpret_cast<IMAGE_BASE_RELOCATION*>(buffer + relocDir.VirtualAddress);
    uintptr_t relocEnd = (uintptr_t)pReloc + relocDir.Size;

    while (pReloc && (uintptr_t)pReloc < relocEnd && pReloc->SizeOfBlock > sizeof(IMAGE_BASE_RELOCATION)) {
        UINT  count = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        WORD* info  = reinterpret_cast<WORD*>(pReloc + 1);

        for (UINT i = 0; i < count; ++i) {
            WORD type   = info[i] >> 12;
            WORD offset = info[i] & 0xFFF;

            if (type == IMAGE_REL_BASED_DIR64) {
                uintptr_t* patch = reinterpret_cast<uintptr_t*>(buffer + pReloc->VirtualAddress + offset);
                *patch += delta;
            }
            else if (type == IMAGE_REL_BASED_HIGHLOW) {
                uint32_t* patch = reinterpret_cast<uint32_t*>(buffer + pReloc->VirtualAddress + offset);
                *patch += (uint32_t)delta;
            }
        }

        pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            reinterpret_cast<BYTE*>(pReloc) + pReloc->SizeOfBlock);
    }
}

namespace Memory {

    bool EnableDebugPrivilege() {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL res = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        CloseHandle(hToken);
        return (res != FALSE);
    }

    bool IsProcess64(DWORD pid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return false;

        BOOL bIsWow64 = FALSE;
        IsWow64Process(hProc, &bIsWow64);
        CloseHandle(hProc);

        if (bIsWow64) return false;

        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    }

    DWORD GetPID(const wchar_t* exeName) {
        DWORD pid = 0;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(pe32);
            if (Process32FirstW(hSnap, &pe32)) {
                do {
                    if (!_wcsicmp(pe32.szExeFile, exeName)) {
                        pid = pe32.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(hSnap, &pe32));
            }
            CloseHandle(hSnap);
        }
        return pid;
    }

    std::vector<ProcessEntry> GetProcessList() {
        std::vector<ProcessEntry> list;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return list;

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(pe32);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                if (pe32.th32ProcessID == 0 || pe32.th32ProcessID == 4) continue;
                ProcessEntry entry;
                entry.pid = pe32.th32ProcessID;
                entry.name = pe32.szExeFile;
                entry.arch = IsProcess64(entry.pid) ? L"x64" : L"x86";
                list.push_back(entry);
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);

        std::sort(list.begin(), list.end(), [](const ProcessEntry& a, const ProcessEntry& b) {
            std::wstring an = a.name, bn = b.name;
            std::transform(an.begin(), an.end(), an.begin(), ::towlower);
            std::transform(bn.begin(), bn.end(), bn.begin(), ::towlower);
            return an < bn;
        });

        return list;
    }

    bool InjectDLLFromMemory(DWORD pid, const std::vector<BYTE>& dllBytes, std::string& outLog) {
        EnableDebugPrivilege();

        if (dllBytes.size() < 0x1000) {
            outLog = "Error: Invalid DLL size (buffer is too small).";
            return false;
        }

        const BYTE* pSrc = dllBytes.data();
        auto* pDosLocal = reinterpret_cast<const IMAGE_DOS_HEADER*>(pSrc);
        if (pDosLocal->e_magic != IMAGE_DOS_SIGNATURE) {
            outLog = "Error: Invalid DOS MZ signature.";
            return false;
        }

        auto* pNtLocal = reinterpret_cast<const IMAGE_NT_HEADERS*>(pSrc + pDosLocal->e_lfanew);
        if (pNtLocal->Signature != IMAGE_NT_SIGNATURE) {
            outLog = "Error: Invalid PE signature.";
            return false;
        }

        auto* pOpt = &pNtLocal->OptionalHeader;

        // Architecture check
        bool isTarget64 = IsProcess64(pid);
        bool isDll64 = (pNtLocal->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
        if (isTarget64 != isDll64) {
            outLog = "Architecture mismatch! Target process is " + std::string(isTarget64 ? "64-bit" : "32-bit") +
                     ", but DLL is " + std::string(isDll64 ? "64-bit" : "32-bit") + ".";
            return false;
        }

        HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProc) {
            outLog = "Error: Failed to OpenProcess (PID " + std::to_string(pid) + "). Run loader as Administrator.";
            return false;
        }

        // Allocate target image memory
        LPVOID pTargetBase = VirtualAllocEx(hProc, nullptr, pOpt->SizeOfImage,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!pTargetBase) {
            CloseHandle(hProc);
            outLog = "Error: VirtualAllocEx failed in target process.";
            return false;
        }

        // Allocate local image buffer to map sections and relocations
        BYTE* pLocalImage = (BYTE*)VirtualAlloc(nullptr, pOpt->SizeOfImage,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pLocalImage) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: Local VirtualAlloc failed.";
            return false;
        }

        // Copy PE headers
        memcpy(pLocalImage, pSrc, pOpt->SizeOfHeaders);

        // Copy PE sections
        auto* pSection = IMAGE_FIRST_SECTION(pNtLocal);
        for (UINT i = 0; i < pNtLocal->FileHeader.NumberOfSections; ++i, ++pSection) {
            if (pSection->SizeOfRawData > 0) {
                memcpy(pLocalImage + pSection->VirtualAddress,
                    pSrc + pSection->PointerToRawData,
                    pSection->SizeOfRawData);
            }
        }

        // Apply Base Relocations
        RelocateImage(pLocalImage, (uintptr_t)pTargetBase);

        // Write mapped sections to target process
        if (!WriteProcessMemory(hProc, pTargetBase, pLocalImage, pOpt->SizeOfImage, nullptr)) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            VirtualFree(pLocalImage, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: WriteProcessMemory failed.";
            return false;
        }
        VirtualFree(pLocalImage, 0, MEM_RELEASE);
        pLocalImage = nullptr;

        // Resolve kernel32 function addresses
        HMODULE hK32Local = GetModuleHandleA("kernel32.dll");
        if (!hK32Local) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: Failed to get handle to kernel32.dll.";
            return false;
        }

        auto pRtlAddFuncTableLocal = (f_RtlAddFunctionTable)GetProcAddress(hK32Local, "RtlAddFunctionTable");
        auto pLoadLibraryALocal    = (f_LoadLibraryA)GetProcAddress(hK32Local, "LoadLibraryA");
        auto pGetProcAddressLocal  = (f_GetProcAddress)GetProcAddress(hK32Local, "GetProcAddress");

        if (!pLoadLibraryALocal || !pGetProcAddressLocal) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: Failed to resolve LoadLibraryA / GetProcAddress.";
            return false;
        }

        // Prepare Shellcode Data
        ShellcodeData data = {};
        data.pDllBase             = (uintptr_t)pTargetBase;
        data.EntryPoint           = pOpt->AddressOfEntryPoint;
        data.ImportDir            = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        data.RelocDir             = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
        data.ExceptionDir         = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
        data.ExceptionSize        = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
        data.TLSDir               = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress;
        data.Status               = InjectionStatus::Waiting;
        data.ErrorCode            = 0;
        data.pLoadLibraryA        = pLoadLibraryALocal;
        data.pGetProcAddress      = pGetProcAddressLocal;
        data.pRtlAddFunctionTable = pRtlAddFuncTableLocal;

        LPVOID pRemoteData = VirtualAllocEx(hProc, nullptr, sizeof(ShellcodeData),
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pRemoteData) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: Failed to allocate ShellcodeData in target process.";
            return false;
        }
        WriteProcessMemory(hProc, pRemoteData, &data, sizeof(ShellcodeData), nullptr);

        // Prepare Shellcode Code
        void* scStart = ResolveFunction((void*)UniversalShellcode);
        void* scEnd   = ResolveFunction((void*)UniversalShellcodeEnd);
        size_t shellcodeSize = (uintptr_t)scEnd - (uintptr_t)scStart;
        if (shellcodeSize == 0 || shellcodeSize > 0x8000) {
            shellcodeSize = 0x1000;
        }

        LPVOID pRemoteShellcode = VirtualAllocEx(hProc, nullptr, shellcodeSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!pRemoteShellcode) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            VirtualFreeEx(hProc, pRemoteData, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: Failed to allocate remote shellcode memory.";
            return false;
        }
        WriteProcessMemory(hProc, pRemoteShellcode, scStart, shellcodeSize, nullptr);

        // Execute remote thread
        HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
            (LPTHREAD_START_ROUTINE)pRemoteShellcode, pRemoteData, 0, nullptr);
        if (!hThread) {
            VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
            VirtualFreeEx(hProc, pRemoteData, 0, MEM_RELEASE);
            VirtualFreeEx(hProc, pRemoteShellcode, 0, MEM_RELEASE);
            CloseHandle(hProc);
            outLog = "Error: CreateRemoteThread failed.";
            return false;
        }

        // Wait for shellcode execution status
        InjectionStatus status = InjectionStatus::Waiting;
        uint32_t errorCode = 0;
        for (int i = 0; i < 1000; i++) {
            ReadProcessMemory(hProc,
                (LPCVOID)((uintptr_t)pRemoteData + offsetof(ShellcodeData, Status)),
                &status, sizeof(status), nullptr);
            if (status == InjectionStatus::Finished || status == InjectionStatus::Error) break;

            DWORD exitCode = STILL_ACTIVE;
            GetExitCodeThread(hThread, &exitCode);
            if (exitCode != STILL_ACTIVE) break;

            Sleep(10);
        }

        if (status == InjectionStatus::Error) {
            ReadProcessMemory(hProc,
                (LPCVOID)((uintptr_t)pRemoteData + offsetof(ShellcodeData, ErrorCode)),
                &errorCode, sizeof(errorCode), nullptr);
        }

        CloseHandle(hThread);
        // Clean up shellcode & parameters from target memory to leave zero traces
        VirtualFreeEx(hProc, pRemoteData, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, pRemoteShellcode, 0, MEM_RELEASE);
        CloseHandle(hProc);

        if (status == InjectionStatus::Finished) {
            outLog = "Manual Map Injection SUCCESS! Base: 0x" + 
                     [&]() {
                         std::stringstream ss;
                         ss << std::hex << (uintptr_t)pTargetBase;
                         return ss.str();
                     }();
            return true;
        }
        else {
            outLog = "Injection failed (Status: " + std::to_string((int)status) + 
                     ", ErrorCode: " + std::to_string(errorCode) + ").";
            return false;
        }
    }

    bool InjectDLL(DWORD pid, const std::string& dllPath, std::string& outLog) {
        std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            outLog = "Error: Unable to open DLL file at: " + dllPath;
            return false;
        }

        std::streamsize size = file.tellg();
        if (size < 0x1000) {
            outLog = "Error: File size too small to be a valid DLL.";
            return false;
        }

        std::vector<BYTE> buffer((size_t)size);
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            outLog = "Error: Failed to read DLL contents into buffer.";
            return false;
        }
        file.close();

        return InjectDLLFromMemory(pid, buffer, outLog);
    }

}
