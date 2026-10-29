#include "glyphs.hpp"

#include <algorithm>
#include <cmath>

namespace saw {

namespace {

// Code page 437's box drawing, B3-DA: the arms up, down, left and right of the middle, each 0
// (none), 1 (single) or 2 (double)
struct Arms {
    unsigned char up, down, left, right;
};
constexpr Arms kBox[0xDA - 0xB3 + 1] = {
    {1, 1, 0, 0}, {1, 1, 1, 0}, {1, 1, 2, 0}, {2, 2, 1, 0}, {0, 2, 1, 0}, {0, 1, 2, 0},  // │ ┤ ╡ ╢ ╖ ╕
    {2, 2, 2, 0}, {2, 2, 0, 0}, {0, 2, 2, 0}, {2, 0, 2, 0}, {2, 0, 1, 0}, {1, 0, 2, 0},  // ╣ ║ ╗ ╝ ╜ ╛
    {0, 1, 1, 0}, {1, 0, 0, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 0, 1}, {0, 0, 1, 1},  // ┐ └ ┴ ┬ ├ ─
    {1, 1, 1, 1}, {1, 1, 0, 2}, {2, 2, 0, 1}, {2, 0, 0, 2}, {0, 2, 0, 2}, {2, 0, 2, 2},  // ┼ ╞ ╟ ╚ ╔ ╩
    {0, 2, 2, 2}, {2, 2, 0, 2}, {0, 0, 2, 2}, {2, 2, 2, 2}, {1, 0, 2, 2}, {2, 0, 1, 1},  // ╦ ╠ ═ ╬ ╧ ╨
    {0, 1, 2, 2}, {0, 2, 1, 1}, {2, 0, 0, 1}, {1, 0, 0, 2}, {0, 1, 0, 2}, {0, 2, 0, 1},  // ╤ ╥ ╙ ╘ ╒ ╓
    {2, 2, 1, 1}, {1, 1, 2, 2}, {1, 0, 1, 0}, {0, 1, 0, 1},                              // ╫ ╪ ┘ ┌
};

constexpr unsigned char kPad = 0x09;    // ○
constexpr unsigned char kYen = 0x9D;    // ¥
constexpr unsigned char kTriple = 0xF0;  // ≡
constexpr unsigned char kChip = 0xFE;   // ■
constexpr unsigned char kFull = 0xDB, kLower = 0xDC, kUpper = 0xDF;

void rect(ImDrawList* dl, float x0, float y0, float x1, float y1, ImU32 col)
{
    dl->AddRectFilled(ImVec2(std::round(x0), std::round(y0)), ImVec2(std::round(x1), std::round(y1)), col);
}

// One cell's lines: each arm from the edge to the middle, a double arm as two lines either side of
// it. Arms run on past the middle (a single by half a line, a double to the far side of the other
// direction's lines), so corners close and crossings meet.
void box(ImDrawList* dl, ImVec2 p, float w, float h, Arms a, ImU32 col, float line)
{
    const float t = std::max(1.0f, std::round(w * (line > 0 ? line : 0.14f)));
    const float gap = std::max(t + 1.0f, std::round(w * 0.2f));
    const float cx = std::round(p.x + w / 2 - t / 2), cy = std::round(p.y + h / 2 - t / 2);
    const float reach = gap + t;
    for (int side : {-1, 1}) {
        const unsigned char v = side < 0 ? a.up : a.down, hz = side < 0 ? a.left : a.right;
        const float yEdge = side < 0 ? p.y : p.y + h, xEdge = side < 0 ? p.x : p.x + w;
        const float yEnd = side < 0 ? cy + t + (v == 2 ? reach : 0) : cy - (v == 2 ? reach : 0);
        const float xEnd = side < 0 ? cx + t + (hz == 2 ? reach : 0) : cx - (hz == 2 ? reach : 0);
        const float twin[2] = {-gap, gap}, one[1] = {0.0f};
        if (v)
            for (int i = 0; i < (v == 2 ? 2 : 1); ++i) {
                const float x = cx + (v == 2 ? twin : one)[i];
                rect(dl, x, std::min(yEdge, yEnd), x + t, std::max(yEdge, yEnd), col);
            }
        if (hz)
            for (int i = 0; i < (hz == 2 ? 2 : 1); ++i) {
                const float y = cy + (hz == 2 ? twin : one)[i];
                rect(dl, std::min(xEdge, xEnd), y, std::max(xEdge, xEnd), y + t, col);
            }
    }
}

}  // namespace

void drawGlyph(ImDrawList* dl, ImVec2 p, float w, float h, unsigned char ch, ImU32 fg, const CellFont& cf, bool bold,
               float line)
{
    if (ch == ' ')
        return;
    if (ch >= 0xB3 && ch <= 0xDA) {
        box(dl, p, w, h, kBox[ch - 0xB3], fg, line);
        return;
    }
    const float t = std::max(1.0f, std::round(w * 0.14f));
    switch (ch) {
    case kFull: rect(dl, p.x, p.y, p.x + w, p.y + h, fg); return;
    case kUpper: rect(dl, p.x, p.y, p.x + w, p.y + h / 2, fg); return;
    case kLower: rect(dl, p.x, p.y + h / 2, p.x + w, p.y + h, fg); return;
    case kChip:  // side by side they make a chip's body, its legs (┬ ┴) above and below
        rect(dl, p.x, p.y + h * 0.28f, p.x + w, p.y + h * 0.72f, fg);
        return;
    case kTriple:  // a connector's contacts
        for (float y : {0.34f, 0.5f, 0.66f})
            rect(dl, p.x + w * 0.1f, p.y + h * y - t / 2, p.x + w * 0.9f, p.y + h * y + t / 2, fg);
        return;
    case kPad:  // a solder pad at a trace's end
        dl->AddCircle(ImVec2(p.x + w / 2, p.y + h / 2), w * 0.34f, fg, 0, t);
        return;
    case kYen: {  // a Y with two bars across its stem
        dl->AddText(cf.font, cf.size, ImVec2(p.x + (w - cf.font->GetFontBaked(cf.size)->GetCharAdvance('Y')) / 2, p.y + cf.baseline),
                    fg, "Y");
        const float mid = p.y + h / 2;
        for (float dy : {0.06f, 0.17f})
            rect(dl, p.x + w * 0.2f, mid + h * dy - t / 2, p.x + w * 0.8f, mid + h * dy + t / 2, fg);
        return;
    }
    }
    if (ch < 0x20 || ch > 0x7E)
        return;
    const char s[2] = {static_cast<char>(ch), 0};
    ImFont* font = bold ? cf.bold : cf.font;
    const float adv = font->GetFontBaked(cf.size)->GetCharAdvance(ch);
    dl->AddText(font, cf.size, ImVec2(std::round(p.x + (w - adv) / 2), p.y + cf.baseline), fg, s);
}

}  // namespace saw
