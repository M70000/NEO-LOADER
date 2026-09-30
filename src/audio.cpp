#include "audio.h"

#include <windows.h>
#include <mmsystem.h>

#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace {

bool FileExistsW(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring ExeDir()
{
    wchar_t buf[MAX_PATH * 2] = {};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    if (n == 0) return L".";
    std::wstring s(buf, n);
    const size_t p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? std::wstring(L".") : s.substr(0, p);
}

std::wstring Cwd()
{
    wchar_t buf[MAX_PATH * 2] = {};
    const DWORD n = GetCurrentDirectoryW(MAX_PATH * 2, buf);
    if (n == 0) return L".";
    return std::wstring(buf, n);
}

// Search order: exe dir -> cwd -> exe dir's 3 parents.  This keeps the asset
// working whether the loader is launched from the project root or from bin\.
std::wstring FindSoundFile()
{
    std::vector<std::wstring> roots;
    roots.push_back(ExeDir());
    roots.push_back(Cwd());

    std::wstring up = ExeDir();
    for (int i = 0; i < 3; ++i) {
        const size_t p = up.find_last_of(L"\\/");
        if (p == std::wstring::npos || p < 2) break;
        up = up.substr(0, p);
        roots.push_back(up);
    }

    for (size_t i = 0; i < roots.size(); ++i) {
        std::wstring candidate = roots[i] + L"\\rezerosound.mp3";
        if (FileExistsW(candidate)) return candidate;
    }
    return std::wstring();
}

} // namespace

void NeoAudio::PlayStartupSound()
{
    const std::wstring path = FindSoundFile();
    if (path.empty()) return;

    mciSendStringW(L"stop neosnd", nullptr, 0, nullptr);
    mciSendStringW(L"close neosnd", nullptr, 0, nullptr);

    // Try the richest device first, then progressively simpler ones.
    static const wchar_t* kTypes[] = { L"mpegvideo", L"MPEGVideo", L"waveaudio", nullptr };
    wchar_t cmd[2048] = {};
    bool opened = false;
    for (int i = 0; kTypes[i] != nullptr && !opened; ++i) {
        swprintf_s(cmd, L"open \"%s\" type %s alias neosnd", path.c_str(), kTypes[i]);
        if (mciSendStringW(cmd, nullptr, 0, nullptr) == 0) opened = true;
    }
    if (!opened) {
        swprintf_s(cmd, L"open \"%s\" alias neosnd", path.c_str());
        if (mciSendStringW(cmd, nullptr, 0, nullptr) == 0) opened = true;
    }
    if (!opened) return;

    // Asynchronous by default; MCI keeps playing while the UI runs.
    mciSendStringW(L"play neosnd", nullptr, 0, nullptr);
}

void NeoAudio::Shutdown()
{
    mciSendStringW(L"stop neosnd", nullptr, 0, nullptr);
    mciSendStringW(L"close neosnd", nullptr, 0, nullptr);
}
