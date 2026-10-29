#pragma once

#include <cmath>

#include "imgui.h"

namespace saw {

// The title's static, as on the CoCo: the logo's letters (the corridors inside its outline) filled
// with random pixels, a TV with no signal. A grain is about a CoCo pixel's size on the 80-column screen
// (a quarter of a cell wide, nearly square; an eighth of a row tall: a scanline). The logo's lines are
// drawn two grains thick (kLogoLine), as the CoCo's are two pixels, and the static keeps a grain clear
// of them, so they stand out of it. Half the grain rows
// are drawn afresh each tick, the even then the odd, as the CoCo's snow() does. A grain lit alone takes an NTSC artifact tint by its column's parity (the
// CoCo's PMODE 4 on a composite TV); a run of them shows plain.
class TitleSnow {
public:
    TitleSnow();
    // True if the screen `cells` (SCR_ROWS x SCR_COLS of character, role) shows the title's logo
    bool showing(const void* cells) const;
    // True if row r, column c is one of the logo's cells (its lines are drawn kLogoLine thick)
    bool logo(int r, int c) const { return r < kRows && logo_[r][c] != ' '; }
    static constexpr float kLogoLine = 0.5f;        // the logo's line thickness, of a cell's width: 2 grains
    // Draws the static over the logo, the cells' edges given by edgeX(c) and edgeY(r)
    template <class EX, class EY>
    void draw(ImDrawList* dl, EX edgeX, EY edgeY, ImU32 fg, double now)
    {
        tick(now);
        for (int gy = 0; gy < kGrainRows; ++gy) {
            const int r = gy / kGY, m = gy % kGY;
            for (int gx = 0; gx < kGrainCols; ++gx) {
                if (!lit_[gy][gx])
                    continue;
                const int c = gx / kGX, k = gx % kGX;
                const float x0 = edgeX(c), w = edgeX(c + 1) - x0, y0 = edgeY(r), h = edgeY(r + 1) - y0;
                const bool alone = (gx == 0 || !lit_[gy][gx - 1]) && (gx + 1 == kGrainCols || !lit_[gy][gx + 1]);
                dl->AddRectFilled(ImVec2(std::round(x0 + k * w / kGX), std::round(y0 + m * h / kGY)),
                                  ImVec2(std::round(x0 + (k + 1) * w / kGX), std::round(y0 + (m + 1) * h / kGY)),
                                  alone ? tint(fg, gx & 1) : fg);
            }
        }
    }

private:
    static constexpr int kRows = 15;                // the logo's rows and one below: rows 0-14
    static constexpr int kGX = 4, kGY = 8;          // grains a cell, across and down
    static constexpr int kGrainRows = kRows * kGY, kGrainCols = 80 * kGX;

    void tick(double now);
    static ImU32 tint(ImU32 fg, int odd);

    unsigned char logo_[kRows][80];                 // the doubled logo as the title draws it
    bool in_[kGrainRows][kGrainCols];               // grains inside the letters
    bool lit_[kGrainRows][kGrainCols] = {};
    double next_ = 0;
    int half_ = 0;
    unsigned seed_ = 1;
};

}  // namespace saw
