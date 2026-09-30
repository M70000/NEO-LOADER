#include "theme.h"
#include <cmath>
#include <cstring>

// ============================================================================
//  Accent
// ============================================================================
static ImU32 g_Accent   = Neo::kAccent;
static ImU32 g_AccentHi = Neo::kAccentHi;
static ImU32 g_AccentLo = Neo::kAccentLo;

void  Neo::SetAccent(ImU32 base) {
    g_Accent   = base;
    g_AccentHi = Mix(base, IM_COL32(255, 255, 255, 255), 0.24f);
    g_AccentLo = Mix(base, IM_COL32(0, 0, 0, 255), 0.30f);
}
ImU32 Neo::Accent()   { return g_Accent; }
ImU32 Neo::AccentHi() { return g_AccentHi; }
ImU32 Neo::AccentLo() { return g_AccentLo; }

// ============================================================================
//  Gradients
// ============================================================================
void Neo::GradientRectH(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, ImU32 left, ImU32 right)
{
    if (mx.x <= mn.x || mx.y <= mn.y) return;
    if (rounding <= 0.5f) { dl->AddRectFilledMultiColor(mn, mx, left, right, right, left); return; }

    const float w = mx.x - mn.x;
    const int   slices = (int)ceilf(w / 1.0f);
    const float r = rounding;
    for (int i = 0; i < slices; ++i) {
        const float x0 = mn.x + (w * (float)i / (float)slices);
        const float x1 = mn.x + (w * (float)(i + 1) / (float)slices);
        const float xm = (x0 + x1) * 0.5f;
        float dx = 0.0f;
        if (xm < mn.x + r)      dx = (mn.x + r) - xm;
        else if (xm > mx.x - r) dx = xm - (mx.x - r);
        float inset = 0.0f;
        if (dx > 0.0f) {
            const float k = r * r - dx * dx;
            inset = r - (k > 0.0f ? sqrtf(k) : 0.0f);
        }
        const float t = (float)i / (float)(slices - 1 > 0 ? slices - 1 : 1);
        dl->AddRectFilled(ImVec2(x0, mn.y + inset), ImVec2(x1, mx.y - inset),
                          Mix(left, right, t));
    }
}

void Neo::GradientRect(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, ImU32 top, ImU32 bot)
{
    if (mx.x <= mn.x || mx.y <= mn.y) return;
    if (rounding <= 0.5f) { dl->AddRectFilledMultiColor(mn, mx, top, top, bot, bot); return; }

    const float h = mx.y - mn.y;
    const float step = 1.0f;
    const int   slices = (int)ceilf(h / step);
    const float r = rounding;
    for (int i = 0; i < slices; ++i) {
        const float y0 = mn.y + (h * (float)i / (float)slices);
        const float y1 = mn.y + (h * (float)(i + 1) / (float)slices);
        const float ym = (y0 + y1) * 0.5f;
        float dy = 0.0f;
        if (ym < mn.y + r)      dy = (mn.y + r) - ym;
        else if (ym > mx.y - r) dy = ym - (mx.y - r);
        float inset = 0.0f;
        if (dy > 0.0f) {
            const float k = r * r - dy * dy;
            inset = r - (k > 0.0f ? sqrtf(k) : 0.0f);
        }
        const float t = (float)i / (float)(slices - 1 > 0 ? slices - 1 : 1);
        const ImU32 col = Mix(top, bot, t);
        dl->AddRectFilled(ImVec2(mn.x + inset, y0), ImVec2(mx.x - inset, y1), col);
    }
}

void Neo::HairLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col)
{
    dl->AddLine(a, b, col, 1.0f);
}

// ============================================================================
//  Backdrop  (deep purple gradient + drifting colour blobs + vignette)
// ============================================================================
void Neo::Blob(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, float alpha)
{
    const int steps = 26;
    const int seg   = 44;
    for (int i = 0; i < steps; ++i) {
        const float t  = (float)i / (float)(steps - 1);
        const float rr = radius * (1.0f - t * 0.985f);
        const float a  = alpha * (1.0f - t) * (4.0f / (float)steps);
        dl->AddCircleFilled(center, rr, AC(col, a), seg);
    }
}

void Neo::Backdrop(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float time)
{
    const float w = mx.x - mn.x;
    const float h = mx.y - mn.y;

    // Neutral charcoal tint over the acrylic backdrop. No blue anywhere: the
    // window must read as smoked grey glass, not as a blue-violet wash.
    dl->AddRectFilledMultiColor(mn, mx, kBgTop, kBgTop, kBgBot, kBgBot);

    const float t = time * 0.11f;
    // One faint violet bloom keeps the surface from looking flat. It sits at
    // the very edge so it reads as a light leak rather than as a colour cast.
    Blob(dl, ImVec2(mn.x + w * (0.52f + 0.06f * sinf(t)),
                    mn.y + h * (-0.34f + 0.05f * cosf(t * 0.90f))), w * 0.70f,
         IM_COL32(150,  74, 232, 255), 0.055f);

    // Soft edge falloff: thin, very low-alpha rings so they read as a gradient
    // instead of banding.
    for (int i = 0; i < 10; ++i) {
        const float e = ((float)i / 10.0f) * 6.0f;
        dl->AddRect(ImVec2(mn.x + e, mn.y + e), ImVec2(mx.x - e, mx.y - e),
                    IM_COL32(0, 0, 0, 7), 0.0f, 1.0f);
    }
}

// ============================================================================
//  Soft drop shadow (dark, wide, layered)
// ============================================================================
void Neo::SoftShadow(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding,
                     float spread, float alpha, ImVec2 offset)
{
    const int steps = 16;
    for (int i = steps; i >= 1; --i) {
        const float t = (float)i / (float)steps;      // 1 = outermost
        const float e = spread * t;
        const float a = alpha * (1.0f - t) * (1.0f - t) * 0.50f + alpha * 0.03f;
        dl->AddRectFilled(ImVec2(mn.x - e + offset.x, mn.y - e + offset.y),
                          ImVec2(mx.x + e + offset.x, mx.y + e + offset.y),
                          IM_COL32(3, 3, 5, (int)(a * 255.0f)), rounding + e);
    }
}

// ============================================================================
//  Glass panel
// ============================================================================
void Neo::Glass(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding, float strength,
                bool hovered, float shadowSpread, ImVec2 shadowOffset)
{
    SoftShadow(dl, mn, mx, rounding, shadowSpread, hovered ? 1.15f : 0.95f, shadowOffset);

    const ImU32 bodyTop = AC(kCardTop, strength);
    const ImU32 bodyBot = AC(kCardBot, strength);
    GradientRect(dl, mn, mx, rounding, bodyTop, bodyBot);

    // Inner sheen along the top third.
    const ImU32 sheenHi = AC(IM_COL32(255, 255, 255, 255), hovered ? 0.075f : 0.055f);
    const ImU32 sheenLo = IM_COL32(255, 255, 255, 0);
    GradientRect(dl, ImVec2(mn.x + 1.0f, mn.y + 1.0f),
                 ImVec2(mx.x - 1.0f, mn.y + (mx.y - mn.y) * 0.38f),
                 ImMax(rounding - 1.0f, 0.0f), sheenHi, sheenLo);

    // Hairline border + bright top edge.
    dl->AddRect(mn, mx, hovered ? kBorderHi : kBorder, rounding, hovered ? 1.35f : 1.0f);
    dl->AddLine(ImVec2(mn.x + rounding * 0.9f, mn.y + 1.0f),
                ImVec2(mx.x - rounding * 0.9f, mn.y + 1.0f),
                hovered ? IM_COL32(255, 255, 255, 46) : IM_COL32(255, 255, 255, 30), 1.0f);
}

// ============================================================================
//  neo wordmark  (vector reconstruction of neo.png)
// ----------------------------------------------------------------------------
//  Source art is 409x162 with uniform 21px monoline strokes; the ink box is
//  x:18..394, y:20..144 (376x124). Circles "e"@(209,82) and "o"@(332,82) and the
//  "n" shoulder all use centre-line radius 51.5.
// ============================================================================
float Neo::NeoLogoWidth(float height) { return height * (376.0f / 124.0f); }

void Neo::NeoLogo(ImDrawList* dl, ImVec2 origin, float height, ImU32 col, float strokeMul)
{
    const float s  = height / 124.0f;
    const float th = 21.0f * s * strokeMul;
    const float R  = 51.5f;
    const float D  = 3.14159265358979f / 180.0f;
    const float capR = th * 0.5f;

    auto P = [&](float x, float y) {
        return ImVec2(origin.x + (x - 18.0f) * s, origin.y + (y - 20.0f) * s);
    };
    auto cap = [&](float x, float y) { dl->AddCircleFilled(P(x, y), capR, col, 14); };

    // ---- n : left stem, shoulder arc, right stem
    dl->PathLineTo(P(28.0f, 133.5f));
    dl->PathLineTo(P(28.0f, 82.0f));
    dl->PathArcTo(P(79.5f, 82.0f), R * s, 180.0f * D, 360.0f * D, 40);
    dl->PathLineTo(P(131.0f, 133.5f));
    dl->PathStroke(col, th);
    cap(28.0f, 133.5f);
    cap(131.0f, 133.5f);

    // ---- e : broken ring + slanted crossbar
    dl->PathArcTo(P(209.0f, 82.0f), R * s, 24.0f * D, 323.0f * D, 44);
    dl->PathStroke(col, th);
    cap(250.2f, 51.0f);
    dl->PathLineTo(P(250.2f, 51.0f));
    dl->PathLineTo(P(213.5f, 100.5f));
    dl->PathStroke(col, th);
    cap(213.5f, 100.5f);

    // ---- o : broken ring + slanted spur (parallel to the e crossbar)
    dl->PathArcTo(P(332.0f, 82.0f), R * s, 217.0f * D, 516.0f * D, 44);
    dl->PathStroke(col, th);
    cap(290.9f, 51.0f);
    dl->PathLineTo(P(290.9f, 51.0f));
    dl->PathLineTo(P(240.0f, 122.0f));
    dl->PathStroke(col, th);
    cap(240.0f, 122.0f);
}

// ============================================================================
//  Icons
// ============================================================================
namespace {
struct IconPath {
    ImVec2 pts[72];
    int    n;
    ImVec2 c;
    float  h;
    void push(float x, float y) { if (n < 72) pts[n++] = ImVec2(c.x + x * h, c.y + y * h); }
};
static void StrokePts(ImDrawList* dl, const ImVec2* p, int n, ImU32 col, float th, bool closed)
{
    if (n < 2) return;
    dl->AddPolyline(p, n, col, th, closed ? ImDrawFlags_Closed : 0);
    if (!closed) {
        dl->AddCircleFilled(p[0], th * 0.5f, col, 12);
        dl->AddCircleFilled(p[n - 1], th * 0.5f, col, 12);
    }
}
static void ArcPts(IconPath& ip, float cx, float cy, float r, float a0, float a1, int seg)
{
    for (int i = 0; i <= seg; ++i) {
        const float a = a0 + (a1 - a0) * (float)i / (float)seg;
        ip.push(cx + r * cosf(a), cy + r * sinf(a));
    }
}
}

void Neo::Icon(ImDrawList* dl, IconKind kind, ImVec2 c, float size, ImU32 col, float thickness)
{
    if (kind == Icon_None) return;
    const float h = size * 0.5f;
    const float D = 3.14159265358979f / 180.0f;

    auto P = [&](float x, float y) { return ImVec2(c.x + x * h, c.y + y * h); };
    auto Line = [&](float x0, float y0, float x1, float y1) {
        ImVec2 p[2] = { P(x0, y0), P(x1, y1) };
        dl->AddPolyline(p, 2, col, thickness, 0);
        dl->AddCircleFilled(p[0], thickness * 0.5f, col, 10);
        dl->AddCircleFilled(p[1], thickness * 0.5f, col, 10);
    };
    auto Poly = [&](const float* xy, int n, bool closed) {
        ImVec2 p[72];
        if (n > 72) n = 72;
        for (int i = 0; i < n; ++i) p[i] = P(xy[i * 2], xy[i * 2 + 1]);
        StrokePts(dl, p, n, col, thickness, closed);
    };
    auto Circle = [&](float cx, float cy, float r) {
        dl->AddCircle(P(cx, cy), r * h, col, 40, thickness);
    };
    auto Dot = [&](float cx, float cy, float r) {
        dl->AddCircleFilled(P(cx, cy), r * h, col, 16);
    };
    auto Arc = [&](float cx, float cy, float r, float a0, float a1, int seg) {
        IconPath ip; ip.n = 0; ip.c = c; ip.h = h;
        ArcPts(ip, cx, cy, r, a0 * D, a1 * D, seg);
        StrokePts(dl, ip.pts, ip.n, col, thickness, false);
    };
    auto Rect = [&](float x0, float y0, float x1, float y1, float rnd) {
        dl->AddRect(P(x0, y0), P(x1, y1), col, rnd * h, thickness);
    };

    switch (kind) {
    case Icon_Bolt: {
        const float p[] = { 0.16f,-1.00f, -0.58f,0.10f, -0.06f,0.10f,
                           -0.22f, 1.00f,  0.58f,-0.12f,  0.06f,-0.12f };
        Poly(p, 6, true);
    } break;

    case Icon_Crosshair: {
        Circle(0, 0, 0.66f);
        Line(0, -1.0f, 0, -0.60f); Line(0, 0.60f, 0, 1.0f);
        Line(-1.0f, 0, -0.60f, 0); Line(0.60f, 0, 1.0f, 0);
    } break;

    case Icon_Cube: {
        const float hex[] = { 0,0.92f, -0.80f,0.46f, -0.80f,-0.46f,
                              0,-0.92f, 0.80f,-0.46f, 0.80f,0.46f };
        Poly(hex, 6, true);
        Line(0, 0, 0, -0.92f);
        Line(0, 0, -0.80f, 0.46f);
        Line(0, 0, 0.80f, 0.46f);
    } break;

    case Icon_Gear: {
        ImVec2 p[16];
        for (int i = 0; i < 16; ++i) {
            const float a = (float)i * 22.5f * D;
            const float r = (i % 2 == 0) ? 1.0f : 0.73f;
            p[i] = P(cosf(a) * r, sinf(a) * r);
        }
        dl->AddPolyline(p, 16, col, thickness, ImDrawFlags_Closed);
        Circle(0, 0, 0.31f);
    } break;

    case Icon_Info: {
        Circle(0, 0, 0.92f);
        Dot(0, -0.44f, 0.13f);
        Line(0, -0.08f, 0, 0.52f);
    } break;

    case Icon_User: {
        Circle(0, -0.40f, 0.36f);
        Arc(0, 1.18f, 0.92f, 200.0f, 340.0f, 24);
    } break;

    case Icon_Chip: {
        Rect(-0.62f, -0.62f, 0.62f, 0.62f, 0.20f);
        Rect(-0.24f, -0.24f, 0.24f, 0.24f, 0.07f);
        Line(-0.30f, -0.62f, -0.30f, -1.00f); Line(0.30f, -0.62f, 0.30f, -1.00f);
        Line(-0.30f,  0.62f, -0.30f,  1.00f); Line(0.30f,  0.62f, 0.30f,  1.00f);
        Line(-0.62f, -0.30f, -1.00f, -0.30f); Line(-0.62f, 0.30f, -1.00f, 0.30f);
        Line( 0.62f, -0.30f,  1.00f, -0.30f); Line( 0.62f, 0.30f,  1.00f, 0.30f);
    } break;

    case Icon_Shield: {
        const float p[] = { 0,-1.00f, 0.86f,-0.60f, 0.86f,0.10f,
                            0, 1.00f, -0.86f, 0.10f, -0.86f,-0.60f };
        Poly(p, 6, true);
    } break;

    case Icon_Clock: {
        Circle(0, 0, 0.92f);
        Line(0, 0, 0, -0.56f);
        Line(0, 0, 0.44f, 0.10f);
    } break;

    case Icon_Folder: {
        const float p[] = { -1.0f,0.66f, -1.0f,-0.62f, -0.30f,-0.62f,
                             0.02f,-0.26f, 1.0f,-0.26f, 1.0f,0.66f };
        Poly(p, 6, true);
    } break;

    case Icon_Download: {
        Line(0, -0.95f, 0, 0.40f);
        const float head[] = { -0.42f,-0.02f, 0,0.42f, 0.42f,-0.02f };
        Poly(head, 3, false);
        const float tray[] = { -0.88f,0.66f, -0.88f,0.94f, 0.88f,0.94f, 0.88f,0.66f };
        Poly(tray, 4, false);
    } break;

    case Icon_Refresh: {
        Arc(0, 0, 0.84f, 62.0f, 360.0f, 30);
        const float head[] = { 0.10f,0.60f, 0.42f,0.72f, 0.50f,0.38f };
        Poly(head, 3, false);
    } break;

    case Icon_Search: {
        Circle(-0.18f, -0.18f, 0.60f);
        Line(0.28f, 0.28f, 0.92f, 0.92f);
    } break;

    case Icon_Close: {
        Line(-0.72f, -0.72f, 0.72f, 0.72f);
        Line( 0.72f, -0.72f, -0.72f, 0.72f);
    } break;

    case Icon_Minimize: {
        Line(-0.80f, 0.0f, 0.80f, 0.0f);
    } break;

    case Icon_Check: {
        const float p[] = { -0.82f,0.02f, -0.26f,0.60f, 0.84f,-0.68f };
        Poly(p, 3, false);
    } break;

    case Icon_Chevron: {
        const float p[] = { -0.36f,-0.74f, 0.38f,0.0f, -0.36f,0.74f };
        Poly(p, 3, false);
    } break;

    case Icon_Power: {
        Arc(0, 0.08f, 0.86f, 300.0f, 600.0f, 30);
        Line(0, -1.0f, 0, 0.08f);
    } break;

    case Icon_Doc: {
        Rect(-0.68f, -0.94f, 0.68f, 0.94f, 0.18f);
        Line(-0.34f, -0.42f, 0.34f, -0.42f);
        Line(-0.34f,  0.02f, 0.34f,  0.02f);
        Line(-0.34f,  0.46f, 0.10f,  0.46f);
    } break;

    case Icon_Globe: {
        Circle(0, 0, 0.92f);
        dl->AddEllipse(P(0, 0), ImVec2(0.46f * h, 0.92f * h), col, 0.0f, 36, thickness);
        dl->AddEllipse(P(0, 0), ImVec2(0.92f * h, 0.42f * h), col, 0.0f, 36, thickness);
    } break;

    case Icon_Spark: {
        const float p[] = { 0,-1.0f, 0.24f,-0.24f, 1.0f,0, 0.24f,0.24f,
                            0, 1.0f, -0.24f, 0.24f, -1.0f,0, -0.24f,-0.24f };
        Poly(p, 8, true);
    } break;

    case Icon_Link: {
        Circle(-0.34f, -0.34f, 0.44f);
        Circle( 0.34f,  0.34f, 0.44f);
        Line(-0.10f, -0.10f, 0.10f, 0.10f);
    } break;

    case Icon_Trash: {
        Line(-0.94f, -0.60f, 0.94f, -0.60f);
        const float handle[] = { -0.34f,-0.92f, 0.34f,-0.92f };
        Poly(handle, 2, false);
        Line(-0.34f, -0.92f, -0.34f, -0.60f);
        Line( 0.34f, -0.92f,  0.34f, -0.60f);
        const float body[] = { -0.74f,-0.60f, -0.62f,0.94f, 0.62f,0.94f, 0.74f,-0.60f };
        Poly(body, 4, false);
        Line(-0.26f, -0.30f, -0.20f, 0.66f);
        Line( 0.26f, -0.30f,  0.20f, 0.66f);
    } break;

    default: break;
    }
}

// ============================================================================
//  Motion helpers
// ============================================================================
void Neo::Spinner(ImDrawList* dl, ImVec2 c, float radius, float thickness, float t, ImU32 col)
{
    const int   seg   = 44;
    const float sweep = 4.55f;              // ~260 degrees
    const float a0    = t * 4.2f;
    for (int i = 0; i < seg; ++i) {
        const float u0 = (float)i / (float)seg;
        const float u1 = (float)(i + 1) / (float)seg;
        const float aa = a0 + sweep * u0;
        const float ab = a0 + sweep * u1;
        const ImU32 cc = AC(col, 0.05f + 0.95f * u0 * u0);
        ImVec2 p[2] = { ImVec2(c.x + cosf(aa) * radius, c.y + sinf(aa) * radius),
                        ImVec2(c.x + cosf(ab) * radius, c.y + sinf(ab) * radius) };
        dl->AddPolyline(p, 2, cc, thickness, 0);
    }
    const float ah = a0 + sweep;
    dl->AddCircleFilled(ImVec2(c.x + cosf(ah) * radius, c.y + sinf(ah) * radius),
                        thickness * 0.5f, col, 12);
}

void Neo::PulseRing(ImDrawList* dl, ImVec2 c, float radius, float t, ImU32 col)
{
    const float u = t - floorf(t);
    dl->AddCircle(c, radius * (0.55f + 0.85f * u), AC(col, (1.0f - u) * 0.60f), 44, 1.8f);
}

// ============================================================================
//  Letter-spaced text (fake tracking for the small caps labels)
// ============================================================================
void Neo::TextSpaced(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col,
                     const char* text, float spacing)
{
    if (!font || !text) return;
    float x = pos.x;
    for (const char* p = text; *p; ++p) {
        const char* q = p + 1;
        dl->AddText(font, size, ImVec2(x, pos.y), col, p, q);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, q).x + spacing;
    }
}

ImVec2 Neo::TextSpacedSize(ImFont* font, float size, const char* text, float spacing)
{
    if (!font || !text || !*text) return ImVec2(0, size);
    float w = 0.0f; int n = 0;
    for (const char* p = text; *p; ++p) {
        w += font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, p + 1).x;
        ++n;
    }
    w += spacing * (float)(n > 0 ? n - 1 : 0);
    return ImVec2(w, size);
}
