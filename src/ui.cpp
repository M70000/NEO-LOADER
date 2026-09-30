// ============================================================================
//  neo loader - compact single-screen shell
// ----------------------------------------------------------------------------
//  Original NEO NIRVANA composition (wordmark / 2x2 status cards / options /
//  LOAD / target line) repainted with the reference's dark glass + violet look.
//  Every pixel is drawn with ImDrawList; ImGui is only used for hit-testing,
//  text input, popups and the DX11 backend.
// ============================================================================
#include "ui.h"
#include "config.h"
#include "version.h"
#include "audio.h"

#include <windows.h>
#include <shobjidl.h>

#include <thread>
#include <cstdio>
#include <cfloat>
#include <cstring>
#include <cctype>
#include <cmath>
#include <string>
#include <algorithm>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
//  small helpers
// ============================================================================
namespace {

constexpr float kPad   = 28.0f;   // left / right margin
constexpr float kGapX  = 16.0f;   // gutter between the two card columns
constexpr float kGapY  = 14.0f;   // gutter between the two card rows
constexpr float kCardH = 84.0f;
constexpr float kCardR = 16.0f;
constexpr float kCardsY = 104.0f;
constexpr float kCheckY = 304.0f;
constexpr float kLoadY  = 344.0f;
constexpr float kLoadH  = 52.0f;

inline ImVec2 V2(float x, float y) { return ImVec2(x, y); }

void Txt(ImDrawList* dl, ImFont* f, float sz, ImVec2 p, ImU32 c, const char* s) {
    if (f && s && s[0]) dl->AddText(f, sz, p, c, s);
}
float TxtW(ImFont* f, float sz, const char* s) {
    if (!f || !s) return 0.0f;
    return f->CalcTextSizeA(sz, FLT_MAX, 0.0f, s).x;
}
void TxtC(ImDrawList* dl, ImFont* f, float sz, float cx, float y, ImU32 c, const char* s) {
    Txt(dl, f, sz, V2(cx - TxtW(f, sz, s) * 0.5f, y), c, s);
}
std::string Fit(ImFont* f, float sz, const std::string& s, float maxW) {
    if (TxtW(f, sz, s.c_str()) <= maxW) return s;
    std::string out = s;
    while (out.size() > 1 && TxtW(f, sz, (out + "...").c_str()) > maxW) out.pop_back();
    return out + "...";
}
std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::wstring ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    while (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

const ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoBackground;

// The floating dialogs MUST be allowed to come to the front, otherwise the
// fullscreen main window (which is focused almost all the time) paints over
// them and the modal appears to be missing entirely.
const ImGuiWindowFlags kModalFlags =
    kPanelFlags & ~ImGuiWindowFlags_NoBringToFrontOnFocus;

} // namespace

// ============================================================================
//  singleton
// ============================================================================
VibeUI& VibeUI::Get() { static VibeUI inst; return inst; }

// ============================================================================
//  animation channel
// ============================================================================
float VibeUI::AnimF(ImGuiID key, float target, float speed)
{
    float* v = ImGui::GetStateStorage()->GetFloatRef(key, target);
    if (m_reducedMotion) { *v = target; return target; }
    const float dt = ImGui::GetIO().DeltaTime;
    *v += (target - *v) * (1.0f - expf(-dt * speed));
    return *v;
}

void VibeUI::AddLog(const std::string& line)
{
    m_log.push_back(line);
    if (m_log.size() > 160)
        m_log.erase(m_log.begin(), m_log.begin() + (m_log.size() - 160));
}

void VibeUI::Toast(const std::string& msg)
{
    m_toast = msg;
    m_toastTimer = 3.4f;
}

// ============================================================================
//  styles
// ============================================================================
void VibeUI::SetupStyles()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 0.0f;
    s.ChildRounding      = 12.0f;
    s.FrameRounding      = 10.0f;
    s.PopupRounding      = 16.0f;
    s.ScrollbarRounding  = 12.0f;
    s.GrabRounding       = 10.0f;
    s.WindowBorderSize   = 0.0f;
    s.ChildBorderSize    = 0.0f;
    s.PopupBorderSize    = 0.0f;
    s.FrameBorderSize    = 0.0f;
    s.WindowPadding      = ImVec2(0, 0);
    s.ItemSpacing        = ImVec2(8, 8);
    s.ScrollbarSize      = 8.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = ImVec4(0.953f, 0.941f, 0.988f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.471f, 0.439f, 0.580f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_ChildBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.075f, 0.059f, 0.110f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
    c[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(1.00f, 1.00f, 1.00f, 0.09f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(1.00f, 1.00f, 1.00f, 0.14f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);
    c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(1.00f, 1.00f, 1.00f, 0.28f);
    c[ImGuiCol_CheckMark]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]            = ImVec4(0.66f, 0.33f, 0.97f, 1.00f);
    c[ImGuiCol_SliderGrabActive]      = ImVec4(0.79f, 0.55f, 0.99f, 1.00f);
    c[ImGuiCol_Button]                = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(1.00f, 1.00f, 1.00f, 0.14f);
    c[ImGuiCol_ButtonActive]          = ImVec4(1.00f, 1.00f, 1.00f, 0.18f);
    c[ImGuiCol_Header]                = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_HeaderActive]          = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
    c[ImGuiCol_Separator]             = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.66f, 0.33f, 0.97f, 0.35f);
}

// ============================================================================
//  startup
// ============================================================================
void VibeUI::Initialize()
{
    SetupStyles();

    ConfigManager& cm = ConfigManager::Get();
    cm.Load();
    LoaderSettings& S = cm.Settings();
    TargetProfile&  P = cm.GetActiveProfile();

    strncpy_s(m_bufProcessName,  P.exeName.c_str(),                     _TRUNCATE);
    strncpy_s(m_bufDllPath,      P.dllPath.c_str(),                     _TRUNCATE);
    strncpy_s(m_bufGithubRepo,   P.githubRepo.c_str(),                  _TRUNCATE);
    strncpy_s(m_bufDirectUrl,    P.directUrl.c_str(),                   _TRUNCATE);
    strncpy_s(m_bufAssetPattern, P.githubAsset.empty() ? ".dll" : P.githubAsset.c_str(), _TRUNCATE);
    strncpy_s(m_bufOperator,     S.operatorName.c_str(),                _TRUNCATE);

    m_cachedProcessList = Memory::GetProcessList();

    if (m_soundOnStart)
        NeoAudio::PlayStartupSound();

    AddLog("neo injection core online.");
    AddLog("active profile: " + (P.name.empty() ? std::string("none") : P.name));

    if (!P.githubRepo.empty()) {
        UpdateConfig cfg;
        cfg.githubRepo     = P.githubRepo;
        cfg.assetPattern   = m_bufAssetPattern;
        cfg.directUrl      = P.directUrl;
        cfg.localSavePath  = P.dllPath;
        cfg.currentVersion = P.version;
        cfg.isSilent       = true;
        m_updater.CheckDllSilentAsync(cfg);
    }
    m_updater.CheckLoaderUpdateAsync(S.loaderGithubRepo, LOADER_VERSION_TAG);
}

// ============================================================================
//  per-frame logic
// ============================================================================
void VibeUI::Tick()
{
    const float dt = ImGui::GetIO().DeltaTime;
    ConfigManager& cm = ConfigManager::Get();
    TargetProfile&  P = cm.GetActiveProfile();

    // ---- target process poll (2 Hz) ---------------------------------------
    m_procCheckTimer -= dt;
    if (m_procCheckTimer <= 0.0f) {
        m_procCheckTimer = 0.5f;
        DWORD pid = 0;
        if (!P.exeName.empty()) {
            const std::wstring exeW = ToWide(P.exeName);
            if (!exeW.empty()) pid = Memory::GetPID(exeW.c_str());
        }
        m_cachedTargetPid     = pid;
        m_cachedTargetRunning = (pid != 0);
    }

    // ---- injection worker handoff -----------------------------------------
    if (m_injectDone.load()) {
        m_injectDone.store(false);
        bool ok = false;
        std::string out;
        {
            std::lock_guard<std::mutex> lk(m_injectLock);
            ok  = m_injectOk.load();
            out = m_injectOut;
        }
        m_isInjecting      = false;
        m_isWaitingForGame = false;
        m_injectSuccess    = ok;
        m_lastInjectLog    = out.empty() ? (ok ? "module mapped." : "injection failed.") : out;
        AddLog(m_lastInjectLog);
        Toast(ok ? "Payload mapped successfully" : "Injection failed");
        m_injectMessageTimer = 3.2f;
        if (ok && cm.Settings().autoCloseOnInject)
            m_closeAt = m_time + 1.9f;
    }

    if (m_injectMessageTimer > 0.0f) m_injectMessageTimer -= dt;
    if (m_closeAt > 0.0f && m_time >= m_closeAt) shouldClose = true;

    // ---- waiting for the target -------------------------------------------
    if (m_isWaitingForGame) {
        m_waitTimer -= dt;
        if (m_cachedTargetPid != 0) {
            m_isWaitingForGame = false;
            TriggerInjection();
        } else if (m_waitTimer <= 0.0f) {
            m_isWaitingForGame = false;
            m_lastInjectLog = "timed out waiting for " + P.exeName;
            AddLog(m_lastInjectLog);
            Toast("Timed out waiting for target");
            m_injectMessageTimer = 3.0f;
        }
    }

    if (m_toastTimer > 0.0f) m_toastTimer -= dt;

    if (m_updater.HasNewLoaderUpdate())
        m_showUpdateModal = true;
}

// ============================================================================
//  widgets
// ============================================================================
void VibeUI::Card(ImVec2 mn, ImVec2 mx, bool hovered)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    Neo::Glass(dl, mn, mx, kCardR, 1.0f, hovered, 18.0f, V2(0.0f, 9.0f));
}

bool VibeUI::GlassButton(const char* id, const char* label, ImVec2 mn, ImVec2 mx,
                         int style, Neo::IconKind ic)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(mn);
    const bool clicked = ImGui::InvisibleButton(id, V2(mx.x - mn.x, mx.y - mn.y));
    const bool hov = ImGui::IsItemHovered();
    const float t  = AnimF(ImGui::GetID(id), hov ? 1.0f : 0.0f, 14.0f);
    const float R  = (style == 0) ? 13.0f : 9.0f;

    ImU32 textCol = Neo::kTextPri;

    if (style == 0) {
        Neo::SoftShadow(dl, mn, mx, R, 16.0f + 8.0f * t, 0.55f, V2(0.0f, 7.0f));
        // Saturated violet on purpose. A pale top stop used to wash this out to
        // periwinkle, which is what made the button read as "blue".
        Neo::GradientRect(dl, mn, mx, R,
                          Neo::Mix(Neo::Accent(), IM_COL32(255, 255, 255, 255), 0.12f + 0.10f * t),
                          Neo::AccentLo());
        Neo::GradientRect(dl, V2(mn.x + 1.0f, mn.y + 1.0f),
                          V2(mx.x - 1.0f, mn.y + (mx.y - mn.y) * 0.24f),
                          ImMax(R - 1.0f, 0.0f),
                          IM_COL32(255, 255, 255, 26), IM_COL32(255, 255, 255, 0));
        dl->AddRect(mn, mx, Neo::AC(Neo::AccentHi(), 0.50f + 0.35f * t), R, 1.2f);
        textCol = IM_COL32(255, 255, 255, 255);
    } else if (style == 2) {
        dl->AddRectFilled(mn, mx, Neo::AC(Neo::kRed, 0.13f + 0.10f * t), R);
        dl->AddRect(mn, mx, Neo::AC(Neo::kRed, 0.42f + 0.25f * t), R, 1.1f);
        textCol = Neo::Mix(Neo::kTextSec, Neo::kRed, 0.35f + 0.65f * t);
    } else {
        dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, (int)(13.0f + 15.0f * t)), R);
        dl->AddRect(mn, mx, hov ? Neo::kBorderHi : Neo::kBorder, R, 1.0f);
        textCol = Neo::Mix(Neo::kTextSec, Neo::kTextPri, 0.42f + 0.58f * t);
    }

    const float fs   = 12.0f;
    const float tw   = TxtW(m_fontBold, fs, label);
    const float iconW = (ic != Neo::Icon_None) ? 19.0f : 0.0f;
    const float total = tw + iconW;
    const float tx   = mn.x + (mx.x - mn.x - total) * 0.5f;
    const float ty   = mn.y + (mx.y - mn.y - fs) * 0.5f - 1.5f;
    if (ic != Neo::Icon_None)
        Neo::Icon(dl, ic, V2(tx + 6.5f, ty + fs * 0.5f + 0.5f), 13.0f, textCol, 1.6f);
    Txt(dl, m_fontBold, fs, V2(tx + iconW, ty), textCol, label);

    return clicked;
}

bool VibeUI::IconButton(const char* id, Neo::IconKind ic, ImVec2 mn, ImVec2 mx,
                        const char* tip, bool danger)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(mn);
    const bool clicked = ImGui::InvisibleButton(id, V2(mx.x - mn.x, mx.y - mn.y));
    const bool hov = ImGui::IsItemHovered();
    const float t  = AnimF(ImGui::GetID(id), hov ? 1.0f : 0.0f, 16.0f);
    const float R  = 8.0f;
    const ImU32 base = danger ? Neo::kRed : IM_COL32(255, 255, 255, 255);

    if (t > 0.01f)
        dl->AddRectFilled(mn, mx, Neo::AC(base, (danger ? 0.15f : 0.09f) * t), R);
    dl->AddRect(mn, mx, Neo::AC(base, (danger ? 0.28f : 0.09f) + (danger ? 0.22f : 0.09f) * t), R, 1.0f);

    const ImU32 col = danger
        ? Neo::AC(Neo::kRed, 0.70f + 0.30f * t)
        : Neo::Mix(Neo::kTextSec, Neo::kTextPri, t);
    Neo::Icon(dl, ic, V2((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f), 12.5f, col, 1.6f);

    if (hov && tip && tip[0]) ImGui::SetTooltip("%s", tip);
    return clicked;
}

bool VibeUI::CheckboxRow(const char* id, ImVec2 mn, const char* label, bool* value)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float box = 22.0f;
    const ImVec2 mnb = mn;
    const ImVec2 mxb(mn.x + box, mn.y + box);
    const float labelW = TxtW(m_fontRegular, 12.5f, label);

    ImGui::SetCursorScreenPos(mn);
    const bool clicked = ImGui::InvisibleButton(id, V2(box + 10.0f + labelW, box));
    const bool hov = ImGui::IsItemHovered();
    if (clicked && value) *value = !*value;

    const float t = AnimF(ImGui::GetID(id), (value && *value) ? 1.0f : 0.0f, 16.0f);

    if (t > 0.02f) {
        Neo::GradientRect(dl, mnb, mxb, 7.0f,
                          Neo::AC(Neo::AccentHi(), t * 0.98f),
                          Neo::AC(Neo::AccentLo(), t * 0.98f));
    } else {
        dl->AddRectFilled(mnb, mxb, IM_COL32(255, 255, 255, hov ? 16 : 9), 7.0f);
    }
    dl->AddRect(mnb, mxb,
                t > 0.5f ? Neo::AC(Neo::AccentHi(), 0.55f + 0.35f * t)
                         : (hov ? Neo::kBorderHi : Neo::kBorder),
                7.0f, 1.15f);

    if (t > 0.25f) {
        const ImU32 ck = Neo::AC(IM_COL32(255, 255, 255, 255), t);
        const ImVec2 c(mnb.x + box * 0.5f, mnb.y + box * 0.5f);
        dl->AddLine(V2(c.x - 4.8f, c.y + 0.4f), V2(c.x - 1.4f, c.y + 3.9f), ck, 2.0f);
        dl->AddLine(V2(c.x - 1.4f, c.y + 3.9f), V2(c.x + 5.0f, c.y - 4.2f), ck, 2.0f);
    }

    Txt(dl, m_fontRegular, 12.5f,
        V2(mnb.x + box + 10.0f, mn.y + (box - 12.5f) * 0.5f - 1.5f),
        hov ? Neo::kTextPri : (value && *value ? Neo::kTextSec : Neo::kTextMut), label);
    return clicked;
}

// ============================================================================
//  status cards
// ============================================================================
void VibeUI::RenderStatCard(int index, ImVec2 mn, ImVec2 mx, bool hovered)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ConfigManager& cm = ConfigManager::Get();
    LoaderSettings& S = cm.Settings();
    TargetProfile&  P = cm.GetActiveProfile();
    Card(mn, mx, hovered);

    const char* label  = "";
    std::string value;
    ImU32 valueCol = Neo::kTextPri;
    ImU32 glyphCol = Neo::AC(Neo::kTextSec, 0.85f);
    const char* action = "";
    int glyph = 0;   // 0 = hollow ring, 1 = filled dot, 2 = status ring, 3 = rounded square

    switch (index) {
    case 0:
        label  = "ACCOUNT ID";
        value  = P.name.empty() ? std::string("No profile") : P.name;
        action = "Switch";
        glyph  = 0;
        break;
    case 1:
        label  = "LOADER VERSION";
        value  = std::string("v") + LOADER_VERSION_TAG;
        action = "Check";
        glyph  = 1;
        break;
    case 2:
        label  = "CHEAT STATUS";
        value  = P.statusText.empty() ? std::string("Undetected") : P.statusText;
        // Colour the status by what it SAYS (Undetected / Detected / Expired)
        // rather than by whether the game happens to be open right now - the
        // running state already has its own line in the footer.
        {
            std::string low;
            for (size_t i = 0; i < value.size(); ++i)
                low.push_back((char)tolower((unsigned char)value[i]));
            const bool good = low.find("undetect") != std::string::npos ||
                              low.find("safe")      != std::string::npos ||
                              low.find("active")    != std::string::npos ||
                              low.find("clean")     != std::string::npos;
            const bool bad  = low.find("expired")  != std::string::npos ||
                              low.find("banned")   != std::string::npos ||
                              low.find("detected") != std::string::npos;
            valueCol = good ? Neo::kGreen : (bad ? Neo::kRed : Neo::kYellow);
            glyphCol = valueCol;
        }
        action = "Process";
        glyph  = 2;
        break;
    default:
        label  = "SUB EXPIRES IN";
        value  = S.subscriptionExpiry.empty() ? std::string("Lifetime (Active)") : S.subscriptionExpiry;
        action = "Browse";
        glyph  = 3;
        break;
    }

    // ---- status glyph -----------------------------------------------------
    const ImVec2 gp(mn.x + 27.0f, mn.y + 25.0f);
    switch (glyph) {
    case 0:
        dl->AddCircle(gp, 5.6f, Neo::AC(Neo::kTextSec, 0.85f), 0, 1.4f);
        break;
    case 1:
        dl->AddCircleFilled(gp, 3.6f, Neo::AC(Neo::kTextSec, 0.85f), 16);
        break;
    case 2:
        dl->AddCircle(gp, 5.6f, glyphCol, 0, 1.6f);
        dl->AddCircleFilled(gp, 2.1f, glyphCol, 12);
        break;
    default:
        dl->AddRect(V2(gp.x - 5.4f, gp.y - 5.4f), V2(gp.x + 5.4f, gp.y + 5.4f),
                    Neo::AC(Neo::kTextSec, 0.80f), 2.0f, 1.4f);
        break;
    }

    // ---- label + value ----------------------------------------------------
    const float textX = mn.x + 46.0f;
    Neo::TextSpaced(dl, m_fontSmall, 9.0f, V2(textX, mn.y + 18.0f), Neo::kTextMut, label, 1.15f);

    const float btnL = mx.x - 16.0f - 78.0f;
    const float availW = btnL - textX - 10.0f;
    const std::string shown = Fit(m_fontBold, 13.5f, value, availW);
    Txt(dl, m_fontBold, 13.5f, V2(textX, mn.y + 37.0f), valueCol, shown.c_str());

    // ---- action button ----------------------------------------------------
    const ImVec2 bmn(btnL, mn.y + kCardH * 0.5f - 15.0f);
    const ImVec2 bmx(mx.x - 16.0f, mn.y + kCardH * 0.5f + 15.0f);

    ImGui::PushID(100 + index);
    if (GlassButton("act", action, bmn, bmx, 1, Neo::Icon_None)) {
        switch (index) {
        case 0: m_showProfilePicker = true;  break;
        case 1: TriggerUpdateCheck();        break;
        case 2: m_showProcessPicker = true;  break;
        default: OpenDllFileDialog();        break;
        }
    }
    ImGui::PopID();
}

// ============================================================================
//  main frame
// ============================================================================
void VibeUI::Render()
{
    ImGuiIO& io = ImGui::GetIO();
    m_time += m_reducedMotion ? 0.0f : io.DeltaTime;
    Tick();

    const ImVec2 disp = io.DisplaySize;
    const float W = disp.x;
    const float H = disp.y;

    ImGui::SetNextWindowPos(V2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(disp);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, V2(0.0f, 0.0f));
    ImGui::Begin("##neo", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ConfigManager& cm = ConfigManager::Get();
    LoaderSettings& S = cm.Settings();
    TargetProfile&  P = cm.GetActiveProfile();

    const bool overlayOn = m_isWaitingForGame || m_isInjecting ||
                           (m_injectMessageTimer > 0.0f && m_closeAt > 0.0f);
    const bool modalOn   = m_showUpdateModal || m_showProcessPicker || m_showProfilePicker;
    const bool blocked   = overlayOn || modalOn;

    // ---- backdrop ---------------------------------------------------------
    Neo::Backdrop(dl, V2(0.0f, 0.0f), disp, m_time);

    // ---- window controls --------------------------------------------------
    if (IconButton("##win_min", Neo::Icon_Minimize, V2(W - 86.0f, 14.0f), V2(W - 56.0f, 44.0f),
                   "Minimize", false)) {
        HWND hw = GetActiveWindow();
        if (!hw) hw = GetForegroundWindow();
        if (hw) ShowWindow(hw, SW_MINIMIZE);
    }
    if (IconButton("##win_close", Neo::Icon_Close, V2(W - 48.0f, 14.0f), V2(W - 18.0f, 44.0f),
                   "Close", true))
        shouldClose = true;

    // ---- header -----------------------------------------------------------
    {
        const float lh = 27.0f;
        const float lw = Neo::NeoLogoWidth(lh);
        const ImVec2 lo((W - lw) * 0.5f, 24.0f);
        const ImU32 drift = m_reducedMotion
            ? Neo::kBrand
            : Neo::Mix(Neo::kBrand, IM_COL32(150, 205, 255, 255), 0.22f + 0.22f * sinf(m_time * 0.9f));
        Neo::NeoLogo(dl, V2(lo.x, lo.y + 1.0f), lh, IM_COL32(0, 8, 26, 150), 1.0f);
        Neo::NeoLogo(dl, lo, lh, drift, 1.0f);

        const ImVec2 ts = Neo::TextSpacedSize(m_fontSmall, 9.0f, "LOADER", 4.6f);
        Neo::TextSpaced(dl, m_fontSmall, 9.0f, V2((W - ts.x) * 0.5f, 60.0f),
                        Neo::kTextMut, "LOADER", 4.6f);

        dl->AddLine(V2(kPad, 88.0f), V2(W - kPad, 88.0f), Neo::kBorder, 1.0f);
    }

    // ---- 2 x 2 status cards ----------------------------------------------
    {
        const float x0 = kPad;
        const float x1 = W - kPad;
        const float cw = (x1 - x0 - kGapX) * 0.5f;
        const ImVec2 mp = io.MousePos;
        for (int i = 0; i < 4; ++i) {
            const int col = i % 2;
            const int row = i / 2;
            const ImVec2 mn(x0 + col * (cw + kGapX), kCardsY + row * (kCardH + kGapY));
            const ImVec2 mx(mn.x + cw, mn.y + kCardH);
            const bool hov = !blocked && mp.x >= mn.x && mp.x <= mx.x && mp.y >= mn.y && mp.y <= mx.y;
            RenderStatCard(i, mn, mx, hov);
        }
    }

    // ---- options ----------------------------------------------------------
    {
        const float lbl1 = TxtW(m_fontRegular, 12.5f, "Save configuration");
        const float lbl2 = TxtW(m_fontRegular, 12.5f, "Auto-close loader after load");
        const float boxW = 22.0f + 10.0f;
        const float sep  = 58.0f;
        const float total = boxW + lbl1 + sep + boxW + lbl2;
        float cx = (W - total) * 0.5f;

        if (!blocked) {
            if (CheckboxRow("##opt_save", V2(cx, kCheckY), "Save configuration", &S.saveSelection))
                cm.Save();
            if (CheckboxRow("##opt_close", V2(cx + boxW + lbl1 + sep, kCheckY),
                            "Auto-close loader after load", &S.autoCloseOnInject))
                cm.Save();
        } else {
            CheckboxRow("##opt_save_d", V2(cx, kCheckY), "Save configuration", nullptr);
            CheckboxRow("##opt_close_d", V2(cx + boxW + lbl1 + sep, kCheckY),
                        "Auto-close loader after load", nullptr);
        }
    }

    // ---- LOAD -------------------------------------------------------------
    {
        const ImVec2 mn(kPad, kLoadY);
        const ImVec2 mx(W - kPad, kLoadY + kLoadH);
        if (!blocked && GlassButton("##load", "LOAD", mn, mx, 0, Neo::Icon_Power))
            TriggerInjection();
        else if (blocked)
            GlassButton("##load_d", "LOAD", mn, mx, 0, Neo::Icon_Power);
    }

    // ---- footer -----------------------------------------------------------
    {
        const std::string tgt = P.exeName.empty() ? std::string("none") : P.exeName;
        const std::string foot = "Target detected: " + tgt;
        const float fw = TxtW(m_fontRegular, 11.5f, foot.c_str());
        const float fx = (W - fw - 13.0f) * 0.5f;
        const float fy = 408.0f;
        const ImU32 dot = P.exeName.empty() ? Neo::kTextMut
                          : (m_cachedTargetRunning ? Neo::kGreen : Neo::kYellow);
        dl->AddCircleFilled(V2(fx + 4.0f, fy + 6.5f), 3.2f, dot, 12);
        Txt(dl, m_fontRegular, 11.5f, V2(fx + 13.0f, fy), Neo::kTextMut, foot.c_str());

        if (m_cachedTargetRunning)
            Txt(dl, m_fontRegular, 11.5f, V2(W - kPad - TxtW(m_fontRegular, 11.5f, "RUNNING"),
                                              fy), Neo::AC(Neo::kGreen, 0.85f), "RUNNING");
    }

    // ---- overlay (waiting / injecting / result) ---------------------------
    if (overlayOn) {
        const float OW = 400.0f, OH = 272.0f;
        const ImVec2 omn((W - OW) * 0.5f, (H - OH) * 0.5f);
        const ImVec2 omx(omn.x + OW, omn.y + OH);
        const ImVec2 cmn(omn.x + 110.0f, omx.y - 60.0f);
        const ImVec2 cmx(omx.x - 110.0f, omx.y - 28.0f);

        ImGui::SetCursorScreenPos(cmn);
        const bool cancel = ImGui::InvisibleButton("##cancel", V2(cmx.x - cmn.x, cmx.y - cmn.y));
        const bool cancelHov = ImGui::IsItemHovered();

        RenderOverlay();

        // repaint the cancel button on top of the overlay dim
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const float ct = m_reducedMotion ? (cancelHov ? 1.0f : 0.0f)
                                         : AnimF(ImGui::GetID("##cancel"), cancelHov ? 1.0f : 0.0f, 14.0f);
        fg->AddRectFilled(cmn, cmx, IM_COL32(255, 255, 255, (int)(12.0f + 14.0f * ct)), 10.0f);
        fg->AddRect(cmn, cmx, cancelHov ? Neo::kBorderHi : Neo::kBorder, 10.0f, 1.0f);
        TxtC(fg, m_fontBold, 11.5f, (cmn.x + cmx.x) * 0.5f,
             (cmn.y + cmx.y) * 0.5f - 7.0f,
             Neo::Mix(Neo::kTextSec, Neo::kTextPri, ct), "CANCEL");

        if (cancel) {
            m_isWaitingForGame = false;
            m_isInjecting      = false;
            m_closeAt          = -1.0f;
            m_injectMessageTimer = 0.0f;
            AddLog("operation cancelled.");
            Toast("Cancelled");
        }
    }

    ImGui::End();

    // ---- modal dim --------------------------------------------------------
    if (modalOn) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(V2(0.0f, 0.0f), disp, IM_COL32(10, 10, 14, 118));
    }

    if (m_showUpdateModal)   RenderUpdateModal();
    if (m_showProcessPicker) RenderProcessPickerModal();
    if (m_showProfilePicker) RenderProfilePickerModal();

    RenderToasts();
}

// ============================================================================
//  overlay
// ============================================================================
void VibeUI::RenderOverlay()
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    TargetProfile& P = ConfigManager::Get().GetActiveProfile();

    dl->AddRectFilled(V2(0.0f, 0.0f), disp, IM_COL32(10, 10, 14, 138));

    const float W = 400.0f, H = 272.0f;
    const ImVec2 mn((disp.x - W) * 0.5f, (disp.y - H) * 0.5f);
    const ImVec2 mx(mn.x + W, mn.y + H);

    Neo::Blob(dl, V2((mn.x + mx.x) * 0.5f, mn.y + 70.0f), 210.0f, Neo::Accent(), 0.18f);
    dl->AddRectFilled(mn, mx, IM_COL32(42, 42, 48, 228), 22.0f);
    Neo::Glass(dl, mn, mx, 22.0f, 1.0f, false, 30.0f, V2(0.0f, 12.0f));

    const float lh = 22.0f;
    const float lw = Neo::NeoLogoWidth(lh);
    Neo::NeoLogo(dl, V2((mn.x + mx.x) * 0.5f - lw * 0.5f, mn.y + 24.0f), lh,
                 Neo::Mix(Neo::kBrand, IM_COL32(255, 255, 255, 255), 0.16f), 1.0f);

    const ImVec2 c((mn.x + mx.x) * 0.5f, mn.y + 100.0f);

    const char* headline;
    const char* sub;
    ImU32 ring;
    if (m_injectSuccess) {
        headline = "MODULE MAPPED";
        sub      = "The payload is running inside the target process.";
        ring     = Neo::kGreen;
    } else if (m_isInjecting) {
        headline = "INJECTING PAYLOAD";
        sub      = "Mapping image sections and resolving imports...";
        ring     = Neo::Accent();
    } else {
        headline = "WAITING FOR TARGET";
        sub      = "Start the game - injection begins automatically.";
        ring     = Neo::kYellow;
    }

    if (m_injectSuccess) {
        dl->AddCircle(c, 25.0f, ring, 48, 2.4f);
        dl->AddCircle(c, 25.0f, Neo::AC(ring, 0.18f), 48, 7.0f);
        Neo::Icon(dl, Neo::Icon_Check, c, 21.0f, ring, 2.6f);
    } else {
        Neo::Spinner(dl, c, 24.0f, 2.6f, m_time, ring);
        Neo::Icon(dl, m_isInjecting ? Neo::Icon_Chip : Neo::Icon_Clock,
                  c, 16.0f, Neo::AC(ring, 0.80f), 1.8f);
    }

    const ImVec2 hs = Neo::TextSpacedSize(m_fontBold, 12.0f, headline, 2.4f);
    Neo::TextSpaced(dl, m_fontBold, 12.0f, V2(c.x - hs.x * 0.5f, c.y + 42.0f),
                    Neo::kTextPri, headline, 2.4f);
    TxtC(dl, m_fontRegular, 11.5f, c.x, c.y + 64.0f, Neo::kTextMut, sub);

    // target line
    {
        std::string t = "target: " + (P.exeName.empty() ? std::string("none") : P.exeName);
        if (m_cachedTargetPid != 0) t += "   pid " + std::to_string((unsigned long)m_cachedTargetPid);
        if (m_isWaitingForGame) {
            const int dots = 1 + ((int)(m_time * 2.5f) % 3);
            t += std::string((size_t)dots, '.');
        }
        TxtC(dl, m_fontRegular, 11.0f, c.x, c.y + 86.0f, Neo::AC(Neo::kTextSec, 0.75f), t.c_str());
    }
}

// ============================================================================
//  toasts
// ============================================================================
void VibeUI::RenderToasts()
{
    if (m_toastTimer <= 0.0f || m_toast.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float t = ImClamp(m_toastTimer / 3.4f, 0.0f, 1.0f);
    const float a = ImClamp(t * 4.0f, 0.0f, 1.0f) * ImClamp((1.0f - t) * 6.0f + 1.0f, 0.0f, 1.0f);

    const float fs = 11.5f;
    const float tw = TxtW(m_fontBold, fs, m_toast.c_str());
    const float w  = tw + 58.0f;
    const float h  = 38.0f;
    const float y  = disp.y - 12.0f - h + (1.0f - a) * 14.0f;
    const ImVec2 mn((disp.x - w) * 0.5f, y);
    const ImVec2 mx(mn.x + w, y + h);

    dl->AddRectFilled(mn, mx, Neo::AC(IM_COL32(30, 30, 35, 238), a), 12.0f);
    dl->AddRect(mn, mx, Neo::AC(Neo::kBorderHi, a), 12.0f, 1.1f);
    dl->AddCircleFilled(V2(mn.x + 22.0f, (mn.y + mx.y) * 0.5f), 4.0f, Neo::AC(Neo::AccentHi(), a), 14);
    Txt(dl, m_fontBold, fs, V2(mn.x + 36.0f, (mn.y + mx.y) * 0.5f - fs * 0.5f - 1.0f),
        Neo::AC(Neo::kTextPri, a), m_toast.c_str());
}

// ============================================================================
//  update modal
// ============================================================================
void VibeUI::RenderUpdateModal()
{
    const float W = 420.0f, H = 250.0f;
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const ImVec2 mn((disp.x - W) * 0.5f, (disp.y - H) * 0.5f);
    const ImVec2 mx(mn.x + W, mn.y + H);

    ImGui::SetNextWindowPos(mn);
    ImGui::SetNextWindowSize(V2(W, H));
    ImGui::Begin("##modal_update", nullptr, kModalFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(mn, mx, IM_COL32(40, 40, 46, 232), 20.0f);
    Neo::Glass(dl, mn, mx, 20.0f, 1.0f, false, 26.0f, V2(0.0f, 12.0f));
    Neo::Blob(dl, V2((mn.x + mx.x) * 0.5f, mn.y + 40.0f), 180.0f, Neo::Accent(), 0.20f);

    const float lh = 20.0f;
    const float lw = Neo::NeoLogoWidth(lh);
    Neo::NeoLogo(dl, V2((mn.x + mx.x) * 0.5f - lw * 0.5f, mn.y + 26.0f), lh,
                 Neo::Mix(Neo::kBrand, IM_COL32(255, 255, 255, 255), 0.16f), 1.0f);

    const UpdaterState st = m_updater.GetState();

    const char* head = "Update";
    std::string body = m_updater.GetStatusMessage();
    switch (st) {
    case UpdaterState::UpdateAvailable: head = "UPDATE AVAILABLE"; break;
    case UpdaterState::Downloading:     head = "DOWNLOADING";      break;
    case UpdaterState::Updated:         head = "UPDATE READY";     break;
    case UpdaterState::Failed:          head = "UPDATE FAILED";    break;
    default:                            head = "UP TO DATE";       break;
    }

    const ImVec2 hs = Neo::TextSpacedSize(m_fontBold, 11.5f, head, 2.2f);
    Neo::TextSpaced(dl, m_fontBold, 11.5f, V2((mn.x + mx.x) * 0.5f - hs.x * 0.5f, mn.y + 74.0f),
                    st == UpdaterState::Failed ? Neo::kRed : Neo::kTextPri, head, 2.2f);

    if (st == UpdaterState::UpdateAvailable && !m_updater.GetLatestVersion().empty())
        body = "version " + m_updater.GetLatestVersion() + " is available";
    TxtC(dl, m_fontRegular, 11.5f, (mn.x + mx.x) * 0.5f, mn.y + 98.0f, Neo::kTextSec, body.c_str());

    if (st == UpdaterState::Downloading) {
        const ImVec2 pmn(mn.x + 36.0f, mn.y + 126.0f);
        const ImVec2 pmx(mx.x - 36.0f, mn.y + 134.0f);
        dl->AddRectFilled(pmn, pmx, Neo::kTrack, 4.0f);
        const float p = ImClamp(m_updater.GetProgress(), 0.0f, 1.0f);
        if (p > 0.01f)
            Neo::GradientRectH(dl, pmn, V2(pmn.x + (pmx.x - pmn.x) * p, pmx.y), 4.0f,
                               Neo::AccentHi(), Neo::AccentLo());
    }

    const float by = mx.y - 56.0f;
    auto place = [&](float x0, float x1, const char* lbl, int style, const char* id) -> bool {
        return GlassButton(id, lbl, V2(x0, by), V2(x1, by + 34.0f), style, Neo::Icon_None);
    };

    if (st == UpdaterState::UpdateAvailable) {
        if (place(mn.x + 36.0f, mn.x + 186.0f, "Later", 1, "##upd_later")) {
            m_updater.DismissLoaderModal();
            m_showUpdateModal = false;
        }
        if (place(mx.x - 186.0f, mx.x - 36.0f, "Update now", 0, "##upd_go"))
            m_updater.StartDownloadLoaderUpdateAsync();
    } else if (st == UpdaterState::Downloading) {
        if (place(mn.x + 36.0f, mx.x - 36.0f, "Cancel", 2, "##upd_cancel"))
            m_updater.Cancel();
    } else if (st == UpdaterState::Updated) {
        if (place(mn.x + 36.0f, mx.x - 36.0f, "Restart now", 0, "##upd_restart")) {
            m_updater.ApplyAndRestart();
            shouldClose = true;
        }
    } else {
        if (place(mn.x + 36.0f, mx.x - 36.0f, "Close", 1, "##upd_close")) {
            m_updater.DismissLoaderModal();
            m_showUpdateModal = false;
        }
    }

    ImGui::End();
}

// ============================================================================
//  process picker
// ============================================================================
void VibeUI::RenderProcessPickerModal()
{
    TargetProfile& P = ConfigManager::Get().GetActiveProfile();

    // 396 keeps a 22 px margin top and bottom inside the 700x440 window; at
    // 452 the dialog was taller than the window and its header got clipped.
    const float W = 480.0f, H = 396.0f;
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const ImVec2 mn((disp.x - W) * 0.5f, (disp.y - H) * 0.5f);
    const ImVec2 mx(mn.x + W, mn.y + H);

    ImGui::SetNextWindowPos(mn);
    ImGui::SetNextWindowSize(V2(W, H));
    ImGui::Begin("##modal_procs", nullptr, kModalFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(mn, mx, IM_COL32(40, 40, 46, 232), 20.0f);
    Neo::Glass(dl, mn, mx, 20.0f, 1.0f, false, 26.0f, V2(0.0f, 12.0f));

    Neo::TextSpaced(dl, m_fontSmall, 9.0f, V2(mn.x + 26.0f, mn.y + 22.0f),
                    Neo::kTextMut, "TARGET PROCESS", 1.3f);
    Txt(dl, m_fontBold, 14.0f, V2(mn.x + 26.0f, mn.y + 38.0f), Neo::kTextPri,
        "Choose a running process");

    if (IconButton("##proc_close", Neo::Icon_Close, V2(mx.x - 44.0f, mn.y + 20.0f),
                   V2(mx.x - 20.0f, mn.y + 44.0f), "Close", true))
        m_showProcessPicker = false;

    // search field ---------------------------------------------------------
    ImGui::SetCursorScreenPos(V2(mn.x + 26.0f, mn.y + 70.0f));
    ImGui::SetNextItemWidth(W - 52.0f - 96.0f - 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, V2(12.0f, 9.0f));
    ImGui::InputTextWithHint("##proc_search", "Search...", m_bufProcessSearch, IM_ARRAYSIZE(m_bufProcessSearch));
    ImGui::PopStyleVar();

    ImGui::SetCursorScreenPos(V2(mx.x - 26.0f - 96.0f, mn.y + 70.0f));
    if (GlassButton("##proc_rescan", "Rescan", V2(mx.x - 26.0f - 96.0f, mn.y + 70.0f),
                    V2(mx.x - 26.0f, mn.y + 70.0f + 36.0f), 1, Neo::Icon_Refresh))
        m_cachedProcessList = Memory::GetProcessList();

    // list -----------------------------------------------------------------
    const float listTop = mn.y + 120.0f;
    const float listBot = mx.y - 26.0f;
    ImGui::SetCursorScreenPos(V2(mn.x + 26.0f, listTop));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, V2(5.0f, 5.0f));
    ImGui::BeginChild("##proc_list", V2(W - 52.0f, listBot - listTop), 0);
    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const float availW = ImGui::GetContentRegionAvail().x;
        const std::string filter = Lower(m_bufProcessSearch);
        const ImVec2 base = ImGui::GetCursorScreenPos();
        const float rh = 34.0f;
        int shown = 0;
        for (size_t i = 0; i < m_cachedProcessList.size(); ++i) {
            const ProcessEntry& pe = m_cachedProcessList[i];
            const std::string name = ToUtf8(pe.name);
            if (name.empty()) continue;
            if (!filter.empty() && Lower(name).find(filter) == std::string::npos) continue;

            const ImVec2 rp(base.x, base.y + shown * rh);
            ImGui::PushID((int)i);
            ImGui::SetCursorScreenPos(rp);
            const bool clicked = ImGui::InvisibleButton("row", V2(availW, rh - 2.0f));
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();

            if (hov)
                cdl->AddRectFilled(rp, V2(rp.x + availW, rp.y + rh - 2.0f), Neo::kRowHover, 9.0f);

            const std::string arch = ToUtf8(pe.arch);
            const std::string meta = "PID " + std::to_string((unsigned long)pe.pid) +
                                     (arch.empty() ? std::string() : ("   " + arch));
            const float metaW = TxtW(m_fontRegular, 11.0f, meta.c_str());

            Txt(cdl, m_fontRegular, 12.5f, V2(rp.x + 14.0f, rp.y + 8.0f),
                hov ? Neo::kTextPri : Neo::kTextSec,
                Fit(m_fontRegular, 12.5f, name, availW - 28.0f - metaW - 16.0f).c_str());
            Txt(cdl, m_fontRegular, 11.0f, V2(rp.x + availW - 14.0f - metaW, rp.y + 9.5f),
                Neo::kTextMut, meta.c_str());

            if (clicked) {
                P.exeName = name;
                strncpy_s(m_bufProcessName, name.c_str(), _TRUNCATE);
                ConfigManager::Get().Save();
                m_showProcessPicker = false;
                Toast("Target set to " + name);
                AddLog("target process -> " + name);
                break;
            }
            ++shown;
        }
        if (shown == 0)
            TxtC(cdl, m_fontRegular, 12.0f, base.x + availW * 0.5f, base.y + 24.0f,
                 Neo::kTextMut, "No matching process");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}

// ============================================================================
//  profile picker
// ============================================================================
void VibeUI::RenderProfilePickerModal()
{
    ConfigManager& cm = ConfigManager::Get();
    LoaderSettings& S = cm.Settings();

    const int n = ImMax((int)S.profiles.size(), 1);
    const float W = 440.0f;
    const float H = 118.0f + 56.0f * (float)n;
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const ImVec2 mn((disp.x - W) * 0.5f, (disp.y - H) * 0.5f);
    const ImVec2 mx(mn.x + W, mn.y + H);

    ImGui::SetNextWindowPos(mn);
    ImGui::SetNextWindowSize(V2(W, H));
    ImGui::Begin("##modal_profiles", nullptr, kModalFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(mn, mx, IM_COL32(40, 40, 46, 232), 20.0f);
    Neo::Glass(dl, mn, mx, 20.0f, 1.0f, false, 26.0f, V2(0.0f, 12.0f));

    Neo::TextSpaced(dl, m_fontSmall, 9.0f, V2(mn.x + 26.0f, mn.y + 22.0f),
                    Neo::kTextMut, "PROFILE", 1.3f);
    Txt(dl, m_fontBold, 14.0f, V2(mn.x + 26.0f, mn.y + 38.0f), Neo::kTextPri,
        "Select a target profile");

    if (IconButton("##prof_close", Neo::Icon_Close, V2(mx.x - 44.0f, mn.y + 20.0f),
                   V2(mx.x - 20.0f, mn.y + 44.0f), "Close", true))
        m_showProfilePicker = false;

    const float rowTop = mn.y + 76.0f;
    const float rowH   = 49.0f;

    for (int i = 0; i < (int)S.profiles.size(); ++i) {
        TargetProfile& pr = S.profiles[(size_t)i];
        const bool active = (i == S.activeProfileIndex);
        const ImVec2 rp(mn.x + 20.0f, rowTop + i * 56.0f);
        const ImVec2 rq(mx.x - 20.0f, rp.y + rowH);

        ImGui::PushID(200 + i);
        ImGui::SetCursorScreenPos(rp);
        const bool clicked = ImGui::InvisibleButton("row", V2(rq.x - rp.x, rq.y - rp.y));
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();

        if (active)
            Neo::GradientRect(dl, rp, rq, 12.0f,
                              Neo::AC(Neo::Accent(), 0.22f), Neo::AC(Neo::AccentLo(), 0.14f));
        else if (hov)
            dl->AddRectFilled(rp, rq, Neo::kRowHover, 12.0f);
        dl->AddRect(rp, rq, active ? Neo::AC(Neo::AccentHi(), 0.42f) : Neo::kBorder, 12.0f, 1.0f);

        if (active)
            dl->AddRectFilled(V2(rp.x + 1.0f, rp.y + 12.0f), V2(rp.x + 4.0f, rq.y - 12.0f),
                              Neo::AccentHi(), 2.0f);

        // avatar
        const ImVec2 av(rp.x + 32.0f, (rp.y + rq.y) * 0.5f);
        dl->AddCircleFilled(av, 14.0f, active ? Neo::AC(Neo::Accent(), 0.85f)
                                               : IM_COL32(255, 255, 255, 16), 32);
        {
            char ini[3] = { 0, 0, 0 };
            if (!pr.name.empty()) {
                ini[0] = (char)toupper((unsigned char)pr.name[0]);
                if (pr.name.size() > 1) ini[1] = (char)toupper((unsigned char)pr.name[1]);
            }
            ImVec2 iw = m_fontBold->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, ini);
            Txt(dl, m_fontBold, 11.0f, V2(av.x - iw.x * 0.5f, av.y - iw.y * 0.5f),
                Neo::kTextPri, ini);
        }

        Txt(dl, m_fontBold, 12.5f, V2(rp.x + 56.0f, rp.y + 8.0f),
            active ? Neo::kTextPri : Neo::kTextSec,
            Fit(m_fontBold, 12.5f, pr.name.empty() ? std::string("(unnamed)") : pr.name,
                rq.x - rp.x - 56.0f - 78.0f).c_str());
        Txt(dl, m_fontRegular, 10.5f, V2(rp.x + 56.0f, rp.y + 27.0f), Neo::kTextMut,
            Fit(m_fontRegular, 10.5f, pr.exeName, rq.x - rp.x - 56.0f - 78.0f).c_str());

        if (active) {
            const char* badge = "ACTIVE";
            const float bw = Neo::TextSpacedSize(m_fontSmall, 8.5f, badge, 1.1f).x;
            const ImVec2 bmn(rq.x - 18.0f - bw - 18.0f, (rp.y + rq.y) * 0.5f - 10.0f);
            const ImVec2 bmx(rq.x - 18.0f, (rp.y + rq.y) * 0.5f + 10.0f);
            Neo::GradientRect(dl, bmn, bmx, 8.0f,
                              Neo::AC(Neo::AccentHi(), 0.30f), Neo::AC(Neo::AccentLo(), 0.22f));
            dl->AddRect(bmn, bmx, Neo::AC(Neo::AccentHi(), 0.40f), 8.0f, 1.0f);
            Neo::TextSpaced(dl, m_fontSmall, 8.5f, V2(bmn.x + 9.0f, bmn.y + 5.0f),
                            Neo::AccentHi(), badge, 1.1f);
        }

        if (clicked) {
            S.activeProfileIndex = i;
            strncpy_s(m_bufProcessName, pr.exeName.c_str(), _TRUNCATE);
            strncpy_s(m_bufDllPath, pr.dllPath.c_str(), _TRUNCATE);
            strncpy_s(m_bufGithubRepo, pr.githubRepo.c_str(), _TRUNCATE);
            strncpy_s(m_bufDirectUrl, pr.directUrl.c_str(), _TRUNCATE);
            cm.Save();
            m_showProfilePicker = false;
            m_cachedTargetPid = 0;
            m_procCheckTimer = 0.0f;
            Toast("Profile: " + pr.name);
            AddLog("active profile -> " + pr.name);
        }
    }

    // new profile ----------------------------------------------------------
    {
        const ImVec2 bmn(mn.x + 20.0f, mx.y - 50.0f);
        const ImVec2 bmx(mx.x - 20.0f, mx.y - 18.0f);
        if (GlassButton("##prof_new", "New profile", bmn, bmx, 1, Neo::Icon_None)) {
            TargetProfile np;
            np.name       = "Custom Game " + std::to_string((int)S.profiles.size() + 1);
            np.exeName    = "game.exe";
            np.dllPath    = "payloads/custom.dll";
            np.version    = "v1.0";
            np.statusText = "Undetected";
            S.profiles.push_back(np);
            cm.Save();
            m_showProfilePicker = false;
            Toast("Profile created");
        }
    }

    ImGui::End();
}

// ============================================================================
//  actions
// ============================================================================
void VibeUI::TriggerInjection()
{
    ConfigManager& cm = ConfigManager::Get();
    TargetProfile&  P = cm.GetActiveProfile();

    if (P.exeName.empty()) {
        Toast("No target executable configured");
        m_injectMessageTimer = 3.0f;
        return;
    }

    std::string dll = m_bufDllPath[0] ? std::string(m_bufDllPath) : P.dllPath;
    if (dll.empty()) {
        Toast("No payload module selected");
        m_injectMessageTimer = 3.0f;
        return;
    }

    if (m_cachedTargetPid == 0) {
        const std::wstring exeW = ToWide(P.exeName);
        if (!exeW.empty()) m_cachedTargetPid = Memory::GetPID(exeW.c_str());
        m_cachedTargetRunning = (m_cachedTargetPid != 0);
    }

    if (m_cachedTargetPid == 0) {
        m_isWaitingForGame = true;
        m_waitTimer        = 90.0f;
        m_lastInjectLog    = "waiting for " + P.exeName + " ...";
        AddLog(m_lastInjectLog);
        Toast("Waiting for " + P.exeName);
        return;
    }

    const DWORD pid = m_cachedTargetPid;
    m_isInjecting   = true;
    m_injectSuccess = false;
    m_closeAt       = -1.0f;
    m_lastInjectLog = "injecting payload -> pid " + std::to_string((unsigned long)pid);
    AddLog(m_lastInjectLog);

    std::thread([this, pid, dll]() {
        std::string out;
        const bool ok = Memory::InjectDLL(pid, dll, out);
        {
            std::lock_guard<std::mutex> lk(m_injectLock);
            m_injectOut = out;
        }
        m_injectOk.store(ok);
        m_injectDone.store(true);
    }).detach();
}

void VibeUI::TriggerUpdateCheck()
{
    ConfigManager& cm = ConfigManager::Get();
    TargetProfile&  P = cm.GetActiveProfile();

    UpdateConfig cfg;
    cfg.githubRepo     = P.githubRepo.empty()     ? std::string(m_bufGithubRepo)   : P.githubRepo;
    cfg.assetPattern   = m_bufAssetPattern[0]     ? std::string(m_bufAssetPattern) : ".dll";
    cfg.directUrl      = P.directUrl.empty()      ? std::string(m_bufDirectUrl)    : P.directUrl;
    cfg.localSavePath  = m_bufDllPath[0]          ? std::string(m_bufDllPath)      : P.dllPath;
    cfg.currentVersion = P.version;
    cfg.isSilent       = true;

    m_updater.CheckDllSilentAsync(cfg);
    Toast("Checking payload source...");
    AddLog("checking payload source...");
}

// ============================================================================
//  file dialog
// ============================================================================
void VibeUI::OpenDllFileDialog()
{
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        Toast("Could not open the file dialog");
        return;
    }

    COMDLG_FILTERSPEC filter[] = { { L"Dynamic Link Libraries (*.dll)", L"*.dll" },
                                   { L"All files (*.*)", L"*.*" } };
    dlg->SetFileTypes(2, filter);
    dlg->SetTitle(L"Select payload module");

    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                const std::wstring w(path);
                const std::string utf8 = ToUtf8(w);
                strncpy_s(m_bufDllPath, utf8.c_str(), _TRUNCATE);
                ConfigManager::Get().GetActiveProfile().dllPath = utf8;
                ConfigManager::Get().Save();
                Toast("Payload module selected");
                AddLog("payload -> " + utf8);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
}
