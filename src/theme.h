#pragma once
// ============================================================================
//  NEO - glassmorphism design system
//  Palette, vector logo, icon set and pure-drawlist painting helpers.
// ----------------------------------------------------------------------------
//  ImGui 1.93 note: AddRect/AddPolyline/PathStroke had 'thickness' and 'flags'
//  SWAPPED in 1.92.8. Every call below uses the NEW order and passes exactly
//  the argument count that makes the obsolete overload ineligible.
// ============================================================================
#include "imgui.h"
#include "imgui_internal.h"   // ImMax / ImMin / ImClamp (moved out of imgui.h in 1.92)

namespace Neo {

// ---------------------------------------------------------------- palette --
// Neutral charcoal glass laid over a REAL acrylic surface: the frame is a DWM
// backdrop seen through a premultiplied DirectComposition swapchain, so every
// colour below only TINTS whatever is behind the window. Keep the surface
// alphas low or the blur stops showing through, and keep the greys neutral -
// a blue-violet cast here makes the whole UI read as blue.
constexpr ImU32 kBgTop      = IM_COL32( 40,  40,  45, 132);
constexpr ImU32 kBgBot      = IM_COL32( 17,  17,  20, 156);
constexpr ImU32 kPanelTop   = IM_COL32(106, 106, 115, 156);
constexpr ImU32 kPanelBot   = IM_COL32( 62,  62,  70, 148);
constexpr ImU32 kCardTop    = IM_COL32(116, 116, 126, 158);
constexpr ImU32 kCardBot    = IM_COL32( 66,  66,  75, 150);
constexpr ImU32 kInnerTop   = IM_COL32(255, 255, 255,  20);
constexpr ImU32 kInnerBot   = IM_COL32(255, 255, 255,   6);
constexpr ImU32 kRowHover   = IM_COL32(255, 255, 255,  18);
constexpr ImU32 kRowActive  = IM_COL32(255, 255, 255,  26);
constexpr ImU32 kBorder     = IM_COL32(255, 255, 255,  28);
constexpr ImU32 kBorderHi   = IM_COL32(255, 255, 255,  52);
constexpr ImU32 kTrack      = IM_COL32(255, 255, 255,  20);

// Violet accent. Deliberately saturated and mid-dark: a pale violet over a
// cool background reads periwinkle/blue, which is exactly what we must avoid.
constexpr ImU32 kAccent     = IM_COL32(157,  60, 232, 255); // #9d3ce8
constexpr ImU32 kAccentHi   = IM_COL32(184, 104, 244, 255); // #b868f4
constexpr ImU32 kAccentLo   = IM_COL32(101,  26, 172, 255); // #651aac

// The neo wordmark's own colour, sampled straight out of neo.png.
constexpr ImU32 kBrand      = IM_COL32(  4, 124, 255, 255); // #047cff

constexpr ImU32 kTextPri    = IM_COL32(246, 246, 248, 255);
constexpr ImU32 kTextSec    = IM_COL32(190, 190, 199, 255);
constexpr ImU32 kTextMut    = IM_COL32(140, 140, 152, 255);
constexpr ImU32 kGreen      = IM_COL32( 74, 222, 128, 255);
constexpr ImU32 kRed        = IM_COL32(248, 113, 113, 255);
constexpr ImU32 kYellow     = IM_COL32(250, 204,  21, 255);
constexpr ImU32 kBlue       = IM_COL32( 96, 165, 250, 255);

// The runtime accent can be re-tinted from the Settings page. Everything that
// wants the "live" accent must call Accent()/AccentHi()/AccentLo().
void  SetAccent(ImU32 base);
ImU32 Accent();
ImU32 AccentHi();
ImU32 AccentLo();

// ------------------------------------------------------------ colour maths --
inline ImU32 AC(ImU32 c, float mul) {
    int a = (int)(((c >> IM_COL32_A_SHIFT) & 0xFF) * mul);
    if (a < 0) a = 0; else if (a > 255) a = 255;
    return (c & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
}
inline ImU32 Mix(ImU32 a, ImU32 b, float t) {
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    const float ia = 1.0f - t;
    const int r = (int)(((a >> IM_COL32_R_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_R_SHIFT) & 0xFF) * t);
    const int g = (int)(((a >> IM_COL32_G_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_G_SHIFT) & 0xFF) * t);
    const int bl= (int)(((a >> IM_COL32_B_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_B_SHIFT) & 0xFF) * t);
    const int al= (int)(((a >> IM_COL32_A_SHIFT) & 0xFF) * ia + ((b >> IM_COL32_A_SHIFT) & 0xFF) * t);
    return IM_COL32(r, g, bl, al);
}
// Vertical two-stop gradient clipped to a rounded rectangle.
void  GradientRect(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, ImU32 top, ImU32 bot);
// Horizontal two-stop gradient clipped to a rounded rectangle.
void  GradientRectH(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, ImU32 left, ImU32 right);

// ------------------------------------------------------------------ paint --
void  Backdrop(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float time);
void  Blob(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, float alpha);
void  SoftShadow(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, float spread,
                 float alpha, ImVec2 offset);
// Frosted "glassmorphism" panel: drop shadow + gradient body + inner sheen +
// hairline border. `strength` 0..1 scales the body opacity.
void  Glass(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, float strength,
            bool hovered, float shadowSpread, ImVec2 shadowOffset);
void  HairLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col);

// ------------------------------------------------------------------- logo --
float NeoLogoWidth(float height);
void  NeoLogo(ImDrawList* dl, ImVec2 origin, float height, ImU32 col, float strokeMul);

// ------------------------------------------------------------------ icons --
enum IconKind {
    Icon_None = 0,
    Icon_Bolt, Icon_Crosshair, Icon_Cube, Icon_Gear, Icon_Info,
    Icon_User, Icon_Chip, Icon_Shield, Icon_Clock, Icon_Folder,
    Icon_Download, Icon_Refresh, Icon_Search, Icon_Close, Icon_Minimize,
    Icon_Check, Icon_Chevron, Icon_Power, Icon_Doc, Icon_Globe, Icon_Spark,
    Icon_Link, Icon_Trash
};
void  Icon(ImDrawList* dl, IconKind kind, ImVec2 center, float size, ImU32 col, float thickness);
void  Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, float t, ImU32 col);
void  PulseRing(ImDrawList* dl, ImVec2 center, float radius, float t, ImU32 col);

// --------------------------------------------------------------- text util --
void  TextSpaced(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col,
                 const char* text, float spacing);
ImVec2 TextSpacedSize(ImFont* font, float size, const char* text, float spacing);

} // namespace Neo
