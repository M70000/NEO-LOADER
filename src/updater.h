#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

enum class UpdaterState {
    Idle,
    Checking,
    Downloading,
    UpToDate,
    Updated,
    Failed
};

struct UpdateConfig {
    std::string githubRepo;       // e.g. "M70000/NeoNirvana" or "Paxai/DLLium"
    std::string assetPattern;     // e.g. ".dll" or "cs2_module.dll"
    std::string directUrl;        // optional direct URL fallback
    std::string localSavePath;    // e.g. "payloads/cs2_module.dll"
    std::string currentVersion;   // e.g. "v1.0.0"
    bool isSilent = true;         // If true, no modal is popped up
};

class DllUpdater {
public:
    DllUpdater();
    ~DllUpdater();

    // Silent background DLL check and update
    void CheckDllSilentAsync(const UpdateConfig& config);

    // Loader client update check (only triggers modal if newer version is found)
    void CheckLoaderUpdateAsync(const std::string& repo, const std::string& currentLoaderVersion);

    // Manual check trigger
    void CheckAndUpdateAsync(const UpdateConfig& config);

    void Cancel();

    UpdaterState GetState();
    float GetProgress();          // 0.0f to 1.0f
    std::string GetStatusMessage();
    std::string GetLatestVersion();
    bool IsBusy();
    bool HasNewLoaderUpdate();
    void DismissLoaderModal();

    static bool DownloadFile(const std::string& url, const std::string& destPath, 
                             std::atomic<float>& progress, std::atomic<bool>& cancelFlag, 
                             std::string& outError);

private:
    void WorkerThreadDll(UpdateConfig config);
    void WorkerThreadLoader(std::string repo, std::string currentVersion);

    std::atomic<UpdaterState> m_state;
    std::atomic<float> m_progress;
    std::atomic<bool> m_cancel;
    std::atomic<bool> m_isBusy;
    std::atomic<bool> m_hasNewLoaderUpdate;

    std::mutex m_mutex;
    std::string m_statusMsg;
    std::string m_latestVersion;
    std::thread m_worker;
};
