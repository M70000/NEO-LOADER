#include "ui.h"
#include <commdlg.h>
#include <shobjidl.h>
#include <cmath>
#include <algorithm>
#include <chrono>

VibeUI& VibeUI::Get() {
    static VibeUI instance;
    return instance;
}

VibeUI::VibeUI() {
    memset(m_bufProcessName, 0, sizeof(m_bufProcessName));
    memset(m_bufDllPath, 0, sizeof(m_bufDllPath));
    memset(m_bufGithubRepo, 0, sizeof(m_bufGithubRepo));
    memset(m_bufDirectUrl, 0, sizeof(m_bufDirectUrl));
    memset(m_bufProcessSearch, 0, sizeof(m_bufProcessSearch));
}

void VibeUI::Initialize() {
    SetupStyles();

    ConfigManager::Get().Load();
    auto& profile = ConfigManager::Get().GetActiveProfile();

    strcpy_s(m_bufProcessName, sizeof(m_bufProcessName), profile.exeName.c_str());
    strcpy_s(m_bufDllPath, sizeof(m_bufDllPath), profile.dllPath.c_str());
    strcpy_s(m_bufGithubRepo, sizeof(m_bufGithubRepo), profile.githubRepo.c_str());
    strcpy_s(m_bufDirectUrl, sizeof(m_bufDirectUrl), profile.directUrl.c_str());

    m_cachedProcessList = Memory::GetProcessList();

    // Trigger 100% silent background DLL check on startup
    if (!profile.githubRepo.empty()) {
        UpdateConfig cfg;
        cfg.githubRepo = profile.githubRepo;
        cfg.assetPattern = profile.githubAsset.empty() ? ".dll" : profile.githubAsset;
        cfg.directUrl = profile.directUrl;
        cfg.localSavePath = profile.dllPath;
        cfg.currentVersion = profile.version;
        cfg.isSilent = true;
        m_updater.CheckDllSilentAsync(cfg);
    }

    // Check loader update in background silently (only shows modal if a newer loader version is found)
    auto& settings = ConfigManager::Get().Settings();
    if (!settings.loaderGithubRepo.empty()) {
        m_updater.CheckLoaderUpdateAsync(settings.loaderGithubRepo, settings.loaderVersion);
    }

    m_showUpdateModal = false;
    m_showProcessPicker = false;
}

void VibeUI::SetupStyles() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 8.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(20.0f, 16.0f);
    style.FramePadding      = ImVec2(10.0f, 7.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);

    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.043f, 0.051f, 0.067f, 0.98f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.000f, 0.000f, 0.000f, 0.00f); 
    colors[ImGuiCol_PopupBg]              = ImVec4(0.055f, 0.063f, 0.082f, 0.98f); 
    colors[ImGuiCol_Border]               = ImVec4(0.200f, 0.235f, 0.314f, 0.70f); 
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.60f);

    colors[ImGuiCol_FrameBg]              = ImVec4(0.075f, 0.086f, 0.118f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.130f, 0.150f, 0.205f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.170f, 0.195f, 0.265f, 1.00f);

    colors[ImGuiCol_TitleBg]              = ImVec4(0.035f, 0.039f, 0.051f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.043f, 0.051f, 0.067f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.035f, 0.039f, 0.051f, 0.80f);

    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.043f, 0.051f, 0.067f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.035f, 0.039f, 0.051f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.180f, 0.208f, 0.278f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.850f, 0.120f, 0.240f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(1.000f, 0.169f, 0.294f, 1.00f);

    colors[ImGuiCol_CheckMark]            = ImVec4(1.000f, 0.169f, 0.294f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(1.000f, 0.169f, 0.294f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(1.000f, 0.250f, 0.380f, 1.00f);

    colors[ImGuiCol_Button]               = ImVec4(0.118f, 0.137f, 0.188f, 0.90f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.750f, 0.100f, 0.200f, 0.85f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.950f, 0.150f, 0.280f, 1.00f);

    colors[ImGuiCol_Header]               = ImVec4(0.149f, 0.173f, 0.235f, 0.80f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.700f, 0.090f, 0.180f, 0.60f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.900f, 0.130f, 0.250f, 0.90f);

    colors[ImGuiCol_Separator]            = ImVec4(0.180f, 0.208f, 0.278f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(1.000f, 0.169f, 0.294f, 0.70f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(1.000f, 0.169f, 0.294f, 1.00f);

    colors[ImGuiCol_Text]                 = ImVec4(0.950f, 0.960f, 0.980f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.480f, 0.520f, 0.600f, 1.00f);
}

void VibeUI::DrawRadialGlow(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, int steps) {
    float r = (float)((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = (float)((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = (float)((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    float baseAlpha = (float)((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;

    for (int i = steps; i >= 1; --i) {
        float factor = (float)i / (float)steps;
        float currentRadius = radius * factor;
        float alpha = baseAlpha * (1.0f - factor * 0.85f) / (float)steps;
        drawList->AddCircleFilled(center, currentRadius, ImColor(r, g, b, alpha), 36);
    }
}

void VibeUI::DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float glowSize, int passes) {
    float r = (float)((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = (float)((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = (float)((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    float baseAlpha = (float)((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;

    for (int i = passes; i >= 1; --i) {
        float factor = (float)i / (float)passes;
        float expand = glowSize * factor;
        float alpha = baseAlpha * (1.0f - factor * 0.75f) / (float)passes;

        ImU32 passCol = ImColor(r, g, b, alpha);
        drawList->AddRect(
            ImVec2(min.x - expand, min.y - expand),
            ImVec2(max.x + expand, max.y + expand),
            passCol,
            rounding + expand * 0.4f,
            0,
            1.5f + expand * 0.35f
        );
    }
}

void VibeUI::Draw3DCard(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 topBg, ImU32 botBg, ImU32 borderColor, float rounding, bool withGlow, ImU32 glowCol, float glowIntensity) {
    // 1. Triple-pass ambient drop shadow
    drawList->AddRectFilled(
        ImVec2(min.x + 2, min.y + 3),
        ImVec2(max.x + 2, max.y + 5),
        IM_COL32(0, 0, 0, 80),
        rounding
    );
    drawList->AddRectFilled(
        ImVec2(min.x - 1, min.y + 7),
        ImVec2(max.x + 1, max.y + 14),
        IM_COL32(0, 0, 0, 45),
        rounding + 2.0f
    );

    // 2. Volumetric perimeter neon red glow
    if (withGlow && glowIntensity > 0.0f) {
        ImU32 glow = (glowCol != 0) ? glowCol : IM_COL32(255, 43, 75, 45);
        DrawGlow(drawList, min, max, glow, rounding, 8.0f * glowIntensity, 4);
    }

    // 3. Card Surface 3D Vertical Gradient
    drawList->AddRectFilledMultiColor(min, max, topBg, topBg, botBg, botBg);

    // 4. Specular Top Highlight Line
    drawList->AddLine(
        ImVec2(min.x + rounding, min.y + 1.2f),
        ImVec2(max.x - rounding, min.y + 1.2f),
        IM_COL32(255, 255, 255, 45),
        1.0f
    );
    drawList->AddLine(
        ImVec2(min.x + rounding, min.y + 1.2f),
        ImVec2(min.x + rounding + 75.0f, min.y + 1.2f),
        IM_COL32(255, 80, 110, 150),
        1.5f
    );

    // 5. 3D Bottom inset shadow
    drawList->AddLine(
        ImVec2(min.x + rounding, max.y - 1.2f),
        ImVec2(max.x - rounding, max.y - 1.2f),
        IM_COL32(0, 0, 0, 100),
        1.0f
    );

    // 6. Crisp Outer Border
    drawList->AddRect(min, max, borderColor, rounding, 0, 1.2f);
}

bool VibeUI::GlowingButton(const char* label, ImVec2 size, ImU32 baseCol, ImU32 hoverCol, ImU32 glowCol, bool pulse, bool isPrimary) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 min = p;
    ImVec2 max = ImVec2(p.x + size.x, p.y + size.y);
    float rounding = 8.0f;

    // Use transparent button to capture mouse interaction cleanly
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

    bool clicked = ImGui::Button((std::string("##Btn_") + label).c_str(), size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    float time = (float)ImGui::GetTime();
    float pulseAlpha = 0.5f + 0.5f * sinf(time * 3.8f);

    // Glowing Bloom Effect
    if (hovered || pulse || isPrimary) {
        ImU32 currentGlow = glowCol;
        float glowAlphaMult = hovered ? 1.5f : (pulse ? (0.7f + 0.5f * pulseAlpha) : 1.0f);
        
        float r = (float)((glowCol >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
        float g = (float)((glowCol >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
        float b = (float)((glowCol >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
        float a = (float)((glowCol >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
        float mult = hovered ? 1.5f : (pulse ? (0.6f + 0.5f * pulseAlpha) : 1.0f);
        currentGlow = ImColor(r, g, b, std::clamp(a * mult, 0.0f, 1.0f));

        DrawGlow(drawList, min, max, currentGlow, rounding, (hovered || isPrimary) ? 12.0f : 6.0f, 4);
    }

    // 3D Button Surface Gradient
    if (isPrimary) {
        ImU32 topCol = hovered ? IM_COL32(255, 60, 95, 255) : IM_COL32(235, 30, 70, 255);
        ImU32 botCol = active ? IM_COL32(170, 15, 45, 255) : (hovered ? IM_COL32(200, 20, 50, 255) : IM_COL32(165, 15, 42, 255));
        drawList->AddRectFilledMultiColor(min, max, topCol, topCol, botCol, botCol);
    }
    else {
        ImU32 bg = active ? IM_COL32(200, 25, 55, 255) : (hovered ? hoverCol : baseCol);
        drawList->AddRectFilled(min, max, bg, rounding);
    }

    // Top Specular Highlight
    drawList->AddLine(
        ImVec2(min.x + rounding, min.y + 1.2f),
        ImVec2(max.x - rounding, min.y + 1.2f),
        IM_COL32(255, 255, 255, (hovered || isPrimary) ? 120 : 60),
        1.2f
    );

    // Border
    ImU32 borderC = hovered ? IM_COL32(255, 75, 105, 255) : (isPrimary ? IM_COL32(255, 43, 75, 220) : IM_COL32(60, 72, 98, 200));
    drawList->AddRect(min, max, borderC, rounding, 0, 1.2f);

    // Centered Text
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos = ImVec2(
        min.x + (size.x - textSize.x) * 0.5f,
        min.y + (size.y - textSize.y) * 0.5f
    );
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
    if (m_fontBold) ImGui::PopFont();

    return clicked;
}

void VibeUI::RenderBackgroundEffects(ImDrawList* drawList, ImVec2 winSize) {
    drawList->PushClipRect(ImVec2(0, 0), winSize, true);

    // Atmospheric Red Glowing Aura behind Header & Logo
    DrawRadialGlow(drawList, ImVec2(140.0f, 80.0f), 280.0f, IM_COL32(255, 35, 68, 45), 18);

    // Subtle Ruby Ambient Light
    DrawRadialGlow(drawList, ImVec2(winSize.x - 120.0f, winSize.y - 80.0f), 240.0f, IM_COL32(220, 20, 50, 28), 14);

    // Subtle Cyber Grid pattern bounded cleanly inside window
    for (float x = 20.0f; x < winSize.x; x += 40.0f) {
        drawList->AddLine(ImVec2(x, 0), ImVec2(x, winSize.y), IM_COL32(255, 255, 255, 5), 1.0f);
    }
    for (float y = 20.0f; y < winSize.y; y += 40.0f) {
        drawList->AddLine(ImVec2(0, y), ImVec2(winSize.x, y), IM_COL32(255, 255, 255, 5), 1.0f);
    }

    drawList->PopClipRect();
}

void VibeUI::OpenDllFileDialog() {
    IFileOpenDialog* pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC rgSpec[] = { 
            { L"Dynamic Link Libraries (*.dll)", L"*.dll" }, 
            { L"All Files (*.*)", L"*.*" } 
        };
        pFileOpen->SetFileTypes(2, rgSpec);
        pFileOpen->SetTitle(L"Select Module DLL to Inject");
        hr = pFileOpen->Show(NULL);

        if (SUCCEEDED(hr)) {
            IShellItem* pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    char mbPath[MAX_PATH];
                    size_t converted = 0;
                    wcstombs_s(&converted, mbPath, pszFilePath, MAX_PATH);
                    strcpy_s(m_bufDllPath, sizeof(m_bufDllPath), mbPath);

                    auto& profile = ConfigManager::Get().GetActiveProfile();
                    profile.dllPath = mbPath;
                    ConfigManager::Get().Save();

                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
}

void VibeUI::TriggerInjection() {
    if (m_isInjecting) return;

    std::wstring exeW(m_bufProcessName, m_bufProcessName + strlen(m_bufProcessName));
    DWORD pid = Memory::GetPID(exeW.c_str());

    if (pid == 0) {
        if (ConfigManager::Get().Settings().waitForProcess) {
            m_lastInjectLog = "Waiting for process: " + std::string(m_bufProcessName);
            m_injectSuccess = false;
            m_injectMessageTimer = 8.0f;
            return;
        }
        else {
            m_lastInjectLog = "Process not running: " + std::string(m_bufProcessName) + ". Start the program first.";
            m_injectSuccess = false;
            m_injectMessageTimer = 6.0f;
            return;
        }
    }

    std::string dllFile = m_bufDllPath;
    if (dllFile.empty()) {
        m_lastInjectLog = "Error: Please select a payload DLL file.";
        m_injectSuccess = false;
        m_injectMessageTimer = 6.0f;
        return;
    }

    m_isInjecting = true;
    m_lastInjectLog = "Mapping PE sections into PID " + std::to_string(pid) + "...";

    std::thread([this, pid, dllFile]() {
        std::string logOut;
        bool ok = Memory::InjectDLL(pid, dllFile, logOut);

        m_injectSuccess = ok;
        m_lastInjectLog = logOut;
        m_injectMessageTimer = 8.0f;
        m_isInjecting = false;

        if (ok && ConfigManager::Get().Settings().autoCloseOnInject) {
            Sleep(800);
            this->shouldClose = true;
        }
    }).detach();
}

void VibeUI::TriggerUpdateCheck() {
    // Manual sync check for DLL from GitHub
    auto& profile = ConfigManager::Get().GetActiveProfile();
    UpdateConfig cfg;
    cfg.githubRepo = m_bufGithubRepo[0] ? m_bufGithubRepo : profile.githubRepo;
    cfg.assetPattern = profile.githubAsset.empty() ? ".dll" : profile.githubAsset;
    cfg.directUrl = m_bufDirectUrl[0] ? m_bufDirectUrl : profile.directUrl;
    cfg.localSavePath = m_bufDllPath[0] ? m_bufDllPath : profile.dllPath;
    cfg.currentVersion = profile.version;
    cfg.isSilent = true;

    m_lastInjectLog = "Checking GitHub repository for latest payload...";
    m_injectMessageTimer = 5.0f;
    m_updater.CheckDllSilentAsync(cfg);
}

void VibeUI::Render() {
    // Process poll every 500ms
    m_procCheckTimer += ImGui::GetIO().DeltaTime;
    if (m_procCheckTimer >= 0.5f) {
        m_procCheckTimer = 0.0f;
        std::wstring exeW(m_bufProcessName, m_bufProcessName + strlen(m_bufProcessName));
        m_cachedTargetPid = Memory::GetPID(exeW.c_str());
        m_cachedTargetRunning = (m_cachedTargetPid != 0);

        if (m_cachedTargetRunning && ConfigManager::Get().Settings().waitForProcess && !m_isInjecting && m_lastInjectLog.find("Waiting") != std::string::npos) {
            TriggerInjection();
        }
    }

    if (m_injectMessageTimer > 0.0f) {
        m_injectMessageTimer -= ImGui::GetIO().DeltaTime;
    }

    // Auto-open modal ONLY IF a new loader update was actually discovered
    if (m_updater.HasNewLoaderUpdate()) {
        m_showUpdateModal = true;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    
    if (m_fontRegular) ImGui::PushFont(m_fontRegular);
    ImGui::Begin("##MainWindow", nullptr, winFlags);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    RenderBackgroundEffects(drawList, ImGui::GetWindowSize());

    RenderTitleBar();
    RenderHeaderBanner();

    ImGui::Spacing();

    // Middle Area: Left Column + Right Column
    // Calculate exact height so middle area NEVER overlaps the footer
    float topAreaHeight = 118.0f;
    float footerHeight = 60.0f;
    float middleHeight = ImGui::GetWindowHeight() - topAreaHeight - footerHeight - 16.0f; // ~386px

    // Left Column
    ImGui::BeginChild("##LeftColumn", ImVec2(245, middleHeight), false, ImGuiWindowFlags_NoBackground);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine(0, 16.0f);

    // Right Column
    float rightWidth = ImGui::GetWindowWidth() - 245.0f - 56.0f;
    ImGui::BeginChild("##RightColumn", ImVec2(rightWidth, middleHeight), false, ImGuiWindowFlags_NoBackground);
    RenderRightPanel();
    ImGui::EndChild();

    // Footer Bar (Clean, unobstructed row at the bottom)
    RenderFooterBar();

    // Modals
    if (m_showUpdateModal) {
        RenderUpdateModal();
    }
    if (m_showProcessPicker) {
        RenderProcessPickerModal();
    }

    ImGui::End();
    if (m_fontRegular) ImGui::PopFont();
    ImGui::PopStyleVar();
}

void VibeUI::RenderTitleBar() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float width = ImGui::GetWindowWidth() - 40.0f;

    // Glowing Neon Crimson Dot
    drawList->AddCircleFilled(ImVec2(p.x + 8, p.y + 12), 4.5f, IM_COL32(255, 43, 75, 255));
    DrawGlow(drawList, ImVec2(p.x + 4, p.y + 8), ImVec2(p.x + 12, p.y + 16), IM_COL32(255, 43, 75, 180), 4.5f, 6.0f, 3);

    // Title: NeoNirvana (No slashes!)
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    drawList->AddText(ImVec2(p.x + 22, p.y + 2), IM_COL32(255, 255, 255, 255), "NEONIRVANA");
    if (m_fontBold) ImGui::PopFont();

    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
    drawList->AddText(ImVec2(p.x + 130, p.y + 5), IM_COL32(150, 160, 180, 255), "STEALTH MANUAL MAP CORE");
    if (m_fontSmall) ImGui::PopFont();

    // Window controls
    ImGui::SetCursorPosX(width - 56.0f);
    if (ImGui::Button("-##Minimize", ImVec2(24, 22))) {
        HWND hWnd = GetActiveWindow();
        ShowWindow(hWnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.1f, 0.2f, 0.9f));
    if (ImGui::Button("X##Close", ImVec2(24, 22))) {
        shouldClose = true;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
}

void VibeUI::RenderHeaderBanner() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth() - 40.0f;
    float height = 66.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 3D Card for Header
    Draw3DCard(drawList, p, ImVec2(p.x + width, p.y + height), 
               IM_COL32(30, 36, 50, 255), IM_COL32(16, 20, 29, 255),
               IM_COL32(255, 43, 75, 140), 12.0f, true, IM_COL32(255, 43, 75, 55), 1.0f);

    // Left Branding: NeoNirvana
    if (m_fontTitle) ImGui::PushFont(m_fontTitle);
    drawList->AddText(ImVec2(p.x + 20, p.y + 12), IM_COL32(255, 43, 75, 255), "NeoNirvana");
    if (m_fontTitle) ImGui::PopFont();

    if (m_fontBold) ImGui::PushFont(m_fontBold);
    drawList->AddText(ImVec2(p.x + 165, p.y + 16), IM_COL32(255, 255, 255, 255), "ELITE EDITION");
    if (m_fontBold) ImGui::PopFont();

    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
    drawList->AddText(ImVec2(p.x + 20, p.y + 42), IM_COL32(170, 180, 200, 255), "ADVANCED STEALTH INJECTION ENGINE");

    // Right Status Badge
    float statusX = p.x + width - 160.0f;
    drawList->AddCircleFilled(ImVec2(statusX, p.y + 32), 4.5f, IM_COL32(0, 245, 140, 255));
    DrawGlow(drawList, ImVec2(statusX - 4, p.y + 28), ImVec2(statusX + 4, p.y + 36), IM_COL32(0, 245, 140, 180), 4.5f, 5.0f, 2);
    drawList->AddText(ImVec2(statusX + 14, p.y + 24), IM_COL32(0, 245, 140, 255), "SYSTEM ONLINE");
    if (m_fontSmall) ImGui::PopFont();

    ImGui::Dummy(ImVec2(width, height));
}

void VibeUI::RenderLeftPanel() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Left Panel 3D Card
    Draw3DCard(drawList, p, ImVec2(p.x + size.x, p.y + size.y), 
               IM_COL32(25, 30, 42, 255), IM_COL32(14, 17, 24, 255),
               IM_COL32(52, 62, 86, 200), 10.0f, true, IM_COL32(255, 43, 75, 35), 0.8f);

    // Padding child inside the card to prevent any border leaking
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
    ImGui::BeginChild("##LeftPanelChild", size, false, ImGuiWindowFlags_NoBackground);

    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Profiles");
    if (m_fontBold) ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 4.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8.0f));

    auto& settings = ConfigManager::Get().Settings();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
    for (int i = 0; i < (int)settings.profiles.size(); ++i) {
        bool isSelected = (settings.activeProfileIndex == i);
        
        std::string itemLabel = settings.profiles[i].name;
        if (ImGui::Selectable(itemLabel.c_str(), isSelected, 0, ImVec2(0, 32.0f))) {
            settings.activeProfileIndex = i;
            auto& activeP = ConfigManager::Get().GetActiveProfile();
            strcpy_s(m_bufProcessName, sizeof(m_bufProcessName), activeP.exeName.c_str());
            strcpy_s(m_bufDllPath, sizeof(m_bufDllPath), activeP.dllPath.c_str());
            strcpy_s(m_bufGithubRepo, sizeof(m_bufGithubRepo), activeP.githubRepo.c_str());
            strcpy_s(m_bufDirectUrl, sizeof(m_bufDirectUrl), activeP.directUrl.c_str());
            ConfigManager::Get().Save();
        }
    }
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 10.0f));
    if (GlowingButton("+ Add Custom Profile", ImVec2(ImGui::GetContentRegionAvail().x, 32), 
                      IM_COL32(32, 38, 54, 255), IM_COL32(180, 25, 55, 255), IM_COL32(255, 43, 75, 75))) {
        TargetProfile customP;
        customP.name = "Custom Game " + std::to_string(settings.profiles.size() + 1);
        customP.exeName = "game.exe";
        customP.dllPath = "payloads/custom.dll";
        customP.version = "v1.0";
        customP.statusText = "Undetected";
        settings.profiles.push_back(customP);
        settings.activeProfileIndex = (int)settings.profiles.size() - 1;
        ConfigManager::Get().Save();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void VibeUI::RenderRightPanel() {
    float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // --- CARD 1: Target Program Information ---
    ImVec2 card1P = ImGui::GetCursorScreenPos();
    float card1H = 136.0f;
    Draw3DCard(drawList, card1P, ImVec2(card1P.x + width, card1P.y + card1H),
               IM_COL32(28, 33, 46, 255), IM_COL32(15, 18, 26, 255),
               IM_COL32(56, 66, 92, 200), 10.0f, true, IM_COL32(255, 43, 75, 35), 0.7f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
    ImGui::BeginChild("##Card1Child", ImVec2(width, card1H), false, ImGuiWindowFlags_NoBackground);

    // Row 1: Header + PID Status
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Game Information");
    if (m_fontBold) ImGui::PopFont();

    ImGui::SameLine(width - 180.0f);
    if (m_cachedTargetRunning) {
        ImGui::TextColored(ImVec4(0.0f, 0.95f, 0.55f, 1.0f), "Running [PID: %d]", m_cachedTargetPid);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f), "Waiting for Game...");
    }

    ImGui::Dummy(ImVec2(0, 8.0f));

    // Row 2: Label
    ImGui::TextColored(ImVec4(0.72f, 0.77f, 0.88f, 1.0f), "Target Executable:");

    ImGui::Dummy(ImVec2(0, 4.0f));

    // Row 3: InputText + Select Process
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 145.0f);
    if (ImGui::InputText("##TargetExe", m_bufProcessName, sizeof(m_bufProcessName))) {
        auto& p = ConfigManager::Get().GetActiveProfile();
        p.exeName = m_bufProcessName;
        ConfigManager::Get().Save();
    }
    ImGui::SameLine(0, 10.0f);
    if (GlowingButton("Select Process", ImVec2(135, 26), IM_COL32(38, 46, 64, 255), IM_COL32(190, 25, 60, 255), IM_COL32(255, 43, 75, 80))) {
        m_cachedProcessList = Memory::GetProcessList();
        m_showProcessPicker = true;
    }

    ImGui::Dummy(ImVec2(0, 10.0f));

    // Row 4: Architecture Info
    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Architecture: x64  |  Injection: Pure Manual Mapping  |  Privileges: SE_DEBUG");
    if (m_fontSmall) ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 14.0f));

    // --- CARD 2: Module Information (Clean, generous spacing, no repo input) ---
    ImVec2 card2P = ImGui::GetCursorScreenPos();
    float card2H = 136.0f;
    Draw3DCard(drawList, card2P, ImVec2(card2P.x + width, card2P.y + card2H),
               IM_COL32(28, 33, 46, 255), IM_COL32(15, 18, 26, 255),
               IM_COL32(56, 66, 92, 200), 10.0f, true, IM_COL32(255, 43, 75, 35), 0.7f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
    ImGui::BeginChild("##Card2Child", ImVec2(width, card2H), false, ImGuiWindowFlags_NoBackground);

    // Row 1: Header + Undetected Status
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Module Information");
    if (m_fontBold) ImGui::PopFont();

    ImGui::SameLine(width - 160.0f);
    ImGui::TextColored(ImVec4(0.0f, 0.95f, 0.55f, 1.0f), "Status: Undetected");

    ImGui::Dummy(ImVec2(0, 8.0f));

    // Row 2: DLL Path label
    ImGui::TextColored(ImVec4(0.72f, 0.77f, 0.88f, 1.0f), "DLL Payload Path:");

    ImGui::Dummy(ImVec2(0, 4.0f));

    // Row 3: DLL Path input + Browse button
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
    if (ImGui::InputText("##DllPath", m_bufDllPath, sizeof(m_bufDllPath))) {
        auto& p = ConfigManager::Get().GetActiveProfile();
        p.dllPath = m_bufDllPath;
        ConfigManager::Get().Save();
    }
    ImGui::SameLine(0, 10.0f);
    if (GlowingButton("Browse...", ImVec2(100, 26), IM_COL32(38, 46, 64, 255), IM_COL32(190, 25, 60, 255), IM_COL32(255, 43, 75, 80))) {
        OpenDllFileDialog();
    }

    ImGui::Dummy(ImVec2(0, 10.0f));

    // Row 4: Clean module details & auto-update status
    auto& p = ConfigManager::Get().GetActiveProfile();
    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
    ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "Version: %s  |  Status: Ready  |  Active Profile: %s", 
                       p.version.c_str(), p.name.c_str());
    if (m_fontSmall) ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 14.0f));

    // Options Row
    auto& settings = ConfigManager::Get().Settings();
    if (ImGui::Checkbox("Save profile selection", &settings.saveSelection)) {
        ConfigManager::Get().Save();
    }
    ImGui::SameLine(240.0f);
    if (ImGui::Checkbox("Auto-close loader after load", &settings.autoCloseOnInject)) {
        ConfigManager::Get().Save();
    }
}

void VibeUI::RenderFooterBar() {
    float width = ImGui::GetWindowWidth() - 40.0f;
    ImGui::SetCursorPos(ImVec2(20.0f, ImGui::GetWindowHeight() - 56.0f));

    // Status Indicator with glowing bullet
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 statusDotCol = m_injectSuccess ? IM_COL32(0, 245, 140, 255) : (m_isInjecting ? IM_COL32(255, 185, 30, 255) : IM_COL32(255, 43, 75, 255));
    drawList->AddCircleFilled(ImVec2(p.x + 8, p.y + 20), 4.5f, statusDotCol);
    DrawGlow(drawList, ImVec2(p.x + 4, p.y + 16), ImVec2(p.x + 12, p.y + 24), statusDotCol, 4.5f, 5.0f, 2);

    ImGui::SetCursorPos(ImVec2(36.0f, ImGui::GetWindowHeight() - 48.0f));
    if (m_isInjecting) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Injecting payload...");
    }
    else if (m_injectMessageTimer > 0.0f) {
        ImVec4 col = m_injectSuccess ? ImVec4(0.0f, 0.95f, 0.55f, 1.0f) : ImVec4(1.0f, 0.25f, 0.35f, 1.0f);
        ImGui::TextColored(col, m_lastInjectLog.c_str());
    }
    else {
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.80f, 1.0f), "Engine Ready. Target configured.");
    }

    // LOAD button aligned to right edge with zero obstruction
    ImGui::SetCursorPos(ImVec2(width - 170.0f, ImGui::GetWindowHeight() - 58.0f));

    // Big Glowing 3D LOAD button
    if (GlowingButton("LOAD", ImVec2(170, 42), IM_COL32(225, 25, 60, 255), IM_COL32(255, 45, 80, 255), IM_COL32(255, 43, 75, 220), true, true)) {
        TriggerInjection();
    }
}

// Modal for Automatic Loader Updates (Appears ONLY if new loader version exists on GitHub)
void VibeUI::RenderUpdateModal() {
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    ImVec2 modalSize(480, 190);
    ImVec2 modalPos((dispSize.x - modalSize.x) * 0.5f, (dispSize.y - modalSize.y) * 0.5f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Dark backdrop overlay
    drawList->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 215));

    // 3D Card for Update Modal
    Draw3DCard(drawList, modalPos, ImVec2(modalPos.x + modalSize.x, modalPos.y + modalSize.y),
               IM_COL32(32, 38, 54, 255), IM_COL32(18, 22, 31, 255), 
               IM_COL32(255, 43, 75, 220), 12.0f, true, IM_COL32(255, 43, 75, 130), 1.5f);

    // Place ImGui cursor inside modal
    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 22, modalPos.y + 20));
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Updating NeoNirvana Client...");
    if (m_fontBold) ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 22, modalPos.y + 50));
    std::string msg = m_updater.GetStatusMessage();
    ImGui::TextColored(ImVec4(0.80f, 0.85f, 0.95f, 1.0f), msg.c_str());

    // Glowing Animated Progress Bar
    float barX = modalPos.x + 22.0f;
    float barY = modalPos.y + 84.0f;
    float barW = modalSize.x - 44.0f;
    float barH = 16.0f;

    drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(14, 16, 22, 255), 7.0f);
    drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(55, 65, 88, 255), 7.0f);

    float progress = m_updater.GetProgress();
    float fillW = barW * std::clamp(progress, 0.05f, 1.0f);
    drawList->AddRectFilledMultiColor(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
                                      IM_COL32(255, 60, 95, 255), IM_COL32(255, 60, 95, 255),
                                      IM_COL32(200, 20, 50, 255), IM_COL32(200, 20, 50, 255));
    DrawGlow(drawList, ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH), 
             IM_COL32(255, 43, 75, 160), 7.0f, 6.0f, 3);

    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + modalSize.x - 135.0f, modalPos.y + 128.0f));
    bool isFinished = (!m_updater.IsBusy() && m_updater.GetState() != UpdaterState::Checking && m_updater.GetState() != UpdaterState::Downloading);

    if (GlowingButton(isFinished ? "Restart" : "Dismiss", ImVec2(115, 34), 
                      IM_COL32(42, 50, 70, 255), IM_COL32(200, 25, 60, 255), IM_COL32(255, 43, 75, 100))) {
        if (!isFinished) {
            m_updater.Cancel();
        }
        m_showUpdateModal = false;
        m_updater.DismissLoaderModal();
    }
}

// Running Process Browser Modal
void VibeUI::RenderProcessPickerModal() {
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    ImVec2 modalSize(540, 460);
    ImVec2 modalPos((dispSize.x - modalSize.x) * 0.5f, (dispSize.y - modalSize.y) * 0.5f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 215));

    Draw3DCard(drawList, modalPos, ImVec2(modalPos.x + modalSize.x, modalPos.y + modalSize.y),
               IM_COL32(32, 38, 54, 255), IM_COL32(18, 22, 31, 255), 
               IM_COL32(255, 43, 75, 220), 12.0f, true, IM_COL32(255, 43, 75, 130), 1.5f);

    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 20, modalPos.y + 18));
    ImGui::BeginChild("##PickerContent", ImVec2(modalSize.x - 40, modalSize.y - 36), false, ImGuiWindowFlags_NoBackground);

    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Select Running Target Process");
    if (m_fontBold) ImGui::PopFont();
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(350);
    ImGui::InputTextWithHint("##FilterProc", "Search process name...", m_bufProcessSearch, sizeof(m_bufProcessSearch));
    ImGui::SameLine(0, 10.0f);
    if (GlowingButton("Refresh", ImVec2(110, 26), IM_COL32(38, 46, 64, 255), IM_COL32(190, 25, 60, 255), IM_COL32(255, 43, 75, 80))) {
        m_cachedProcessList = Memory::GetProcessList();
    }

    ImGui::Spacing();

    // Process List
    ImGui::BeginChild("##ProcListScroll", ImVec2(modalSize.x - 40, 290), true);

    std::string search = m_bufProcessSearch;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    for (const auto& proc : m_cachedProcessList) {
        char nameBuf[260] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, proc.name.c_str(), -1, nameBuf, sizeof(nameBuf), NULL, NULL);
        std::string nameA = nameBuf;
        std::string lowerName = nameA;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (!search.empty() && lowerName.find(search) == std::string::npos) continue;

        char archBuf[32] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, proc.arch.c_str(), -1, archBuf, sizeof(archBuf), NULL, NULL);

        std::string item = nameA + "  [PID: " + std::to_string(proc.pid) + " | " + archBuf + "]";
        if (ImGui::Selectable(item.c_str(), false, 0, ImVec2(0, 24))) {
            strcpy_s(m_bufProcessName, sizeof(m_bufProcessName), nameA.c_str());
            auto& p = ConfigManager::Get().GetActiveProfile();
            p.exeName = nameA;
            ConfigManager::Get().Save();
            m_showProcessPicker = false;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::SetCursorPosX(modalSize.x - 40 - 120);
    if (GlowingButton("Close", ImVec2(120, 32), IM_COL32(42, 50, 70, 255), IM_COL32(190, 25, 55, 255), IM_COL32(255, 43, 75, 90))) {
        m_showProcessPicker = false;
    }

    ImGui::EndChild();
}
