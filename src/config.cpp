#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

ConfigManager& ConfigManager::Get() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    SetDefaultProfiles();
}

void ConfigManager::SetDefaultProfiles() {
    m_settings.operatorName = "spawnyk1ng";
    m_settings.subscriptionExpiry = "Active - Lifetime";
    m_settings.autoCloseOnInject = true;
    m_settings.waitForProcess = false;
    m_settings.saveSelection = true;
    m_settings.autoCheckUpdates = false; // Do not block user at startup
    m_settings.activeProfileIndex = 0;
    m_settings.profiles.clear();

    TargetProfile cs2;
    cs2.name = "Counter-Strike 2";
    cs2.exeName = "cs2.exe";
    cs2.dllPath = "payloads/cs2_module.dll";
    cs2.githubRepo = "Paxai/DLLium";
    cs2.githubAsset = ".dll";
    cs2.directUrl = "";
    cs2.version = "Latest";
    cs2.statusText = "Undetected";
    m_settings.profiles.push_back(cs2);

    TargetProfile custom;
    custom.name = "Custom Program";
    custom.exeName = "notepad.exe";
    custom.dllPath = "payloads/test_payload.dll";
    custom.githubRepo = "";
    custom.githubAsset = ".dll";
    custom.directUrl = "";
    custom.version = "v1.0.0";
    custom.statusText = "Testing";
    m_settings.profiles.push_back(custom);
}

TargetProfile& ConfigManager::GetActiveProfile() {
    if (m_settings.profiles.empty()) {
        SetDefaultProfiles();
    }
    if (m_settings.activeProfileIndex < 0 || m_settings.activeProfileIndex >= (int)m_settings.profiles.size()) {
        m_settings.activeProfileIndex = 0;
    }
    return m_settings.profiles[m_settings.activeProfileIndex];
}

static std::string JsonEscape(const std::string& str) {
    std::string out;
    for (char c : str) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string ExtractJsonValue(const std::string& block, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = block.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();

    while (pos < block.length() && (block[pos] == ' ' || block[pos] == '\t' || block[pos] == '\r' || block[pos] == '\n')) pos++;

    if (pos >= block.length()) return "";

    if (block[pos] == '"') {
        size_t end = block.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return block.substr(pos + 1, end - pos - 1);
    }
    else {
        size_t end = block.find_first_of(",}\r\n", pos);
        if (end == std::string::npos) end = block.length();
        std::string val = block.substr(pos, end - pos);
        // trim
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        return val;
    }
}

bool ConfigManager::Load(const std::string& filePath) {
    if (!fs::exists(filePath)) {
        Save(filePath);
        return false;
    }

    std::ifstream in(filePath);
    if (!in.is_open()) return false;

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json = buffer.str();
    in.close();

    std::string activeIdx = ExtractJsonValue(json, "activeProfileIndex");
    if (!activeIdx.empty()) m_settings.activeProfileIndex = std::stoi(activeIdx);

    std::string autoClose = ExtractJsonValue(json, "autoCloseOnInject");
    if (!autoClose.empty()) m_settings.autoCloseOnInject = (autoClose == "true" || autoClose == "1");

    std::string waitProc = ExtractJsonValue(json, "waitForProcess");
    if (!waitProc.empty()) m_settings.waitForProcess = (waitProc == "true" || waitProc == "1");

    std::string saveSel = ExtractJsonValue(json, "saveSelection");
    if (!saveSel.empty()) m_settings.saveSelection = (saveSel == "true" || saveSel == "1");

    std::string autoCheck = ExtractJsonValue(json, "autoCheckUpdates");
    if (!autoCheck.empty()) m_settings.autoCheckUpdates = (autoCheck == "true" || autoCheck == "1");

    std::string opName = ExtractJsonValue(json, "operatorName");
    if (!opName.empty()) m_settings.operatorName = opName;

    std::string loaderRepo = ExtractJsonValue(json, "loaderGithubRepo");
    if (!loaderRepo.empty()) m_settings.loaderGithubRepo = loaderRepo;

    std::string loaderVer = ExtractJsonValue(json, "loaderVersion");
    if (!loaderVer.empty()) m_settings.loaderVersion = loaderVer;

    // Parse profiles array
    size_t profilesPos = json.find("\"profiles\":");
    if (profilesPos != std::string::npos) {
        size_t arrayStart = json.find('[', profilesPos);
        size_t arrayEnd = json.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t itemStart = 0;
            std::vector<TargetProfile> loadedProfiles;

            while ((itemStart = arrayContent.find('{', itemStart)) != std::string::npos) {
                size_t itemEnd = arrayContent.find('}', itemStart);
                if (itemEnd == std::string::npos) break;

                std::string itemBlock = arrayContent.substr(itemStart, itemEnd - itemStart + 1);
                TargetProfile p;
                p.name = ExtractJsonValue(itemBlock, "name");
                p.exeName = ExtractJsonValue(itemBlock, "exeName");
                p.dllPath = ExtractJsonValue(itemBlock, "dllPath");
                p.githubRepo = ExtractJsonValue(itemBlock, "githubRepo");
                p.githubAsset = ExtractJsonValue(itemBlock, "githubAsset");
                p.directUrl = ExtractJsonValue(itemBlock, "directUrl");
                p.version = ExtractJsonValue(itemBlock, "version");
                p.statusText = ExtractJsonValue(itemBlock, "statusText");

                if (!p.name.empty()) {
                    loadedProfiles.push_back(p);
                }

                itemStart = itemEnd + 1;
            }

            if (!loadedProfiles.empty()) {
                m_settings.profiles = loadedProfiles;
            }
        }
    }

    if (m_settings.activeProfileIndex >= (int)m_settings.profiles.size()) {
        m_settings.activeProfileIndex = 0;
    }

    return true;
}

bool ConfigManager::Save(const std::string& filePath) {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"operatorName\": \"" << JsonEscape(m_settings.operatorName) << "\",\n";
    out << "  \"activeProfileIndex\": " << m_settings.activeProfileIndex << ",\n";
    out << "  \"autoCloseOnInject\": " << (m_settings.autoCloseOnInject ? "true" : "false") << ",\n";
    out << "  \"waitForProcess\": " << (m_settings.waitForProcess ? "true" : "false") << ",\n";
    out << "  \"saveSelection\": " << (m_settings.saveSelection ? "true" : "false") << ",\n";
    out << "  \"autoCheckUpdates\": " << (m_settings.autoCheckUpdates ? "true" : "false") << ",\n";
    out << "  \"loaderGithubRepo\": \"" << JsonEscape(m_settings.loaderGithubRepo) << "\",\n";
    out << "  \"loaderVersion\": \"" << JsonEscape(m_settings.loaderVersion) << "\",\n";
    out << "  \"profiles\": [\n";

    for (size_t i = 0; i < m_settings.profiles.size(); ++i) {
        const auto& p = m_settings.profiles[i];
        out << "    {\n";
        out << "      \"name\": \"" << JsonEscape(p.name) << "\",\n";
        out << "      \"exeName\": \"" << JsonEscape(p.exeName) << "\",\n";
        out << "      \"dllPath\": \"" << JsonEscape(p.dllPath) << "\",\n";
        out << "      \"githubRepo\": \"" << JsonEscape(p.githubRepo) << "\",\n";
        out << "      \"githubAsset\": \"" << JsonEscape(p.githubAsset) << "\",\n";
        out << "      \"directUrl\": \"" << JsonEscape(p.directUrl) << "\",\n";
        out << "      \"version\": \"" << JsonEscape(p.version) << "\",\n";
        out << "      \"statusText\": \"" << JsonEscape(p.statusText) << "\"\n";
        out << "    }" << (i + 1 < m_settings.profiles.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";

    out.close();
    return true;
}
