#include <Windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            MessageBoxA(NULL, "Manual Map Injection SUCCESS!\nLoaded cleanly into target process without LoadLibrary.\nEngine: VibeLoader Stealth Core", "VibeLoader - Manual Map", MB_OK | MB_ICONINFORMATION);
            return 0;
        }, nullptr, 0, nullptr);
    }
    return TRUE;
}
