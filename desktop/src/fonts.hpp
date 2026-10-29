#pragma once

#include <array>
#include <string>

#include "imgui.h"

// The font themes: one plain font and five sci-fi and cyberpunk ones (fonts/README.md). The screen
// is a grid of cells, and each glyph is centred in its cell, so a proportional face keeps the grid.
// Lines, boxes, blocks and the currency sign are drawn by the program (glyphs.cpp), not the font.
namespace saw {

struct FontTheme {
    const char* name;
    const char* key;   // value stored in the settings file
    const char* file;
    bool pixel;        // a pixel font: kept on whole pixels
    float fill;        // how much of the cell's width its widest capital may take
    ImFont* font = nullptr;
    ImFont* bold = nullptr;  // the same, emboldened by FreeType: the hives' letters
};

std::array<FontTheme, 6>& fontThemes();
const FontTheme& fontThemeByKey(const std::string& key);  // Normal if there's no such theme

// The folder with the fonts and the instructions (SYNACK.HLP), ending in a separator: beside the
// program, in a macOS bundle's Resources, or the source tree's when run from a build folder
std::string findDataDir();

// Loads every theme's font, and the menus' font (Normal); false if the fonts aren't there
bool loadFonts(const std::string& dir);
ImFont* menuFont();

// How a theme's font sits in cells of this size
struct CellFont {
    ImFont* font;
    ImFont* bold;
    float size;      // the size to draw at
    float baseline;  // from the cell's top to the top of the font's line, so capitals sit centred
};
CellFont fitCell(const FontTheme& t, float cellW, float cellH);

}  // namespace saw
