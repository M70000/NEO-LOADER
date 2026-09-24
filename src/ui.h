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
    void RenderTopBar();
    void RenderPrimordialDashboard();
    void RenderWaitingForGameScreen();
    void RenderUpdateModal();
    void RenderProcessPickerModal();
    void RenderProfilePickerModal();

    // Primordial Visual Primitives
    void DrawHourglassLogo(ImDrawList* drawList, ImVec2 center, float size, ImU32 outlineColor, ImU32 sandColor);
    void DrawGradientUnderline(ImDrawList* drawList, ImVec2 start, ImVec2 end, ImU32 colLeft, ImU32 colRight);
    void DrawFlatCard(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 bgCol, ImU32 borderCol, float rounding);
    void DrawGlow(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color, float rounding, float glowSize, int passes = 4);
    bool PrimordialButton(const char* label, ImVec2 size, bool isPrimary = false, bool withGlow = true);

    // Live state
    DllUpdater m_updater;
    bool m_showUpdateModal = false;
    bool m_showProcessPicker = false;
    bool m_showProfilePicker = false;
    bool m_isInjecting = false;
    bool m_isWaitingForGame = false;
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
