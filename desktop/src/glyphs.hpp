#pragma once

#include "fonts.hpp"
#include "imgui.h"

namespace saw {

// Draws one cell's character, code page 437 as the MS-DOS client has it, in `fg`, over the cell at
// `pos` of size `w` x `h` (its background is drawn already). Printable ASCII comes from the font,
// or its bold copy if `bold` (a hive's letter); box drawing, blocks, the chips' ■ and ≡, the pads'
// ○ and the yen sign are drawn as shapes, so they join up across cells whatever the font.
void drawGlyph(ImDrawList* dl, ImVec2 pos, float w, float h, unsigned char ch, ImU32 fg, const CellFont& cf,
               bool bold = false);

}  // namespace saw
