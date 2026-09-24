#pragma once

#include "imgui.h"
#include "config.h"
#include "updater.h"
#include "injection.h"
#include <string>
#include <vector>

class VibeUI {
public:
    static VibeUI& Get();

    void Initialize();
    void Render();

    // Dialogs & Helpers
    void OpenDllFileDialog();
    void TriggerInjection();
    void TriggerUpdateCheck();

    // Window control states
    bool shouldClose = false;
    bool isDragging = false;
    POINT dragOffset = { 0, 0 };

    // High quality fonts
    ImFont* m_fontRegular = nullptr;
    ImFont* m_fontBold = nullptr;
    ImFont* m_fontTitle = nullptr;
    ImFont* m_fontSmall = nullptr;

private:
    VibeUI();

    // Sub-renderers
    void SetupStyles();
    void RenderBackgroundEffects(ImDrawList* drawList, ImVec2 winSize);
    void RenderTitleBar();
    void RenderHeaderBanner();
    void RenderLeftPanel();
    void RenderRightPanel();
    void RenderFooterBar();
    void RenderUpdateModal();
    void RenderProcessPickerModal();

    // Custom 3D & Glow visual primitives
    void DrawRadialGlow(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, int steps = 16);
    void DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float glowSize, int passes = 5);
    void Draw3DCard(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 topBg, ImU32 botBg, ImU32 borderColor, float rounding, bool withGlow = true, ImU32 glowCol = 0, float glowIntensity = 1.0f);
    bool GlowingButton(const char* label, ImVec2 size, ImU32 baseCol, ImU32 hoverCol, ImU32 glowCol, bool pulse = false, bool isPrimary = false);

    // Live state
    DllUpdater m_updater;
    bool m_showUpdateModal = false;
    bool m_showProcessPicker = false;
    bool m_isInjecting = false;
    std::string m_lastInjectLog = "Ready to inject.";
    bool m_injectSuccess = false;
    float m_injectMessageTimer = 0.0f;

    // Process polling cache
    DWORD m_cachedTargetPid = 0;
    bool m_cachedTargetRunning = false;
    float m_procCheckTimer = 0.0f;
    std::vector<ProcessEntry> m_cachedProcessList;

    // Custom process/dll input buffers
    char m_bufProcessName[128];
    char m_bufDllPath[MAX_PATH];
    char m_bufGithubRepo[128];
    char m_bufDirectUrl[256];
    char m_bufProcessSearch[64];
};
