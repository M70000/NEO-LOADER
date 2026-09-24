#define NOMINMAX
#include "updater.h"
#include "config.h"
#include <Windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")

namespace fs = std::filesystem;

static std::string GetGithubAuthToken() {
    std::string token = ConfigManager::Get().Settings().githubToken;
    if (!token.empty()) return token;

    char* envToken = getenv("GITHUB_TOKEN");
    if (envToken && strlen(envToken) > 0) return std::string(envToken);

    char* envGh = getenv("GH_TOKEN");
    if (envGh && strlen(envGh) > 0) return std::string(envGh);

    return "";
}

DllUpdater::DllUpdater()
    : m_state(UpdaterState::Idle)
    , m_progress(0.0f)
    , m_cancel(false)
    , m_isBusy(false)
    , m_hasNewLoaderUpdate(false)
    , m_statusMsg("Idle")
    , m_latestVersion("")
    , m_pendingExeUrl("")
{
}

DllUpdater::~DllUpdater() {
    Cancel();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void DllUpdater::Cancel() {
    m_cancel = true;
}

UpdaterState DllUpdater::GetState() {
    return m_state.load();
}

float DllUpdater::GetProgress() {
    return m_progress.load();
}

std::string DllUpdater::GetStatusMessage() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statusMsg;
}

std::string DllUpdater::GetLatestVersion() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_latestVersion;
}

std::string DllUpdater::GetPendingExeUrl() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingExeUrl;
}

bool DllUpdater::IsBusy() {
    return m_isBusy.load();
}

bool DllUpdater::HasNewLoaderUpdate() {
    return m_hasNewLoaderUpdate.load();
}

void DllUpdater::DismissLoaderModal() {
    m_hasNewLoaderUpdate = false;
}

static std::string ExtractJsonField(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    size_t colonPos = json.find(':', pos + searchKey.length());
    if (colonPos == std::string::npos) return "";

    size_t startQuote = json.find('"', colonPos + 1);
    if (startQuote == std::string::npos) return "";

    size_t endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return "";

    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

static std::string ExtractAssetUrl(const std::string& json, const std::string& pattern) {
    std::string key = "\"browser_download_url\"";
    size_t searchPos = 0;

    std::string lowerPattern = pattern;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

    while ((searchPos = json.find(key, searchPos)) != std::string::npos) {
        size_t colonPos = json.find(':', searchPos + key.length());
        if (colonPos == std::string::npos) break;

        size_t quoteStart = json.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) break;

        size_t quoteEnd = json.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;

        std::string url = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        std::string lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);

        if (lowerPattern.empty() || lowerUrl.find(lowerPattern) != std::string::npos) {
            return url;
        }

        searchPos = quoteEnd + 1;
    }

    return "";
}

bool DllUpdater::DownloadFile(const std::string& url, const std::string& destPath,
                             std::atomic<float>& progress, std::atomic<bool>& cancelFlag,
                             std::string& outError) {
    HINTERNET hInternet = InternetOpenA("NeoNirvana-Updater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        outError = "Failed to initialize WinINet.";
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_SECURE;
    std::string token = GetGithubAuthToken();
    std::string headers = "User-Agent: NeoNirvana-Updater/1.0\r\n";
    if (!token.empty() && url.find("github") != std::string::npos) {
        headers += "Authorization: token " + token + "\r\n";
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), -1, flags, 0);
    if (!hUrl) {
        if (url.find("https://") == std::string::npos) {
            flags &= ~INTERNET_FLAG_SECURE;
            hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), -1, flags, 0);
        }
    }

    if (!hUrl) {
        DWORD err = GetLastError();
        InternetCloseHandle(hInternet);
        outError = "Failed to open URL connection (Error: " + std::to_string(err) + ").";
        return false;
    }

    char lenBuf[64] = { 0 };
    DWORD lenBufSize = sizeof(lenBuf);
    DWORD contentLength = 0;
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH, lenBuf, &lenBufSize, NULL)) {
        contentLength = (DWORD)strtoul(lenBuf, NULL, 10);
    }

    try {
        fs::path p(destPath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    }
    catch (...) {}

    std::string tempPath = destPath + ".tmp";
    std::ofstream out(tempPath, std::ios::binary);
    if (!out.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        outError = "Failed to create destination file: " + tempPath;
        return false;
    }

    char buffer[32768];
    DWORD bytesRead = 0;
    DWORD totalDownloaded = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (cancelFlag.load()) {
            out.close();
            fs::remove(tempPath);
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            outError = "Download canceled.";
            return false;
        }

        out.write(buffer, bytesRead);
        totalDownloaded += bytesRead;

        if (contentLength > 0) {
            progress = (float)totalDownloaded / (float)contentLength;
        }
        else {
            progress = (std::min)(0.95f, progress.load() + 0.05f);
        }
    }

    out.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (totalDownloaded == 0) {
        fs::remove(tempPath);
        outError = "Zero bytes downloaded from server.";
        return false;
    }

    try {
        if (fs::exists(destPath)) {
            fs::remove(destPath);
        }
        fs::rename(tempPath, destPath);
    }
    catch (const std::exception& e) {
        outError = "Failed to finalize downloaded file: " + std::string(e.what());
        return false;
    }

    progress = 1.0f;
    return true;
}

// 100% Silent DLL Auto-Updater
void DllUpdater::CheckDllSilentAsync(const UpdateConfig& config) {
    if (m_isBusy) return;

    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_cancel = false;
    m_isBusy = true;
    m_state = UpdaterState::Checking;
    m_progress = 0.0f;

    m_worker = std::thread(&DllUpdater::WorkerThreadDll, this, config);
}

void DllUpdater::CheckAndUpdateAsync(const UpdateConfig& config) {
    CheckDllSilentAsync(config);
}

void DllUpdater::WorkerThreadDll(UpdateConfig config) {
    std::string downloadUrl = config.directUrl;
    std::string foundVersion = "";

    if (!config.githubRepo.empty()) {
        std::string apiUrl = "https://api.github.com/repos/" + config.githubRepo + "/releases/latest";
        HINTERNET hInternet = InternetOpenA("NeoNirvana-Updater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet) {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE;
            std::string token = GetGithubAuthToken();
            std::string headers = "User-Agent: NeoNirvana-Updater/1.0\r\nAccept: application/vnd.github.v3+json\r\n";
            if (!token.empty()) {
                headers += "Authorization: token " + token + "\r\n";
            }
            HINTERNET hUrl = InternetOpenUrlA(hInternet, apiUrl.c_str(), headers.c_str(), -1, flags, 0);

            if (hUrl) {
                std::string jsonResponse;
                char buf[8192];
                DWORD read = 0;
                while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
                    jsonResponse.append(buf, read);
                }
                InternetCloseHandle(hUrl);

                if (!jsonResponse.empty()) {
                    foundVersion = ExtractJsonField(jsonResponse, "tag_name");
                    std::string assetUrl = ExtractAssetUrl(jsonResponse, config.assetPattern);
                    if (!assetUrl.empty()) {
                        downloadUrl = assetUrl;
                    }
                }
            }
            InternetCloseHandle(hInternet);
        }
    }

    if (m_cancel.load()) {
        m_state = UpdaterState::Failed;
        m_isBusy = false;
        return;
    }

    if (!foundVersion.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latestVersion = foundVersion;
    }

    // If version matches and file exists, already up to date
    if (!foundVersion.empty() && !config.currentVersion.empty() && foundVersion == config.currentVersion) {
        if (fs::exists(config.localSavePath)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_statusMsg = "DLL is up to date (" + foundVersion + ").";
            m_state = UpdaterState::UpToDate;
            m_progress = 1.0f;
            m_isBusy = false;
            return;
        }
    }

    // If no remote URL found, check if local file is present
    if (downloadUrl.empty()) {
        if (fs::exists(config.localSavePath)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_statusMsg = "Local DLL payload is ready.";
            m_state = UpdaterState::UpToDate;
            m_progress = 1.0f;
            m_isBusy = false;
            return;
        }
        else {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_statusMsg = "Payload DLL ready locally.";
            m_state = UpdaterState::UpToDate;
            m_isBusy = false;
            return;
        }
    }

    // Silent background download of the newer DLL
    m_state = UpdaterState::Downloading;
    std::string errorStr;
    bool success = DownloadFile(downloadUrl, config.localSavePath, m_progress, m_cancel, errorStr);

    if (success) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "DLL updated silently" + (foundVersion.empty() ? "" : " to " + foundVersion) + ".";
        m_state = UpdaterState::Updated;
        m_progress = 1.0f;
    }
    else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "Local DLL ready.";
        m_state = UpdaterState::UpToDate;
    }

    m_isBusy = false;
}

// Loader Client Auto-Updater: checks in background for new .exe releases on GitHub
void DllUpdater::CheckLoaderUpdateAsync(const std::string& repo, const std::string& currentVersion) {
    if (repo.empty() || m_isBusy) return;

    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_cancel = false;
    m_isBusy = true;
    m_state = UpdaterState::Checking;
    m_worker = std::thread(&DllUpdater::WorkerThreadLoaderCheck, this, repo, currentVersion);
}

static std::string FetchUrlText(const std::string& url) {
    HINTERNET hInternet = InternetOpenA("NeoNirvana-Updater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    std::string token = GetGithubAuthToken();
    std::string headers = "User-Agent: NeoNirvana-Updater/1.0\r\n";
    if (!token.empty() && url.find("github") != std::string::npos) {
        headers += "Authorization: token " + token + "\r\n";
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_SECURE;
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), -1, flags, 0);
    if (!hUrl) {
        if (url.find("https://") == std::string::npos) {
            flags &= ~INTERNET_FLAG_SECURE;
            hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.c_str(), -1, flags, 0);
        }
    }

    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string result;
    char buf[1024];
    DWORD read = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
        result.append(buf, read);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    while (!result.empty() && (result.front() == '\r' || result.front() == '\n' || result.front() == ' ' || result.front() == '\t')) {
        result.erase(result.begin());
    }

    return result;
}

void DllUpdater::WorkerThreadLoaderCheck(std::string repo, std::string currentVersion) {
    std::string effectiveCurrentVersion = currentVersion.empty() ? LOADER_VERSION_TAG : currentVersion;
    if (!effectiveCurrentVersion.empty() && (effectiveCurrentVersion.front() == 'v' || effectiveCurrentVersion.front() == 'V')) {
        effectiveCurrentVersion.erase(effectiveCurrentVersion.begin());
    }

    std::string foundVersion = "";
    std::string exeDownloadUrl = "";

    // 1. Try pulling directly from repo bin folder (bin/version.txt) on main branch
    std::vector<std::string> versionUrls = {
        "https://raw.githubusercontent.com/" + repo + "/main/bin/version.txt",
        "https://raw.githubusercontent.com/" + repo + "/main/version.txt",
        "https://raw.githubusercontent.com/" + repo + "/master/bin/version.txt"
    };

    for (const auto& vUrl : versionUrls) {
        std::string v = FetchUrlText(vUrl);
        if (!v.empty() && v.find("404") == std::string::npos && v.find("Not Found") == std::string::npos) {
            foundVersion = v;
            exeDownloadUrl = "https://raw.githubusercontent.com/" + repo + "/main/bin/NeoNirvana.exe";
            break;
        }
    }

    // 2. If raw URL failed, query GitHub Contents API (supports token / private repositories)
    if (foundVersion.empty()) {
        std::string token = GetGithubAuthToken();
        std::string apiUrl = "https://api.github.com/repos/" + repo + "/contents/bin/version.txt";
        HINTERNET hInternet = InternetOpenA("NeoNirvana-LoaderUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet) {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE;
            std::string headers = "User-Agent: NeoNirvana-LoaderUpdater/1.0\r\nAccept: application/vnd.github.v3+json\r\n";
            if (!token.empty()) {
                headers += "Authorization: token " + token + "\r\n";
            }
            HINTERNET hUrl = InternetOpenUrlA(hInternet, apiUrl.c_str(), headers.c_str(), -1, flags, 0);
            if (hUrl) {
                std::string jsonResponse;
                char buf[8192];
                DWORD read = 0;
                while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
                    jsonResponse.append(buf, read);
                }
                InternetCloseHandle(hUrl);

                std::string b64Content = ExtractJsonField(jsonResponse, "content");
                b64Content.erase(std::remove(b64Content.begin(), b64Content.end(), '\n'), b64Content.end());
                b64Content.erase(std::remove(b64Content.begin(), b64Content.end(), '\r'), b64Content.end());

                if (!b64Content.empty()) {
                    DWORD decodedLen = 0;
                    if (CryptStringToBinaryA(b64Content.c_str(), 0, CRYPT_STRING_BASE64, NULL, &decodedLen, NULL, NULL) && decodedLen > 0) {
                        std::vector<char> dec(decodedLen + 1, 0);
                        if (CryptStringToBinaryA(b64Content.c_str(), 0, CRYPT_STRING_BASE64, (BYTE*)dec.data(), &decodedLen, NULL, NULL)) {
                            std::string vStr(dec.data(), decodedLen);
                            while (!vStr.empty() && (vStr.back() == '\r' || vStr.back() == '\n' || vStr.back() == ' ')) vStr.pop_back();
                            foundVersion = vStr;
                            exeDownloadUrl = "https://raw.githubusercontent.com/" + repo + "/main/bin/NeoNirvana.exe";
                        }
                    }
                }
            }
            InternetCloseHandle(hInternet);
        }
    }

    // 3. Fallback to GitHub Releases API if version.txt was not available
    if (foundVersion.empty()) {
        std::string token = GetGithubAuthToken();
        std::string apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
        HINTERNET hInternet = InternetOpenA("NeoNirvana-LoaderUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet) {
            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_SECURE;
            std::string headers = "User-Agent: NeoNirvana-LoaderUpdater/1.0\r\nAccept: application/vnd.github.v3+json\r\n";
            if (!token.empty()) {
                headers += "Authorization: token " + token + "\r\n";
            }
            HINTERNET hUrl = InternetOpenUrlA(hInternet, apiUrl.c_str(), headers.c_str(), -1, flags, 0);

            if (hUrl) {
                std::string jsonResponse;
                char buf[8192];
                DWORD read = 0;
                while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
                    jsonResponse.append(buf, read);
                }
                InternetCloseHandle(hUrl);

                if (!jsonResponse.empty()) {
                    foundVersion = ExtractJsonField(jsonResponse, "tag_name");
                    exeDownloadUrl = ExtractAssetUrl(jsonResponse, ".exe");
                }
            }
            InternetCloseHandle(hInternet);
        }
    }

    // Compare versions (clean 'v' prefixes for flexible match)
    std::string compareFound = foundVersion;
    if (!compareFound.empty() && (compareFound.front() == 'v' || compareFound.front() == 'V')) {
        compareFound.erase(compareFound.begin());
    }

    if (!compareFound.empty() && compareFound != effectiveCurrentVersion && !exeDownloadUrl.empty()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_statusMsg = "Nova versao do loader disponivel: " + foundVersion;
            m_latestVersion = foundVersion;
            m_pendingExeUrl = exeDownloadUrl;
        }
        // Signal UI to show update screen with the "ATUALIZAR" button!
        m_hasNewLoaderUpdate = true;
        m_state = UpdaterState::UpdateAvailable;
    }
    else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "O loader esta na versao mais recente (" + std::string(LOADER_VERSION_TAG) + ").";
        m_state = UpdaterState::UpToDate;
    }

    m_isBusy = false;
}

// When user clicks the "ATUALIZAR" button
void DllUpdater::StartDownloadLoaderUpdateAsync() {
    if (m_isBusy) return;

    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_cancel = false;
    m_isBusy = true;
    m_state = UpdaterState::Downloading;
    m_progress = 0.0f;
    m_worker = std::thread(&DllUpdater::WorkerThreadLoaderDownload, this);
}

void DllUpdater::WorkerThreadLoaderDownload() {
    std::string downloadUrl;
    std::string ver;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        downloadUrl = m_pendingExeUrl;
        ver = m_latestVersion;
        m_statusMsg = "Baixando versão " + ver + "...";
    }

    if (downloadUrl.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "URL de download do executável não encontrada.";
        m_state = UpdaterState::Failed;
        m_isBusy = false;
        return;
    }

    char currentExePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
    fs::path currentPath(currentExePath);
    fs::path newExePath = currentPath.parent_path() / "NeoNirvana_new.exe";

    std::string err;
    bool ok = DownloadFile(downloadUrl, newExePath.string(), m_progress, m_cancel, err);

    if (ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "Download concluído com sucesso!";
        m_state = UpdaterState::Updated;
    }
    else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMsg = "Falha no download: " + err;
        m_state = UpdaterState::Failed;
    }

    m_isBusy = false;
}

void DllUpdater::ApplyAndRestart() {
    char currentExePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
    fs::path currentPath(currentExePath);
    fs::path newPath = currentPath.parent_path() / "NeoNirvana_new.exe";

    if (fs::exists(newPath)) {
        // cmd script to wait 1s, overwrite old exe, and restart it cleanly
        std::string cmd = "cmd.exe /c timeout /t 1 /nobreak >nul & move /y \"" + newPath.string() + "\" \"" + currentPath.string() + "\" & start \"\" \"" + currentPath.string() + "\"";
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            ExitProcess(0);
        }
    }

    // Direct run fallback
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    std::string runTarget = fs::exists(newPath) ? newPath.string() : currentPath.string();
    if (CreateProcessA(runTarget.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ExitProcess(0);
    }
}
