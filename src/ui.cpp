#define NOMINMAX
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
        m_updater.CheckLoaderUpdateAsync(settings.loaderGithubRepo, LOADER_VERSION_TAG);
    }

    m_showUpdateModal = false;
    m_showProcessPicker = false;
    m_showProfilePicker = false;
    m_isWaitingForGame = false;
}

void VibeUI::SetupStyles() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(24.0f, 16.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(12.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);

    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;

    // Primordial Palette: Dark obsidian matte charcoal with soft rose and cyan accents
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.063f, 0.067f, 0.082f, 1.00f); // #101115
    colors[ImGuiCol_ChildBg]              = ImVec4(0.090f, 0.094f, 0.118f, 1.00f); // #17181e
    colors[ImGuiCol_PopupBg]              = ImVec4(0.082f, 0.086f, 0.106f, 0.98f); 
    colors[ImGuiCol_Border]               = ImVec4(0.180f, 0.190f, 0.235f, 0.70f); // #2e303c
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    colors[ImGuiCol_FrameBg]              = ImVec4(0.075f, 0.078f, 0.098f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.120f, 0.125f, 0.157f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.150f, 0.157f, 0.196f, 1.00f);

    colors[ImGuiCol_TitleBg]              = ImVec4(0.055f, 0.059f, 0.071f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.063f, 0.067f, 0.082f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.055f, 0.059f, 0.071f, 1.00f);

    colors[ImGuiCol_CheckMark]            = ImVec4(0.933f, 0.557f, 0.643f, 1.00f); // Primordial Rose #ee8ea4
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.933f, 0.557f, 0.643f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(1.000f, 0.650f, 0.720f, 1.00f);

    colors[ImGuiCol_Button]               = ImVec4(0.094f, 0.098f, 0.125f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.150f, 0.157f, 0.196f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.190f, 0.200f, 0.250f, 1.00f);

    colors[ImGuiCol_Header]               = ImVec4(0.130f, 0.137f, 0.173f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.180f, 0.190f, 0.235f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.220f, 0.230f, 0.285f, 1.00f);

    colors[ImGuiCol_Separator]            = ImVec4(0.180f, 0.190f, 0.235f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.933f, 0.557f, 0.643f, 0.80f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.933f, 0.557f, 0.643f, 1.00f);

    colors[ImGuiCol_Text]                 = ImVec4(0.920f, 0.925f, 0.940f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.530f, 0.550f, 0.620f, 1.00f);
}

// Draw the authentic Primordial Hourglass Logo
void VibeUI::DrawHourglassLogo(ImDrawList* drawList, ImVec2 center, float size, ImU32 outlineColor, ImU32 sandColor) {
    float r = size * 0.21f;
    float topCy = center.y - size * 0.22f;
    float botCy = center.y + size * 0.22f;
    float stroke = (std::max)(1.6f, size * 0.045f);

    // Top loop
    drawList->AddCircle(ImVec2(center.x, topCy), r, outlineColor, 28, stroke);

    // Bottom loop
    drawList->AddCircle(ImVec2(center.x, botCy), r, outlineColor, 28, stroke);

    // Sand cone inside the bottom loop
    float sandTopY = center.y + size * 0.08f;
    float sandBaseY = botCy + r * 0.70f;
    float sandHalfW = r * 0.72f;

    ImVec2 sandPoints[3] = {
        ImVec2(center.x, sandTopY),
        ImVec2(center.x - sandHalfW, sandBaseY),
        ImVec2(center.x + sandHalfW, sandBaseY)
    };
    drawList->AddConvexPolyFilled(sandPoints, 3, sandColor);

    // Neck intersection accent
    drawList->AddLine(ImVec2(center.x - r * 0.35f, center.y), ImVec2(center.x + r * 0.35f, center.y), outlineColor, stroke);
}

// Draw the signature Primordial gradient underline
void VibeUI::DrawGradientUnderline(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 colLeft, ImU32 colRight) {
    float width = end.x - start.x;
    float halfW = width * 0.5f;
    ImVec2 mid(start.x + halfW, start.y);

    drawList->AddRectFilledMultiColor(start, ImVec2(mid.x, start.y + 1.5f), colLeft, colRight, colRight, colLeft);
    drawList->AddRectFilledMultiColor(mid, ImVec2(end.x, start.y + 1.5f), colRight, colLeft, colLeft, colRight);
}

// Flat card with subtle border (No glow on cards, as requested)
void VibeUI::DrawFlatCard(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 bgCol, ImU32 borderCol, float rounding) {
    drawList->AddRectFilled(min, max, bgCol, rounding);
    drawList->AddRect(min, max, borderCol, rounding, 0, 1.0f);
}

// Smooth bloom glow for the LOAD button
void VibeUI::DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float glowSize, int passes) {
    float r = (float)((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
    float g = (float)((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
    float b = (float)((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
    float baseAlpha = (float)((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;

    for (int i = passes; i >= 1; --i) {
        float factor = (float)i / (float)passes;
        float expand = glowSize * factor;
        float alpha = baseAlpha * (1.0f - factor * 0.75f) / (float)passes;
        ImVec2 gMin(min.x - expand, min.y - expand);
        ImVec2 gMax(max.x + expand, max.y + expand);
        drawList->AddRectFilled(gMin, gMax, ImColor(r, g, b, alpha), rounding + expand * 0.5f);
    }
}

// Render text with smooth vertical gradient across vertex buffer
void VibeUI::DrawGradientText(ImDrawList* drawList, ImFont* font, float fontSize, ImVec2 pos, const char* text, ImU32 colTop, ImU32 colBottom) {
    if (!text || !*text) return;

    ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text) : ImGui::CalcTextSize(text);
    float minY = pos.y;
    float maxY = pos.y + textSize.y;

    int vtx_start = drawList->VtxBuffer.Size;
    if (font) {
        drawList->AddText(font, fontSize, pos, IM_COL32_WHITE, text);
    } else {
        drawList->AddText(pos, IM_COL32_WHITE, text);
    }
    int vtx_end = drawList->VtxBuffer.Size;

    float r1 = (float)((colTop >> IM_COL32_R_SHIFT) & 0xFF);
    float g1 = (float)((colTop >> IM_COL32_G_SHIFT) & 0xFF);
    float b1 = (float)((colTop >> IM_COL32_B_SHIFT) & 0xFF);
    float a1 = (float)((colTop >> IM_COL32_A_SHIFT) & 0xFF);

    float r2 = (float)((colBottom >> IM_COL32_R_SHIFT) & 0xFF);
    float g2 = (float)((colBottom >> IM_COL32_G_SHIFT) & 0xFF);
    float b2 = (float)((colBottom >> IM_COL32_B_SHIFT) & 0xFF);
    float a2 = (float)((colBottom >> IM_COL32_A_SHIFT) & 0xFF);

    float height = (maxY - minY) > 0.0f ? (maxY - minY) : 1.0f;

    for (int i = vtx_start; i < vtx_end; ++i) {
        ImDrawVert& vert = drawList->VtxBuffer[i];
        float t = std::clamp((vert.pos.y - minY) / height, 0.0f, 1.0f);
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);
        int a = (int)(a1 + (a2 - a1) * t);
        vert.col = IM_COL32(r, g, b, a);
    }
}

// Render "NEO NIRVANA" with purple gradient on "NE"
void VibeUI::RenderBrandedTitle(ImDrawList* drawList, ImVec2 pos) {
    ImFont* font = m_fontTitle ? m_fontTitle : m_fontBold;
    float fontSize = font ? font->LegacySize : 24.0f;

    float curX = pos.x;

    // 1. "NE" with purple gradient (Electric violet to deep royal purple)
    ImU32 purpleTop = IM_COL32(215, 125, 255, 255);
    ImU32 purpleBottom = IM_COL32(135, 45, 240, 255);
    DrawGradientText(drawList, font, fontSize, ImVec2(curX, pos.y), "NE", purpleTop, purpleBottom);
    curX += font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NE").x : ImGui::CalcTextSize("NE").x;

    // 2. "O " in soft lavender white
    drawList->AddText(font, fontSize, ImVec2(curX, pos.y), IM_COL32(245, 238, 255, 255), "O ");
    curX += font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "O ").x : ImGui::CalcTextSize("O ").x;

    // 3. "NIRVANA" in signature pastel rose
    drawList->AddText(font, fontSize, ImVec2(curX, pos.y), IM_COL32(238, 142, 164, 255), "NIRVANA");
}

// Primordial Button (Clean flat style with glowing LOAD button)
bool VibeUI::PrimordialButton(const char* label, ImVec2 size, bool isPrimary, bool withGlow) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 min = p;
    ImVec2 max = ImVec2(p.x + size.x, p.y + size.y);
    float rounding = 6.0f;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

    bool clicked = ImGui::Button((std::string("##Btn_") + label).c_str(), size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Glowing bloom under the LOAD button
    if (isPrimary && withGlow) {
        float time = (float)ImGui::GetTime();
        float pulse = 0.8f + 0.2f * sinf(time * 3.5f);
        ImU32 glowCol = hovered ? IM_COL32(238, 142, 164, 180) : IM_COL32(238, 142, 164, (int)(110 * pulse));
        DrawGlow(drawList, min, max, glowCol, rounding, hovered ? 12.0f : 8.0f, 4);
    }

    // Button body
    ImU32 bgCol;
    ImU32 borderCol;
    ImU32 textCol = IM_COL32(235, 235, 240, 255);

    if (isPrimary) {
        bgCol = active ? IM_COL32(36, 38, 48, 255) : (hovered ? IM_COL32(28, 30, 38, 255) : IM_COL32(18, 20, 26, 255));
        borderCol = hovered ? IM_COL32(255, 165, 185, 255) : IM_COL32(238, 142, 164, 200);
        textCol = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(245, 175, 195, 255);
    }
    else {
        bgCol = active ? IM_COL32(32, 34, 42, 255) : (hovered ? IM_COL32(24, 26, 32, 255) : IM_COL32(18, 19, 24, 255));
        borderCol = hovered ? IM_COL32(80, 85, 105, 255) : IM_COL32(45, 48, 60, 200);
    }

    drawList->AddRectFilled(min, max, bgCol, rounding);
    drawList->AddRect(min, max, borderCol, rounding, 0, 1.0f);

    // Center text
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f);
    drawList->AddText(textPos, textCol, label);

    return clicked;
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
        m_isWaitingForGame = true;
        m_lastInjectLog = "waiting for game....";
        m_injectSuccess = false;
        m_injectMessageTimer = 10.0f;
        return;
    }

    m_isInjecting = true;
    m_isWaitingForGame = false;
    m_lastInjectLog = "injecting module....";
    m_injectMessageTimer = 6.0f;

    std::string dllFile = m_bufDllPath;

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
    auto& profile = ConfigManager::Get().GetActiveProfile();
    UpdateConfig cfg;
    cfg.githubRepo = profile.githubRepo;
    cfg.assetPattern = profile.githubAsset.empty() ? ".dll" : profile.githubAsset;
    cfg.directUrl = profile.directUrl;
    cfg.localSavePath = profile.dllPath;
    cfg.currentVersion = profile.version;
    cfg.isSilent = true;

    m_lastInjectLog = "Checking for payload updates...";
    m_injectMessageTimer = 4.0f;
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

        if (m_cachedTargetRunning && m_isWaitingForGame && !m_isInjecting) {
            TriggerInjection();
        }
    }

    if (m_injectMessageTimer > 0.0f) {
        m_injectMessageTimer -= ImGui::GetIO().DeltaTime;
    }

    // Auto-open modal ONLY IF a new loader update was actually discovered on GitHub
    if (m_updater.HasNewLoaderUpdate()) {
        m_showUpdateModal = true;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 16.0f));
    
    if (m_fontRegular) ImGui::PushFont(m_fontRegular);
    ImGui::Begin("##MainWindow", nullptr, winFlags);

    RenderTopBar();

    if (m_isWaitingForGame) {
        RenderWaitingForGameScreen();
    }
    else {
        RenderPrimordialDashboard();
    }

    // Modals
    if (m_showUpdateModal) {
        RenderUpdateModal();
    }
    if (m_showProcessPicker) {
        RenderProcessPickerModal();
    }
    if (m_showProfilePicker) {
        RenderProfilePickerModal();
    }

    ImGui::End();
    if (m_fontRegular) ImGui::PopFont();
    ImGui::PopStyleVar();
}

void VibeUI::RenderTopBar() {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth() - 48.0f;

    // Window controls at top right (Clean rounded buttons)
    ImGui::SetCursorPos(ImVec2(width - 24.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.22f, 0.28f, 0.6f));
    if (ImGui::Button("-##Minimize", ImVec2(24, 22))) {
        HWND hWnd = GetActiveWindow();
        ShowWindow(hWnd, SW_MINIMIZE);
    }
    ImGui::SameLine(0, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.3f, 0.8f));
    if (ImGui::Button("X##Close", ImVec2(24, 22))) {
        shouldClose = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
}

// Main Dashboard inspired faithfully by Primordial
void VibeUI::RenderPrimordialDashboard() {
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 1. Centered Header: Hourglass Logo + "NEO NIRVANA" (purple gradient on "NE")
    float centerX = winSize.x * 0.5f;
    float headerY = 54.0f;

    ImFont* font = m_fontTitle ? m_fontTitle : m_fontBold;
    float fontSize = font ? font->LegacySize : 24.0f;
    float titleWidth = (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NE").x : 32.0f)
                     + (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "O ").x : 20.0f)
                     + (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NIRVANA").x : 100.0f);

    float logoSize = 28.0f;
    float gap = 14.0f;
    float totalHeaderW = logoSize + gap + titleWidth;
    float startX = centerX - totalHeaderW * 0.5f;

    // Hourglass Logo (28px height) with purple-rose sand
    ImVec2 logoCenter(startX + logoSize * 0.5f, headerY + 14.0f);
    DrawHourglassLogo(drawList, logoCenter, logoSize, IM_COL32(230, 232, 240, 255), IM_COL32(185, 95, 245, 255));

    // Title: NEO NIRVANA
    RenderBrandedTitle(drawList, ImVec2(startX + logoSize + gap, headerY));

    // 2. Signature Gradient Underline
    float lineY = headerY + 44.0f;
    float lineWidth = winSize.x - 96.0f;
    float lineStartX = 48.0f;
    DrawGradientUnderline(drawList, ImVec2(lineStartX, lineY), ImVec2(lineStartX + lineWidth, lineY),
                          IM_COL32(238, 142, 164, 220), IM_COL32(140, 212, 198, 200));

    // 3. Four Flat Information Blocks (2x2 Grid)
    float gridTopY = lineY + 22.0f;
    float gridW = winSize.x - 96.0f;
    float colW = (gridW - 20.0f) * 0.5f;
    float rowH = 68.0f;

    auto& activeProfile = ConfigManager::Get().GetActiveProfile();

    // --- Block 1: Target Profile (Top-Left) ---
    ImVec2 b1Min(lineStartX, gridTopY);
    ImVec2 b1Max(lineStartX + colW, gridTopY + rowH);
    DrawFlatCard(drawList, b1Min, b1Max, IM_COL32(22, 23, 29, 255), IM_COL32(38, 40, 50, 200), 8.0f);

    // Profile icon
    drawList->AddCircle(ImVec2(b1Min.x + 24.0f, b1Min.y + 24.0f), 7.0f, IM_COL32(160, 165, 180, 255), 16, 1.4f);
    drawList->AddText(ImVec2(b1Min.x + 42.0f, b1Min.y + 14.0f), IM_COL32(150, 155, 170, 255), "Account ID:");
    drawList->AddText(ImVec2(b1Min.x + 42.0f, b1Min.y + 36.0f), IM_COL32(238, 142, 164, 255), activeProfile.name.c_str());

    // Clickable button on Block 1 to switch profile
    ImGui::SetCursorScreenPos(ImVec2(b1Max.x - 72.0f, b1Min.y + 18.0f));
    if (PrimordialButton("Switch", ImVec2(58, 26), false, false)) {
        m_showProfilePicker = true;
    }

    // --- Block 2: Last Update (Top-Right) ---
    ImVec2 b2Min(lineStartX + colW + 20.0f, gridTopY);
    ImVec2 b2Max(lineStartX + gridW, gridTopY + rowH);
    DrawFlatCard(drawList, b2Min, b2Max, IM_COL32(22, 23, 29, 255), IM_COL32(38, 40, 50, 200), 8.0f);

    // Lightbulb / Clock icon
    drawList->AddCircleFilled(ImVec2(b2Min.x + 24.0f, b2Min.y + 24.0f), 6.0f, IM_COL32(238, 142, 164, 220));
    drawList->AddText(ImVec2(b2Min.x + 42.0f, b2Min.y + 14.0f), IM_COL32(150, 155, 170, 255), "Loader Version:");
    drawList->AddText(ImVec2(b2Min.x + 42.0f, b2Min.y + 36.0f), IM_COL32(235, 235, 240, 255), ("v" + std::string(LOADER_VERSION_TAG)).c_str());

    // Clickable Check button on Block 2 to re-check GitHub
    ImGui::SetCursorScreenPos(ImVec2(b2Max.x - 72.0f, b2Min.y + 18.0f));
    if (PrimordialButton("Check", ImVec2(58, 26), false, false)) {
        auto& s = ConfigManager::Get().Settings();
        if (!s.loaderGithubRepo.empty()) {
            m_updater.CheckLoaderUpdateAsync(s.loaderGithubRepo, LOADER_VERSION_TAG);
        }
    }

    // --- Block 3: Cheat Status (Bottom-Left) ---
    ImVec2 b3Min(lineStartX, gridTopY + rowH + 12.0f);
    ImVec2 b3Max(lineStartX + colW, gridTopY + rowH * 2.0f + 12.0f);
    DrawFlatCard(drawList, b3Min, b3Max, IM_COL32(22, 23, 29, 255), IM_COL32(38, 40, 50, 200), 8.0f);

    // Shield checkmark icon
    drawList->AddCircle(ImVec2(b3Min.x + 24.0f, b3Min.y + 24.0f), 7.0f, IM_COL32(0, 235, 140, 255), 16, 1.4f);
    drawList->AddText(ImVec2(b3Min.x + 42.0f, b3Min.y + 14.0f), IM_COL32(150, 155, 170, 255), "Cheat status:");
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    drawList->AddText(ImVec2(b3Min.x + 42.0f, b3Min.y + 36.0f), IM_COL32(238, 142, 164, 255), "UNDETECTED");
    if (m_fontBold) ImGui::PopFont();

    // Process select button on Block 3
    ImGui::SetCursorScreenPos(ImVec2(b3Max.x - 72.0f, b3Min.y + 18.0f));
    if (PrimordialButton("Process", ImVec2(58, 26), false, false)) {
        m_cachedProcessList = Memory::GetProcessList();
        m_showProcessPicker = true;
    }

    // --- Block 4: Sub expires in / DLL payload (Bottom-Right) ---
    ImVec2 b4Min(lineStartX + colW + 20.0f, gridTopY + rowH + 12.0f);
    ImVec2 b4Max(lineStartX + gridW, gridTopY + rowH * 2.0f + 12.0f);
    DrawFlatCard(drawList, b4Min, b4Max, IM_COL32(22, 23, 29, 255), IM_COL32(38, 40, 50, 200), 8.0f);

    // Document icon
    drawList->AddRect(ImVec2(b4Min.x + 18.0f, b4Min.y + 16.0f), ImVec2(b4Min.x + 28.0f, b4Min.y + 30.0f), IM_COL32(160, 165, 180, 255), 2.0f, 0, 1.4f);
    drawList->AddText(ImVec2(b4Min.x + 42.0f, b4Min.y + 14.0f), IM_COL32(150, 155, 170, 255), "Sub expires in:");
    drawList->AddText(ImVec2(b4Min.x + 42.0f, b4Min.y + 36.0f), IM_COL32(235, 235, 240, 255), "LIFETIME (Active)");

    // Browse DLL button on Block 4
    ImGui::SetCursorScreenPos(ImVec2(b4Max.x - 72.0f, b4Min.y + 18.0f));
    if (PrimordialButton("Browse", ImVec2(58, 26), false, false)) {
        OpenDllFileDialog();
    }

    // 4. Options Row (Clean flat checkboxes)
    float optY = gridTopY + rowH * 2.0f + 30.0f;
    ImGui::SetCursorScreenPos(ImVec2(lineStartX + 10.0f, optY));

    auto& settings = ConfigManager::Get().Settings();
    if (ImGui::Checkbox("Save configuration", &settings.saveSelection)) {
        ConfigManager::Get().Save();
    }
    ImGui::SameLine(0, 28.0f);
    if (ImGui::Checkbox("Auto-close loader after load", &settings.autoCloseOnInject)) {
        ConfigManager::Get().Save();
    }

    // 5. Centered LOAD Button with subtle Primordial rose glow
    float btnW = 190.0f;
    float btnH = 36.0f;
    float btnX = centerX - btnW * 0.5f;
    float btnY = optY + 36.0f;

    ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
    if (m_fontBold) ImGui::PushFont(m_fontBold);
    if (PrimordialButton("LOAD", ImVec2(btnW, btnH), true, true)) {
        TriggerInjection();
    }
    if (m_fontBold) ImGui::PopFont();

    // 6. Sub-status line
    float statusY = btnY + btnH + 10.0f;
    std::string displayStatus = m_injectMessageTimer > 0.0f ? m_lastInjectLog : (m_cachedTargetRunning ? ("Target detected: " + std::string(m_bufProcessName)) : "Engine Ready. Target configured.");
    ImVec2 statusTextSize = ImGui::CalcTextSize(displayStatus.c_str());
    ImVec2 statusPos(centerX - statusTextSize.x * 0.5f, statusY);
    ImU32 statusCol = m_injectSuccess ? IM_COL32(0, 235, 140, 255) : IM_COL32(140, 145, 160, 255);
    drawList->AddText(statusPos, statusCol, displayStatus.c_str());
}

// Authentic Primordial "waiting for game...." state screen
void VibeUI::RenderWaitingForGameScreen() {
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float centerX = winSize.x * 0.5f;
    float centerY = winSize.y * 0.42f;

    // Animated Hourglass Logo in center (size: 64px)
    float time = (float)ImGui::GetTime();
    float pulse = 0.85f + 0.15f * sinf(time * 3.0f);
    ImU32 sandCol = IM_COL32(238, 142, 164, (int)(255 * pulse));

    DrawHourglassLogo(drawList, ImVec2(centerX, centerY - 25.0f), 64.0f, IM_COL32(235, 235, 245, 255), sandCol);

    // Title: NEO NIRVANA
    ImFont* font = m_fontTitle ? m_fontTitle : m_fontBold;
    float fontSize = font ? font->LegacySize : 24.0f;
    float titleWidth = (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NE").x : 32.0f)
                     + (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "O ").x : 20.0f)
                     + (font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, "NIRVANA").x : 100.0f);

    RenderBrandedTitle(drawList, ImVec2(centerX - titleWidth * 0.5f, centerY + 30.0f));

    // Gradient underline
    float lineY = centerY + 65.0f;
    float lineW = 200.0f;
    DrawGradientUnderline(drawList, ImVec2(centerX - lineW * 0.5f, lineY), ImVec2(centerX + lineW * 0.5f, lineY),
                          IM_COL32(238, 142, 164, 220), IM_COL32(140, 212, 198, 200));

    // Animated text: waiting for game....
    int dotsCount = ((int)(time * 2.5f)) % 4;
    std::string waitText = "waiting for game" + std::string(dotsCount + 1, '.');
    if (m_fontRegular) ImGui::PushFont(m_fontRegular);
    ImVec2 waitSize = ImGui::CalcTextSize(waitText.c_str());
    drawList->AddText(ImVec2(centerX - waitSize.x * 0.5f, lineY + 16.0f), IM_COL32(160, 165, 180, 255), waitText.c_str());
    if (m_fontRegular) ImGui::PopFont();

    // Cancel Button
    float btnW = 120.0f;
    float btnH = 32.0f;
    ImGui::SetCursorScreenPos(ImVec2(centerX - btnW * 0.5f, winSize.y - 65.0f));
    if (PrimordialButton("Cancel", ImVec2(btnW, btnH), false, false)) {
        m_isWaitingForGame = false;
        m_isInjecting = false;
    }
}

// Modal for Automatic Loader Updates with ATUALIZAR button
void VibeUI::RenderUpdateModal() {
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    ImVec2 modalSize(460, 200);
    ImVec2 modalPos((dispSize.x - modalSize.x) * 0.5f, (dispSize.y - modalSize.y) * 0.5f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Dark backdrop overlay
    drawList->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 215));

    // Flat Card for Update Modal
    DrawFlatCard(drawList, modalPos, ImVec2(modalPos.x + modalSize.x, modalPos.y + modalSize.y),
                 IM_COL32(22, 23, 29, 255), IM_COL32(238, 142, 164, 200), 10.0f);

    UpdaterState state = m_updater.GetState();

    if (state == UpdaterState::UpdateAvailable) {
        // --- STAGE 1: Update Detected Notice with ATUALIZAR button ---
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 20));
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(0.933f, 0.557f, 0.643f, 1.0f), "Nova Atualizacao Disponivel!");
        if (m_fontBold) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 54));
        std::string newVer = m_updater.GetLatestVersion();
        ImGui::TextColored(ImVec4(0.92f, 0.93f, 0.95f, 1.0f), "Uma nova versao do executavel foi detectada no GitHub:");
        
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 78));
        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.95f, 1.0f), "Versao detectada: %s", newVer.c_str());

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 104));
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.80f, 1.0f), "Deseja baixar e atualizar o executavel agora?");

        // Action Buttons
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + modalSize.x - 220.0f, modalPos.y + 145.0f));
        if (PrimordialButton("Depois", ImVec2(80, 32), false, false)) {
            m_showUpdateModal = false;
            m_updater.DismissLoaderModal();
        }

        ImGui::SameLine(0, 12.0f);
        if (PrimordialButton("ATUALIZAR", ImVec2(115, 32), true, true)) {
            m_updater.StartDownloadLoaderUpdateAsync();
        }
    }
    else if (state == UpdaterState::Downloading) {
        // --- STAGE 2: Downloading with Progress Bar ---
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 20));
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(0.933f, 0.557f, 0.643f, 1.0f), "Baixando Atualizacao...");
        if (m_fontBold) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 52));
        float progress = m_updater.GetProgress();
        ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "Progresso: %d%% - %s", (int)(progress * 100.0f), m_updater.GetStatusMessage().c_str());

        // Glowing progress bar
        float barX = modalPos.x + 24.0f;
        float barY = modalPos.y + 88.0f;
        float barW = modalSize.x - 48.0f;
        float barH = 14.0f;

        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(14, 15, 20, 255), 6.0f);
        drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(45, 48, 60, 255), 6.0f);

        float fillW = barW * std::clamp(progress, 0.05f, 1.0f);
        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH), IM_COL32(238, 142, 164, 255), 6.0f);
        DrawGlow(drawList, ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH), IM_COL32(238, 142, 164, 120), 6.0f, 6.0f, 3);

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + modalSize.x - 110.0f, modalPos.y + 145.0f));
        if (PrimordialButton("Cancelar", ImVec2(90, 32), false, false)) {
            m_updater.Cancel();
            m_showUpdateModal = false;
            m_updater.DismissLoaderModal();
        }
    }
    else if (state == UpdaterState::Updated) {
        // --- STAGE 3: Finished, prompt to Restart ---
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 20));
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(0.0f, 0.95f, 0.55f, 1.0f), "Atualizacao Concluida!");
        if (m_fontBold) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 54));
        ImGui::TextColored(ImVec4(0.92f, 0.93f, 0.95f, 1.0f), "O novo executavel foi baixado com sucesso.");

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 80));
        ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), "Clique em Reiniciar para abrir a versao atualizada.");

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + modalSize.x - 125.0f, modalPos.y + 145.0f));
        if (PrimordialButton("Reiniciar", ImVec2(105, 32), true, true)) {
            m_updater.ApplyAndRestart();
            shouldClose = true;
        }
    }
    else {
        // Failed or other
        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 20));
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Status da Atualizacao");
        if (m_fontBold) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 24, modalPos.y + 60));
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), m_updater.GetStatusMessage().c_str());

        ImGui::SetCursorScreenPos(ImVec2(modalPos.x + modalSize.x - 110.0f, modalPos.y + 145.0f));
        if (PrimordialButton("Fechar", ImVec2(90, 32), false, false)) {
            m_showUpdateModal = false;
            m_updater.DismissLoaderModal();
        }
    }
}

// Running Process Browser Modal
void VibeUI::RenderProcessPickerModal() {
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    ImVec2 modalSize(500, 380);
    ImVec2 modalPos((dispSize.x - modalSize.x) * 0.5f, (dispSize.y - modalSize.y) * 0.5f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 215));

    DrawFlatCard(drawList, modalPos, ImVec2(modalPos.x + modalSize.x, modalPos.y + modalSize.y),
                 IM_COL32(22, 23, 29, 255), IM_COL32(238, 142, 164, 180), 10.0f);

    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 20, modalPos.y + 18));
    ImGui::BeginChild("##PickerContent", ImVec2(modalSize.x - 40, modalSize.y - 36), false, ImGuiWindowFlags_NoBackground);

    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.933f, 0.557f, 0.643f, 1.0f), "Select Running Target Process");
    if (m_fontBold) ImGui::PopFont();
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint("##FilterProc", "Search process name...", m_bufProcessSearch, sizeof(m_bufProcessSearch));
    ImGui::SameLine(0, 10.0f);
    if (PrimordialButton("Refresh", ImVec2(100, 26), false, false)) {
        m_cachedProcessList = Memory::GetProcessList();
    }

    ImGui::Spacing();

    // Process List
    ImGui::BeginChild("##ProcListScroll", ImVec2(modalSize.x - 40, 210), true);

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
    ImGui::SetCursorPosX(modalSize.x - 40 - 100);
    if (PrimordialButton("Close", ImVec2(100, 30), false, false)) {
        m_showProcessPicker = false;
    }

    ImGui::EndChild();
}

// Profile Switcher Modal
void VibeUI::RenderProfilePickerModal() {
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    ImVec2 modalSize(420, 300);
    ImVec2 modalPos((dispSize.x - modalSize.x) * 0.5f, (dispSize.y - modalSize.y) * 0.5f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 215));

    DrawFlatCard(drawList, modalPos, ImVec2(modalPos.x + modalSize.x, modalPos.y + modalSize.y),
                 IM_COL32(22, 23, 29, 255), IM_COL32(238, 142, 164, 180), 10.0f);

    ImGui::SetCursorScreenPos(ImVec2(modalPos.x + 20, modalPos.y + 18));
    ImGui::BeginChild("##ProfileContent", ImVec2(modalSize.x - 40, modalSize.y - 36), false, ImGuiWindowFlags_NoBackground);

    if (m_fontBold) ImGui::PushFont(m_fontBold);
    ImGui::TextColored(ImVec4(0.933f, 0.557f, 0.643f, 1.0f), "Select Target Profile");
    if (m_fontBold) ImGui::PopFont();
    ImGui::Separator();

    ImGui::Spacing();

    auto& settings = ConfigManager::Get().Settings();
    for (int i = 0; i < (int)settings.profiles.size(); ++i) {
        bool isSelected = (settings.activeProfileIndex == i);
        std::string label = settings.profiles[i].name;
        if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0, 28.0f))) {
            settings.activeProfileIndex = i;
            auto& activeP = ConfigManager::Get().GetActiveProfile();
            strcpy_s(m_bufProcessName, sizeof(m_bufProcessName), activeP.exeName.c_str());
            strcpy_s(m_bufDllPath, sizeof(m_bufDllPath), activeP.dllPath.c_str());
            strcpy_s(m_bufGithubRepo, sizeof(m_bufGithubRepo), activeP.githubRepo.c_str());
            strcpy_s(m_bufDirectUrl, sizeof(m_bufDirectUrl), activeP.directUrl.c_str());
            ConfigManager::Get().Save();
            m_showProfilePicker = false;
        }
    }

    ImGui::Spacing();
    if (PrimordialButton("+ Add Profile", ImVec2(120, 28), false, false)) {
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

    ImGui::SameLine(modalSize.x - 40 - 90);
    if (PrimordialButton("Close", ImVec2(90, 28), false, false)) {
        m_showProfilePicker = false;
    }

    ImGui::EndChild();
}
