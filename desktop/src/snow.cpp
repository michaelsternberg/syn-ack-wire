#include "snow.hpp"

#include <cstring>

#include "desk.h"

// The logo as the title draws it (display_text.c): MS-DOS's title art, each column doubled
extern "C" {
#include "../../client/src/msdos/title_art.h"
#include "../../client/src/msdos/margins.h"
}

namespace saw {

namespace {

// The box pieces' arms: up, down, left, right (the logo's single lines only)
bool arms(unsigned char ch, bool a[4])
{
    static const struct {
        unsigned char ch;
        bool a[4];
    } kPieces[] = {
        {0xB3, {1, 1, 0, 0}}, {0xC4, {0, 0, 1, 1}}, {0xDA, {0, 1, 0, 1}}, {0xBF, {0, 1, 1, 0}},
        {0xC0, {1, 0, 0, 1}}, {0xD9, {1, 0, 1, 0}}, {0xC2, {0, 1, 1, 1}}, {0xC1, {1, 0, 1, 1}},
        {0xC3, {1, 1, 0, 1}}, {0xB4, {1, 1, 1, 0}}, {0xC5, {1, 1, 1, 1}},
    };
    for (const auto& p : kPieces)
        if (p.ch == ch) {
            std::memcpy(a, p.a, sizeof p.a);
            return true;
        }
    return false;
}

}  // namespace

TitleSnow::TitleSnow()
{
    std::memset(logo_, ' ', sizeof logo_);
    for (const unsigned char* p = title_art; *p != 0xFF; p += 3 + p[2]) {
        unsigned char* row = logo_[p[0]] + 2 * p[1] + TITLE80_SHIFT;
        for (int n = 0; n < p[2]; ++n) {
            const unsigned char v = p[3 + n];
            row[2 * n] = v;
            row[2 * n + 1] = std::strchr("\xC4\xC3\xDA\xC0\xC2\xC1\xC5", v) ? 0xC4
                : v == 0xDF || (n + 1 < p[2] && p[4 + n] == 0xDF) ? 0xDF : ' ';
        }
    }

    // Each cell as 2 x 2 quarters, the lines on the borders between them (a cell's lines run through
    // its middle). vwall[y][x]: a line between quarters x and x + 1 (x even), hwall: between y and y + 1
    // (y even); the hyphen's half block fills the top quarters.
    constexpr int QY = 2 * kRows, QX = 160;
    static bool vwall[QY][QX], hwall[QY][QX], solid[QY][QX], out[QY][QX];
    std::memset(vwall, 0, sizeof vwall);
    std::memset(hwall, 0, sizeof hwall);
    std::memset(solid, 0, sizeof solid);
    std::memset(out, 0, sizeof out);
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < 80; ++c) {
            bool a[4];
            if (logo_[r][c] == 0xDF)
                solid[2 * r][2 * c] = solid[2 * r][2 * c + 1] = true;
            else if (arms(logo_[r][c], a)) {
                vwall[2 * r][2 * c] |= a[0];
                vwall[2 * r + 1][2 * c] |= a[1];
                hwall[2 * r][2 * c] |= a[2];
                hwall[2 * r][2 * c + 1] |= a[3];
            }
        }
    // The outside: everything a flood from the edges reaches; the rest is the letters
    static int todo[QY * QX * 4][2];
    int n = 0;
    for (int y = 0; y < QY; ++y)
        for (int x = 0; x < QX; ++x)
            if (y == 0 || y == QY - 1 || x == 0 || x == QX - 1) {
                todo[n][0] = y;
                todo[n++][1] = x;
            }
    while (n) {
        const int y = todo[--n][0], x = todo[n][1];
        if (out[y][x] || solid[y][x])
            continue;
        out[y][x] = true;
        auto go = [&](int ny, int nx) {
            if (ny >= 0 && ny < QY && nx >= 0 && nx < QX && !out[ny][nx]) {
                todo[n][0] = ny;
                todo[n++][1] = nx;
            }
        };
        if (x + 1 < QX && !(x % 2 == 0 && vwall[y][x])) go(y, x + 1);
        if (x > 0 && !(x % 2 == 1 && vwall[y][x - 1])) go(y, x - 1);
        if (y + 1 < QY && !(y % 2 == 0 && hwall[y][x])) go(y + 1, x);
        if (y > 0 && !(y % 2 == 1 && hwall[y - 1][x])) go(y - 1, x);
    }

    // A grain is inside if its quarter is, and a grain clear of the lines: a line down a cell's middle
    // is its two middle grains, so the cell's four are left out there; a line across is the middle two
    // grain rows, so four rows are left out under it
    for (int gy = 0; gy < kGrainRows; ++gy)
        for (int gx = 0; gx < kGrainCols; ++gx) {
            const int r = gy / kGY, m = gy % kGY, c = gx / kGX, k = gx % kGX;
            const int y = 2 * r + (m >= kGY / 2), x = 2 * c + (k >= kGX / 2);
            bool in = !out[y][x] && !solid[y][x];
            if (vwall[y][2 * c])
                in = false;
            if (m >= kGY / 2 - 2 && m <= kGY / 2 + 1 && hwall[2 * r][x])
                in = false;
            in_[gy][gx] = in;
        }
}

bool TitleSnow::showing(const void* cells) const
{
    const cell(*s)[SCR_COLS] = static_cast<const cell(*)[SCR_COLS]>(cells);
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < 80; ++c)
            if (logo_[r][c] != ' ' && s[r][c].ch != logo_[r][c])
                return false;
    return true;
}

void TitleSnow::tick(double now)
{
    if (now < next_)
        return;
    // About the CoCo's pace: each call there does half the lines in under 2 frames
    next_ = now + 1.0 / 30;
    half_ ^= 1;
    for (int gy = half_; gy < kGrainRows; gy += 2)
        for (int gx = 0; gx < kGrainCols; ++gx) {
            seed_ = seed_ * 1103515245u + 12345u;
            lit_[gy][gx] = in_[gy][gx] && (seed_ >> 16) % 10 < 3;   // 30% of them, as the CoCo's table
        }
}

// NTSC artifact colour: a lone pixel on an even column of the CoCo's 256 shows one hue, on an odd
// column the other; mixed into the theme's colour so the static stays the theme's
ImU32 TitleSnow::tint(ImU32 fg, int odd)
{
    const ImVec4 f = ImGui::ColorConvertU32ToFloat4(fg);
    const ImVec4 t = odd ? ImVec4(1.0f, 0.55f, 0.15f, 1) : ImVec4(0.25f, 0.55f, 1.0f, 1);
    const float k = 0.3f;
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(f.x + (t.x - f.x) * k, f.y + (t.y - f.y) * k, f.z + (t.z - f.z) * k, f.w));
}

}  // namespace saw
