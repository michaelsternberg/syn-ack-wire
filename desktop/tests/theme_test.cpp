// Every theme keeps its hives apart from the backgrounds around them, and their letters readable.
#include <cstdio>

#include "theme.hpp"

int main()
{
    int bad = 0;
    for (const saw::Theme& t : saw::themes())
        for (int h = 0; h < 7; ++h) {
            const float d = saw::hiveDistance(t, h);
            if (d < saw::kHiveDistance) {
                std::printf("%s: hive %d is %.0f from a background it sits on (at least %.0f)\n", t.key, h, d,
                            saw::kHiveDistance);
                ++bad;
            }
            const saw::Pair& p = t.role[R_HIVE + h];
            const float c = saw::contrast(p.fg, p.bg);
            if (c < saw::kHiveContrast) {
                std::printf("%s: hive %d's letter has contrast %.1f:1 (at least %.1f:1)\n", t.key, h, c,
                            saw::kHiveContrast);
                ++bad;
            }
        }
    return bad != 0;
}
