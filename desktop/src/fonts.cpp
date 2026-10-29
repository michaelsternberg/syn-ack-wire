#include "fonts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <SDL3/SDL.h>

#include "imgui_freetype.h"

namespace saw {

namespace {

namespace fs = std::filesystem;

constexpr const char* kProbe = "JetBrainsMono.ttf";
constexpr float kRefPx = 100.0f;

ImFont* g_menu = nullptr;

}  // namespace

std::array<FontTheme, 6>& fontThemes()
{
    static std::array<FontTheme, 6> themes{{
        {"Normal", "normal", "JetBrainsMono.ttf", false, 0.92f},
        {"Share Tech Mono", "sharetech", "ShareTechMono-Regular.ttf", false, 0.95f},
        {"Orbitron", "orbitron", "Orbitron.ttf", false, 0.92f},
        {"Oxanium", "oxanium", "Oxanium.ttf", false, 0.92f},
        {"VT323 Terminal", "vt323", "VT323-Regular.ttf", true, 1.0f},
        {"Chakra Petch", "chakrapetch", "ChakraPetch-Regular.ttf", false, 0.9f},
    }};
    return themes;
}

const FontTheme& fontThemeByKey(const std::string& key)
{
    for (const FontTheme& t : fontThemes())
        if (key == t.key)
            return t;
    return fontThemes()[0];
}

std::string findDataDir()
{
    std::error_code ec;
    auto good = [&](const fs::path& dir) { return fs::exists(dir / kProbe, ec); };
    if (const char* base = SDL_GetBasePath()) {
        for (const fs::path& dir : {fs::path(base) / "data", fs::path(base)})
            if (good(dir))
                return (dir / "").string();
    }
#ifdef SAW_SOURCE_DATA_DIR
    if (good(SAW_SOURCE_DATA_DIR))
        return (fs::path(SAW_SOURCE_DATA_DIR) / "").string();
#endif
    return {};
}

bool loadFonts(const std::string& dir)
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    bool ok = !dir.empty();
    for (FontTheme& t : fontThemes()) {
        if (!ok)
            break;
        const std::string path = (fs::path(dir) / t.file).string();
        ImFontConfig cfg;
        std::snprintf(cfg.Name, sizeof cfg.Name, "%s", t.file);
        cfg.PixelSnapH = t.pixel;
        cfg.Flags |= ImFontFlags_NoLoadError;
        t.font = atlas->AddFontFromFileTTF(path.c_str(), kRefPx, &cfg);
        std::snprintf(cfg.Name, sizeof cfg.Name, "%s bold", t.file);
        cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Bold;
        t.bold = t.font ? atlas->AddFontFromFileTTF(path.c_str(), kRefPx, &cfg) : nullptr;
        if (!t.font)
            std::fprintf(stderr, "Could not load the font %s%s\n", dir.c_str(), t.file);
    }
    g_menu = fontThemes()[0].font;
    if (!g_menu) {
        std::fprintf(stderr, "Fonts not found; using the built-in font\n");
        g_menu = atlas->AddFontDefault();
        for (FontTheme& t : fontThemes())
            t.font = t.bold = g_menu;
        return false;
    }
    for (FontTheme& t : fontThemes()) {
        if (!t.font)
            t.font = g_menu;
        if (!t.bold)
            t.bold = t.font;
    }
    return true;
}

ImFont* menuFont()
{
    return g_menu;
}

// The size where an average capital or digit fills `fill` of the cell's width, or the capitals 64%
// of its height, whichever is smaller; then the line placed so the capitals' middle is the cell's
// middle. An average, not the widest: a display face's M and W would shrink everything else.
CellFont fitCell(const FontTheme& t, float cellW, float cellH)
{
    ImFontBaked* ref = t.font->GetFontBaked(kRefPx);
    float wide = 0.0f;
    for (const char* c = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; *c; ++c)
        wide += ref->GetCharAdvance(static_cast<ImWchar>(*c)) / 36;
    wide = std::max(wide, 1.0f);
    const ImFontGlyph* h = ref->FindGlyph('H');
    const float capTop = h ? h->Y0 : kRefPx * 0.2f, capBottom = h ? h->Y1 : kRefPx * 0.9f;
    const float capH = std::max(1.0f, capBottom - capTop);
    float size = std::min(kRefPx * cellW * t.fill / wide, kRefPx * cellH * 0.64f / capH);
    if (t.pixel)
        size = std::max(1.0f, std::floor(size));
    const float scale = size / kRefPx;
    const float baseline = cellH / 2 - (capTop + capH / 2) * scale;
    return {t.font, t.bold, size, std::round(baseline)};
}

}  // namespace saw
