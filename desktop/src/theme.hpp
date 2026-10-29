#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "desk.h"

// Colour themes. Colours are 0xRRGGBB. Each role of desk.h has a foreground and a background; the
// hives' letters (R_HIVE + syndicate id) have their own, from the MS-DOS client's colours unless a
// theme needs others to stand apart from its own.
namespace saw {

struct Pair {
    uint32_t fg, bg;
};

struct Theme {
    const char* name;
    const char* key;  // value stored in the settings file
    Pair role[NROLES];
    uint32_t menu;    // the menu bar and dialogs
    uint32_t accent;  // menu highlights
};

const std::array<Theme, 7>& themes();
const Theme& themeByKey(const std::string& key);  // DOS Colour if there's no such theme

// The smallest distance between a hive's background and the theme's own backgrounds it sits on or
// next to (text, panel, question rows, title bar), as the redmean colour difference: 0-765.
// The tests hold every theme above kHiveDistance.
float hiveDistance(const Theme& t, int hive);
inline constexpr float kHiveDistance = 90.0f;

// The WCAG contrast ratio of two colours, 1-21. Every hive's letter has at least kHiveContrast
// against its background (the tests again).
float contrast(uint32_t a, uint32_t b);
inline constexpr float kHiveContrast = 4.5f;

void applyImGuiStyle(const Theme& t);

}  // namespace saw
