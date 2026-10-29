#pragma once

#include <cstdint>

#include "imgui.h"

// The CRT effect, from ../4AD_SDL (and text_adv_map before it). The screen's cells are drawn into
// an offscreen texture, which is then shown with bloom, scanlines, barrel curvature, a vignette and
// a faint flicker. The menu bar and its menus are drawn afterwards, unprocessed.
namespace saw {

enum class CrtLevel { Off, Low, Medium, High };
inline constexpr CrtLevel kCrtLevels[] = {CrtLevel::Off, CrtLevel::Low, CrtLevel::Medium, CrtLevel::High};
const char* crtLevelLabel(CrtLevel level);
const char* crtLevelKey(CrtLevel level);  // value stored in the settings file
CrtLevel crtLevelFromKey(const char* key);  // Low if there's no such level

struct CrtParams {
    float glow = 0.0f;       // tight halo around lines and text
    float haze = 0.0f;       // wide, soft bloom
    float scanlines = 0.0f;  // darkening between scanlines, 0..1
    float curvature = 0.0f;  // barrel distortion strength
    float vignette = 0.0f;   // darkening towards the corners, 0..1
    float flicker = 0.0f;    // frame-to-frame brightness variation
    float noise = 0.0f;      // per-pixel grain
};

CrtParams crtParams(CrtLevel level);

// The point of the undistorted picture shown at `screen` once the curvature is applied: the
// shader samples the picture there, so mouse input warped with this lands on what the user sees
ImVec2 crtWarp(ImVec2 screen, ImVec2 rectMin, ImVec2 rectMax, float curvature);

// A rectangle in framebuffer pixels, bottom-up as OpenGL counts them
struct FbRect {
    float x0, y0, x1, y1;
};

class CrtEffect {
public:
    CrtEffect() = default;
    CrtEffect(const CrtEffect&) = delete;
    CrtEffect& operator=(const CrtEffect&) = delete;
    ~CrtEffect();

    // Compiles the shaders on first use; false if the GL features are missing, and the content
    // is then drawn without the effect
    bool ready();

    // Redirects drawing into the offscreen picture, cleared to the background colour. False if
    // the effect isn't available; drawing then goes to the window as usual.
    bool beginCapture(int fbWidth, int fbHeight, uint32_t background);
    // Draws the captured picture into the window through the effect, over `rect` only.
    // scanPeriod: framebuffer pixels from one scanline to the next (the screen has 8 to a text row).
    void endCapture(const FbRect& rect, const CrtParams& params, uint32_t background, float scanPeriod);

private:
    bool init();
    void resize(int width, int height);
    void release();

    bool tried_ = false;
    bool ok_ = false;
    unsigned int scene_ = 0, sceneFbo_ = 0;
    // Half-resolution glow: A holds the tight halo, C the wide haze, B is scratch for the blurs
    unsigned int glowA_ = 0, glowAFbo_ = 0, glowB_ = 0, glowBFbo_ = 0, glowC_ = 0, glowCFbo_ = 0;
    unsigned int vao_ = 0;
    unsigned int brightProg_ = 0, blurProg_ = 0, compositeProg_ = 0;
    int width_ = 0, height_ = 0;
    int previousFbo_ = 0;
};

}  // namespace saw
