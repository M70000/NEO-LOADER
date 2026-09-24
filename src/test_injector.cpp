#include "injection.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: test_injector.exe <process_name.exe> <path_to_dll>" << std::endl;
        return 1;
    }

    std::string procName = argv[1];
    std::string dllPath = argv[2];

    std::wstring procNameW(procName.begin(), procName.end());
    DWORD pid = Memory::GetPID(procNameW.c_str());

    if (pid == 0) {
        std::cout << "Target process " << procName << " is not running." << std::endl;
        return 1;
    }

    std::cout << "Found " << procName << " with PID " << pid << std::endl;
    std::cout << "Injecting " << dllPath << " via Manual Mapping..." << std::endl;

    std::string logOut;
    bool success = Memory::InjectDLL(pid, dllPath, logOut);

    if (success) {
        std::cout << "[SUCCESS] " << logOut << std::endl;
        return 0;
    } else {
        std::cout << "[FAILED] " << logOut << std::endl;
        return 2;
    }
}
