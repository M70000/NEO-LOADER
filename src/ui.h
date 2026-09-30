#pragma once
// ============================================================================
//  neo loader - compact single-screen shell (700 x 440)
//  Layout: wordmark / 2x2 status cards / options / LOAD / target line.
//  Style: dark glassmorphism with a violet accent (reference palette).
// ============================================================================
#include "imgui.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include "injection.h"
#include "updater.h"
#include "theme.h"

class VibeUI {
public:
    static VibeUI& Get();

    void Initialize();
    void Render();

    bool shouldClose = false;

    // Fonts owned by the ImGui atlas, created in main.cpp.
    ImFont* m_fontRegular = nullptr;
    ImFont* m_fontBold    = nullptr;
    ImFont* m_fontTitle   = nullptr;
    ImFont* m_fontSmall   = nullptr;
    ImFont* m_fontTiny    = nullptr;

    void OpenDllFileDialog();
    void TriggerInjection();
    void TriggerUpdateCheck();

private:
    VibeUI() = default;
    VibeUI(const VibeUI&) = delete;
    VibeUI& operator=(const VibeUI&) = delete;

    void SetupStyles();
    void Tick();

    // ---------------------------------------------------------------- screen
    void RenderStatCard(int index, ImVec2 mn, ImVec2 mx, bool hovered);
    void RenderOverlay();
    void RenderToasts();

    // -------------------------------------------------------------- modals
    void RenderUpdateModal();
    void RenderProcessPickerModal();
    void RenderProfilePickerModal();

    // ------------------------------------------------------------- widgets
    bool GlassButton(const char* id, const char* label, ImVec2 mn, ImVec2 mx, int style,
                     Neo::IconKind ic);
    bool IconButton(const char* id, Neo::IconKind ic, ImVec2 mn, ImVec2 mx,
                    const char* tip, bool danger);
    bool CheckboxRow(const char* id, ImVec2 mn, const char* label, bool* value);
    void Card(ImVec2 mn, ImVec2 mx, bool hovered);
    void Toast(const std::string& msg);

    float AnimF(ImGuiID key, float target, float speed);
    void  AddLog(const std::string& line);

    // --------------------------------------------------------------- state
    int   m_page  = 0;          // kept for the settings jump
    int   m_tab   = 0;
    float m_time  = 0.0f;
    float m_intro = 0.0f;

    DllUpdater  m_updater;
    bool  m_showUpdateModal    = false;
    bool  m_showProcessPicker  = false;
    bool  m_showProfilePicker  = false;
    bool  m_isInjecting        = false;
    bool  m_isWaitingForGame   = false;
    bool  m_injectSuccess      = false;
    float m_injectMessageTimer = 0.0f;
    float m_waitTimer          = 0.0f;
    std::string m_lastInjectLog = "Ready to inject.";
    std::vector<std::string> m_log;

    DWORD m_cachedTargetPid     = 0;
    bool  m_cachedTargetRunning = false;
    float m_procCheckTimer      = 0.0f;
    std::vector<ProcessEntry> m_cachedProcessList;

    char m_bufProcessName[128]  = {};
    char m_bufDllPath[MAX_PATH] = {};
    char m_bufGithubRepo[160]   = {};
    char m_bufDirectUrl[256]    = {};
    char m_bufAssetPattern[64]  = {};
    char m_bufProcessSearch[64] = {};
    char m_bufOperator[64]      = {};

    // worker -> UI handoff for the detached injection thread
    std::atomic<bool> m_injectDone { false };
    std::atomic<bool> m_injectOk   { false };
    std::mutex        m_injectLock;
    std::string       m_injectOut;
    float             m_closeAt    = -1.0f;

    std::string m_toast;
    float m_toastTimer    = 0.0f;
    int   m_accentIndex   = 0;
    bool  m_reducedMotion = false;
    bool  m_soundOnStart  = true;
};
