#pragma once

#include <string>
#include <vector>

struct TargetProfile {
    std::string name;           // Display name (e.g. "Counter-Strike 2", "Assault Cube", "Custom")
    std::string exeName;        // Target process (e.g. "cs2.exe", "ac_client.exe", "notepad.exe")
    std::string dllPath;        // Local path to payload DLL (e.g. "payloads/cs2.dll")
    std::string githubRepo;     // GitHub repository (e.g. "Paxai/DLLium" or "user/repo")
    std::string githubAsset;    // Asset name filter (e.g. ".dll")
    std::string directUrl;      // Optional direct URL fallback
    std::string version;        // Module version (e.g. "Latest", "v1.2.0")
    std::string statusText;     // Detection status (e.g. "Undetected", "Testing")
};

struct LoaderSettings {
    int activeProfileIndex = 0;
    bool autoCloseOnInject = true;
    bool waitForProcess = false;
    bool saveSelection = true;
    bool autoCheckUpdates = true;
    std::string operatorName = "spawnyk1ng";
    std::string subscriptionExpiry = "30 Days";
    std::string loaderGithubRepo = "M70000/NEO-LOADER";
    std::string loaderVersion = "v1.0.0";
    std::string githubToken = "";
    std::vector<TargetProfile> profiles;
};

class ConfigManager {
public:
    static ConfigManager& Get();

    bool Load(const std::string& filePath = "loader_config.json");
    bool Save(const std::string& filePath = "loader_config.json");

    LoaderSettings& Settings() { return m_settings; }
    TargetProfile& GetActiveProfile();

    void SetDefaultProfiles();

private:
    ConfigManager();
    LoaderSettings m_settings;
};
