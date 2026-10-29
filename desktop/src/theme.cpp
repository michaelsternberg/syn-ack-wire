#include "theme.hpp"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace saw {

namespace {

// The MS-DOS client's colours (src/msdos/display_text.c, hive_colour[]) in the VGA palette, in
// syndicate order: black on cyan (white, on DOS, is hard to read there), white on red, yellow on
// brown, black on light grey, white on blue, black on yellow, white on dark grey. Never green or
// magenta, the DOS screen's own colours.
#define DOS_HIVES                                                                                    \
    {0x000000, 0x00AAAA}, {0xFFFFFF, 0xAA0000}, {0xFFFF55, 0xAA5500}, {0x000000, 0xAAAAAA},         \
        {0xFFFFFF, 0x0000AA}, {0x000000, 0xFFFF55}, {0xFFFFFF, 0x555555}

// A phosphor screen has one colour: hives show in reverse, as on a DOS monochrome adapter, at half
// brightness, apart from the full reverse of the title bar and the questions
#define MONO_HIVES(fg, bg) {bg, fg}, {bg, fg}, {bg, fg}, {bg, fg}, {bg, fg}, {bg, fg}, {bg, fg}

// role order: text, panel, inv, bar, ask, last, art, bright, then the seven hives. On the dark
// themes G is white on a dark teal, which reads better there than black on a brighter one.
const std::array<Theme, 7> kThemes = {{
    {"DOS Colour", "dos",
     {{0x55FF55, 0x000000}, {0x000000, 0x00AA00}, {0x000000, 0x55FF55}, {0xFFFFFF, 0x00AA00},
      {0xFFFFFF, 0xAA00AA}, {0x000000, 0xFFFFFF}, {0x00AA00, 0x000000}, {0xFFFFFF, 0x000000}, DOS_HIVES},
     0x0A1A0A, 0x00AA00},
    {"Green Phosphor", "green",
     {{0x33FF66, 0x050A05}, {0x33FF66, 0x0B2A12}, {0x050A05, 0x33FF66}, {0x050A05, 0x33FF66},
      {0x050A05, 0x33FF66}, {0x000000, 0xCCFFDD}, {0x145A28, 0x050A05}, {0xB6FFB6, 0x050A05},
      MONO_HIVES(0x1C9A3C, 0x000000)},
     0x081408, 0x1FA040},
    {"Amber Phosphor", "amber",
     {{0xFFB000, 0x0A0602}, {0xFFB000, 0x2A1A04}, {0x0A0602, 0xFFB000}, {0x0A0602, 0xFFB000},
      {0x0A0602, 0xFFB000}, {0x000000, 0xFFE2A0}, {0x6A4500, 0x0A0602}, {0xFFE2A0, 0x0A0602},
      MONO_HIVES(0xA86C00, 0x000000)},
     0x140C03, 0xB07400},
    {"Neon", "neon",
     {{0x00E5FF, 0x0B0716}, {0x00E5FF, 0x1A1036}, {0x0B0716, 0x00E5FF}, {0xFFFFFF, 0xFF2A6D},
      {0xFFFFFF, 0x9D1DE0}, {0x000000, 0xFFFFFF}, {0x5B2A86, 0x0B0716}, {0xFFFFFF, 0x0B0716},
      {0xFFFFFF, 0x007A7A}, {0xFFFFFF, 0xB00020}, {0xFFE66D, 0x8B4500}, {0x000000, 0xC0C0C0},
      {0xFFFFFF, 0x1F3FBF}, {0x000000, 0xFFE600}, {0xFFFFFF, 0x4A4A5A}},
     0x140C26, 0xFF2A6D},
    {"Synthwave", "synthwave",
     {{0xECE4FF, 0x1E1A38}, {0xECE4FF, 0x2E2856}, {0x1E1A38, 0x7FD4FF}, {0xFFFFFF, 0x8A3FFC},
      {0x1E1A38, 0xFF7FA8}, {0x1E1A38, 0xFFF1A8}, {0x4A3F8A, 0x1E1A38}, {0xFFFFFF, 0x1E1A38},
      {0xFFFFFF, 0x007A7A}, {0xFFFFFF, 0xC0263A}, {0xFFE66D, 0x8B4F10}, {0x000000, 0xC0C0C0},
      {0xFFFFFF, 0x2F6FD6}, {0x000000, 0xFFE600}, {0xFFFFFF, 0x6A6A7A}},
     0x252047, 0x8A3FFC},
    {"Dark", "dark",
     {{0xE6E9EE, 0x1B1E24}, {0xE6E9EE, 0x2B3039}, {0x1B1E24, 0xC8CED8}, {0xFFFFFF, 0x3A4150},
      {0x1B1E24, 0xE0915A}, {0x000000, 0xFFFFFF}, {0x3E4552, 0x1B1E24}, {0xFFFFFF, 0x1B1E24},
      {0xFFFFFF, 0x007A7A}, {0xFFFFFF, 0xC42B2B}, {0xFFE66D, 0x8B5A12}, {0x000000, 0xC0C0C0},
      {0xFFFFFF, 0x2F5FD0}, {0x000000, 0xFFE600}, {0xFFFFFF, 0x6B6B6B}},
     0x22262D, 0x4C6FA8},
    {"Light", "light",
     {{0x1B2330, 0xF6F7F9}, {0x1B2330, 0xDDE2EA}, {0xF6F7F9, 0x1B2330}, {0xFFFFFF, 0x1B2330},
      {0xFFFFFF, 0x6A1B9A}, {0xFFFFFF, 0x000000}, {0xB8C0CC, 0xF6F7F9}, {0xFFFFFF, 0x000000},
      {0x000000, 0x008B8B}, {0xFFFFFF, 0xC0262D}, {0xFFE66D, 0x8B4F10}, {0x000000, 0xA8A8A8},
      {0xFFFFFF, 0x2F6FD6}, {0x000000, 0xFFD600}, {0xFFFFFF, 0x505050}},
     0xECEFF3, 0x2F6FD6},
}};

#undef DOS_HIVES
#undef MONO_HIVES

double linear(uint32_t c, int shift)
{
    const double v = double(c >> shift & 0xFF) / 255;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double luminance(uint32_t c)
{
    return 0.2126 * linear(c, 16) + 0.7152 * linear(c, 8) + 0.0722 * linear(c, 0);
}

float redmean(uint32_t a, uint32_t b)
{
    const float r1 = float(a >> 16 & 0xFF), g1 = float(a >> 8 & 0xFF), b1 = float(a & 0xFF);
    const float r2 = float(b >> 16 & 0xFF), g2 = float(b >> 8 & 0xFF), b2 = float(b & 0xFF);
    const float r = (r1 + r2) / 2, dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    return std::sqrt((2 + r / 256) * dr * dr + 4 * dg * dg + (2 + (255 - r) / 256) * db * db);
}

ImVec4 vec(uint32_t rgb, float alpha = 1.0f)
{
    return ImVec4(float(rgb >> 16 & 0xFF) / 255, float(rgb >> 8 & 0xFF) / 255, float(rgb & 0xFF) / 255, alpha);
}

uint32_t mix(uint32_t a, uint32_t b, float t)
{
    uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
        const float x = float(a >> shift & 0xFF) * (1 - t) + float(b >> shift & 0xFF) * t;
        out |= uint32_t(x + 0.5f) << shift;
    }
    return out;
}

}  // namespace

const std::array<Theme, 7>& themes()
{
    return kThemes;
}

const Theme& themeByKey(const std::string& key)
{
    for (const Theme& t : kThemes)
        if (key == t.key)
            return t;
    return kThemes[0];
}

float contrast(uint32_t a, uint32_t b)
{
    const double la = luminance(a), lb = luminance(b);
    return float((std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05));
}

float hiveDistance(const Theme& t, int hive)
{
    const uint32_t bg = t.role[R_HIVE + hive].bg;
    float d = 765;
    for (int r : {R_TEXT, R_PANEL, R_ASK, R_BAR})
        d = std::min(d, redmean(bg, t.role[r].bg));
    return d;
}

void applyImGuiStyle(const Theme& t)
{
    ImGuiStyle& s = ImGui::GetStyle();
    const uint32_t text = t.role[R_TEXT].fg == t.role[R_TEXT].bg ? 0xFFFFFF : t.role[R_TEXT].fg;
    const bool light = redmean(t.menu, 0xFFFFFF) < redmean(t.menu, 0x000000);
    const uint32_t ink = light ? 0x1B2330 : text;
    s.WindowRounding = s.PopupRounding = s.FrameRounding = 2.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = vec(ink);
    c[ImGuiCol_TextDisabled] = vec(mix(ink, t.menu, 0.5f));
    c[ImGuiCol_WindowBg] = c[ImGuiCol_PopupBg] = c[ImGuiCol_ChildBg] = vec(t.menu);
    c[ImGuiCol_MenuBarBg] = vec(t.menu);
    c[ImGuiCol_Border] = vec(mix(t.accent, t.menu, 0.4f));
    c[ImGuiCol_FrameBg] = vec(mix(t.menu, ink, 0.12f));
    c[ImGuiCol_FrameBgHovered] = vec(mix(t.menu, t.accent, 0.35f));
    c[ImGuiCol_FrameBgActive] = vec(mix(t.menu, t.accent, 0.5f));
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = vec(mix(t.menu, t.accent, 0.3f));
    c[ImGuiCol_Button] = vec(mix(t.menu, t.accent, 0.35f));
    c[ImGuiCol_ButtonHovered] = vec(mix(t.menu, t.accent, 0.6f));
    c[ImGuiCol_ButtonActive] = vec(t.accent);
    c[ImGuiCol_Header] = vec(mix(t.menu, t.accent, 0.4f));
    c[ImGuiCol_HeaderHovered] = vec(mix(t.menu, t.accent, 0.6f));
    c[ImGuiCol_HeaderActive] = vec(t.accent);
    c[ImGuiCol_CheckMark] = vec(light ? t.accent : mix(t.accent, 0xFFFFFF, 0.3f));
    c[ImGuiCol_Separator] = vec(mix(ink, t.menu, 0.7f));
    c[ImGuiCol_ModalWindowDimBg] = vec(0x000000, 0.45f);
    c[ImGuiCol_NavCursor] = vec(t.accent);
}

}  // namespace saw
