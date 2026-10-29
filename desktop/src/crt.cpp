#include "crt.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

namespace saw {

namespace {

// OpenGL 2.0+ entry points, loaded through SDL; the 1.1 ones come from the GL library itself
PFNGLACTIVETEXTUREPROC pActiveTexture;
PFNGLGENFRAMEBUFFERSPROC pGenFramebuffers;
PFNGLDELETEFRAMEBUFFERSPROC pDeleteFramebuffers;
PFNGLBINDFRAMEBUFFERPROC pBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC pFramebufferTexture2D;
PFNGLCHECKFRAMEBUFFERSTATUSPROC pCheckFramebufferStatus;
PFNGLGENVERTEXARRAYSPROC pGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC pDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC pBindVertexArray;
PFNGLCREATESHADERPROC pCreateShader;
PFNGLSHADERSOURCEPROC pShaderSource;
PFNGLCOMPILESHADERPROC pCompileShader;
PFNGLGETSHADERIVPROC pGetShaderiv;
PFNGLGETSHADERINFOLOGPROC pGetShaderInfoLog;
PFNGLDELETESHADERPROC pDeleteShader;
PFNGLCREATEPROGRAMPROC pCreateProgram;
PFNGLATTACHSHADERPROC pAttachShader;
PFNGLLINKPROGRAMPROC pLinkProgram;
PFNGLGETPROGRAMIVPROC pGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC pGetProgramInfoLog;
PFNGLDELETEPROGRAMPROC pDeleteProgram;
PFNGLUSEPROGRAMPROC pUseProgram;
PFNGLGETUNIFORMLOCATIONPROC pGetUniformLocation;
PFNGLUNIFORM1IPROC pUniform1i;
PFNGLUNIFORM1FPROC pUniform1f;
PFNGLUNIFORM2FPROC pUniform2f;
PFNGLUNIFORM3FPROC pUniform3f;
PFNGLUNIFORM4FPROC pUniform4f;

template <typename T>
bool load(T& fn, const char* name)
{
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return fn != nullptr;
}

bool loadGl()
{
    return load(pActiveTexture, "glActiveTexture") && load(pGenFramebuffers, "glGenFramebuffers") &&
           load(pDeleteFramebuffers, "glDeleteFramebuffers") && load(pBindFramebuffer, "glBindFramebuffer") &&
           load(pFramebufferTexture2D, "glFramebufferTexture2D") &&
           load(pCheckFramebufferStatus, "glCheckFramebufferStatus") && load(pGenVertexArrays, "glGenVertexArrays") &&
           load(pDeleteVertexArrays, "glDeleteVertexArrays") && load(pBindVertexArray, "glBindVertexArray") &&
           load(pCreateShader, "glCreateShader") && load(pShaderSource, "glShaderSource") &&
           load(pCompileShader, "glCompileShader") && load(pGetShaderiv, "glGetShaderiv") &&
           load(pGetShaderInfoLog, "glGetShaderInfoLog") && load(pDeleteShader, "glDeleteShader") &&
           load(pCreateProgram, "glCreateProgram") && load(pAttachShader, "glAttachShader") &&
           load(pLinkProgram, "glLinkProgram") && load(pGetProgramiv, "glGetProgramiv") &&
           load(pGetProgramInfoLog, "glGetProgramInfoLog") && load(pDeleteProgram, "glDeleteProgram") &&
           load(pUseProgram, "glUseProgram") && load(pGetUniformLocation, "glGetUniformLocation") &&
           load(pUniform1i, "glUniform1i") && load(pUniform1f, "glUniform1f") && load(pUniform2f, "glUniform2f") &&
           load(pUniform3f, "glUniform3f") && load(pUniform4f, "glUniform4f");
}

// A triangle covering the whole viewport, with no vertex buffer
constexpr const char* kVertex = R"(#version 330 core
out vec2 vUv;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Keeps the bright parts of the picture, for the glow. Only the brightest: a text screen has whole
// cells filled with colour, and their glow would wash out the dark letters on them
constexpr const char* kBright = R"(#version 330 core
in vec2 vUv;
out vec4 outColour;
uniform sampler2D uTex;
void main()
{
    vec3 c = texture(uTex, vUv).rgb;
    float l = max(c.r, max(c.g, c.b));
    outColour = vec4(c * smoothstep(0.45, 1.0, l), 1.0);
}
)";

// One direction of a Gaussian blur
constexpr const char* kBlur = R"(#version 330 core
in vec2 vUv;
out vec4 outColour;
uniform sampler2D uTex;
uniform vec2 uStep;  // one texel along the blur direction, times the spread
void main()
{
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(uTex, vUv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(uTex, vUv + uStep * float(i)).rgb * w[i];
        c += texture(uTex, vUv - uStep * float(i)).rgb * w[i];
    }
    outColour = vec4(c, 1.0);
}
)";

// The picture as the tube shows it. crtWarp() mirrors the curvature on the CPU.
constexpr const char* kComposite = R"(#version 330 core
out vec4 outColour;
uniform sampler2D uScene;
uniform sampler2D uGlow;
uniform sampler2D uHaze;
uniform vec4 uRect;      // canvas in framebuffer pixels: x0, y0, x1, y1 (bottom-up)
uniform vec2 uFbSize;
uniform vec3 uBackground;
uniform float uGlowAmount, uHazeAmount, uScan, uCurve, uVignette, uFlicker, uNoise, uTime, uScanPeriod;

float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main()
{
    vec2 size = uRect.zw - uRect.xy;
    vec2 c = (gl_FragCoord.xy - uRect.xy) / size * 2.0 - 1.0;
    c *= 1.0 + uCurve * dot(c, c);
    vec2 uv = c * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        outColour = vec4(uBackground * 0.35, 1.0);
        return;
    }
    vec2 pos = uRect.xy + uv * size;
    vec2 tc = pos / uFbSize;
    vec3 col = texture(uScene, tc).rgb + texture(uGlow, tc).rgb * uGlowAmount + texture(uHaze, tc).rgb * uHazeAmount;

    float scan = 0.5 + 0.5 * cos(pos.y * 6.2831853 / uScanPeriod);
    col *= 1.0 - uScan * (1.0 - scan);
    col *= 1.0 - uVignette * pow(dot(c, c) * 0.5, 1.5);
    col *= 1.0 + uFlicker * (hash(vec2(floor(uTime * 24.0), 0.0)) - 0.5);
    col += uNoise * (hash(gl_FragCoord.xy + fract(uTime) * 100.0) - 0.5);
    outColour = vec4(col, 1.0);
}
)";

unsigned int compile(unsigned int type, const char* source)
{
    const unsigned int sh = pCreateShader(type);
    pShaderSource(sh, 1, &source, nullptr);
    pCompileShader(sh);
    int ok = 0;
    pGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        pGetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "CRT shader failed to compile: %s\n", log);
        pDeleteShader(sh);
        return 0;
    }
    return sh;
}

unsigned int program(const char* fragment)
{
    const unsigned int vs = compile(GL_VERTEX_SHADER, kVertex);
    const unsigned int fs = compile(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs)
        return 0;
    const unsigned int prog = pCreateProgram();
    pAttachShader(prog, vs);
    pAttachShader(prog, fs);
    pLinkProgram(prog);
    pDeleteShader(vs);
    pDeleteShader(fs);
    int ok = 0;
    pGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = "";
        pGetProgramInfoLog(prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "CRT shader failed to link: %s\n", log);
        pDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// A colour texture with a framebuffer drawing into it
bool target(unsigned int& tex, unsigned int& fbo, int w, int h)
{
    if (!tex)
        glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (!fbo)
        pGenFramebuffers(1, &fbo);
    pBindFramebuffer(GL_FRAMEBUFFER, fbo);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    return pCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void fullscreenPass(unsigned int prog, unsigned int input)
{
    pUseProgram(prog);
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input);
    pUniform1i(pGetUniformLocation(prog, "uTex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace

const char* crtLevelLabel(CrtLevel level)
{
    switch (level) {
    case CrtLevel::Off: return "Off";
    case CrtLevel::Low: return "Low";
    case CrtLevel::High: return "High";
    case CrtLevel::Medium: break;
    }
    return "Medium";
}

const char* crtLevelKey(CrtLevel level)
{
    switch (level) {
    case CrtLevel::Off: return "off";
    case CrtLevel::Low: return "low";
    case CrtLevel::High: return "high";
    case CrtLevel::Medium: break;
    }
    return "medium";
}

CrtLevel crtLevelFromKey(const char* key)
{
    for (CrtLevel level : kCrtLevels)
        if (std::strcmp(key, crtLevelKey(level)) == 0)
            return level;
    return CrtLevel::Low;
}

CrtParams crtParams(CrtLevel level)
{
    switch (level) {
    // glow, haze, scanlines, curvature, vignette, flicker, noise
    case CrtLevel::Off: return {};
    case CrtLevel::Low: return {0.3f, 0.15f, 0.12f, 0.012f, 0.2f, 0.0f, 0.0f};
    case CrtLevel::High: return {0.8f, 0.45f, 0.4f, 0.045f, 0.55f, 0.03f, 0.04f};
    case CrtLevel::Medium: break;
    }
    return {0.5f, 0.3f, 0.25f, 0.025f, 0.35f, 0.012f, 0.02f};
}

ImVec2 crtWarp(ImVec2 screen, ImVec2 min, ImVec2 max, float curvature)
{
    if (curvature <= 0.0f || max.x <= min.x || max.y <= min.y)
        return screen;
    const float w = max.x - min.x, h = max.y - min.y;
    float cx = (screen.x - min.x) / w * 2.0f - 1.0f;
    float cy = (screen.y - min.y) / h * 2.0f - 1.0f;
    const float k = 1.0f + curvature * (cx * cx + cy * cy);
    cx *= k;
    cy *= k;
    return ImVec2(min.x + (cx * 0.5f + 0.5f) * w, min.y + (cy * 0.5f + 0.5f) * h);
}

CrtEffect::~CrtEffect()
{
    release();
}

bool CrtEffect::ready()
{
    if (!tried_) {
        tried_ = true;
        ok_ = init();
        if (!ok_)
            std::fprintf(stderr, "The CRT effect isn't available on this system\n");
    }
    return ok_;
}

bool CrtEffect::init()
{
    if (!loadGl())
        return false;
    brightProg_ = program(kBright);
    blurProg_ = program(kBlur);
    compositeProg_ = program(kComposite);
    if (!brightProg_ || !blurProg_ || !compositeProg_)
        return false;
    pGenVertexArrays(1, &vao_);
    return true;
}

void CrtEffect::release()
{
    if (!ok_)
        return;
    for (unsigned int* fbo : {&sceneFbo_, &glowAFbo_, &glowBFbo_, &glowCFbo_})
        if (*fbo)
            pDeleteFramebuffers(1, fbo);
    for (unsigned int* tex : {&scene_, &glowA_, &glowB_, &glowC_})
        if (*tex)
            glDeleteTextures(1, tex);
    for (unsigned int prog : {brightProg_, blurProg_, compositeProg_})
        if (prog)
            pDeleteProgram(prog);
    if (vao_)
        pDeleteVertexArrays(1, &vao_);
    ok_ = false;
}

void CrtEffect::resize(int width, int height)
{
    if (width == width_ && height == height_)
        return;
    width_ = width;
    height_ = height;
    // The glow works at half resolution: softer and cheaper
    const bool ok = target(scene_, sceneFbo_, width, height) &&
                    target(glowA_, glowAFbo_, std::max(1, width / 2), std::max(1, height / 2)) &&
                    target(glowB_, glowBFbo_, std::max(1, width / 2), std::max(1, height / 2)) &&
                    target(glowC_, glowCFbo_, std::max(1, width / 2), std::max(1, height / 2));
    if (!ok) {
        std::fprintf(stderr, "The CRT effect's framebuffers are incomplete; turning it off\n");
        release();
        tried_ = true;
    }
}

bool CrtEffect::beginCapture(int width, int height, uint32_t background)
{
    if (!ready())
        return false;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo_);
    resize(width, height);
    if (!ok_)
        return false;
    // From here the content is drawn into the scene texture
    pBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(((background >> 16) & 0xFF) / 255.0f, ((background >> 8) & 0xFF) / 255.0f,
                 (background & 0xFF) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

void CrtEffect::endCapture(const FbRect& r, const CrtParams& params, uint32_t background, float scanPeriod)
{
    if (!ok_)
        return;
    const int w = width_, h = height_;
    const int hw = std::max(1, w / 2), hh = std::max(1, h / 2);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    pBindVertexArray(vao_);

    // Glow: the bright parts, blurred across and then down. A tight blur gives thin lines a
    // visible halo; blurring that again, wider, gives the soft haze.
    auto blur = [&](unsigned int from, unsigned int toFbo, float dx, float dy) {
        pBindFramebuffer(GL_FRAMEBUFFER, toFbo);
        pUseProgram(blurProg_);
        pUniform2f(pGetUniformLocation(blurProg_, "uStep"), dx / float(hw), dy / float(hh));
        fullscreenPass(blurProg_, from);
    };
    pBindFramebuffer(GL_FRAMEBUFFER, glowAFbo_);
    glViewport(0, 0, hw, hh);
    fullscreenPass(brightProg_, scene_);
    blur(glowA_, glowBFbo_, 0.8f, 0.0f);
    blur(glowB_, glowAFbo_, 0.0f, 0.8f);
    blur(glowA_, glowBFbo_, 3.0f, 0.0f);
    blur(glowB_, glowCFbo_, 0.0f, 3.0f);

    // The picture, back into the window over the rectangle only
    pBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(previousFbo_));
    glViewport(0, 0, w, h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<int>(r.x0), static_cast<int>(r.y0), static_cast<int>(r.x1 - r.x0),
              static_cast<int>(r.y1 - r.y0));
    const unsigned int prog = compositeProg_;
    pUseProgram(prog);
    pActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glowC_);
    pActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glowA_);
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_);
    pUniform1i(pGetUniformLocation(prog, "uScene"), 0);
    pUniform1i(pGetUniformLocation(prog, "uGlow"), 1);
    pUniform1i(pGetUniformLocation(prog, "uHaze"), 2);
    pUniform4f(pGetUniformLocation(prog, "uRect"), r.x0, r.y0, r.x1, r.y1);
    pUniform2f(pGetUniformLocation(prog, "uFbSize"), float(w), float(h));
    const uint32_t bg = background;
    pUniform3f(pGetUniformLocation(prog, "uBackground"), ((bg >> 16) & 0xFF) / 255.0f, ((bg >> 8) & 0xFF) / 255.0f,
               (bg & 0xFF) / 255.0f);
    const CrtParams& pr = params;
    pUniform1f(pGetUniformLocation(prog, "uGlowAmount"), pr.glow);
    pUniform1f(pGetUniformLocation(prog, "uHazeAmount"), pr.haze);
    pUniform1f(pGetUniformLocation(prog, "uScan"), pr.scanlines);
    pUniform1f(pGetUniformLocation(prog, "uCurve"), pr.curvature);
    pUniform1f(pGetUniformLocation(prog, "uVignette"), pr.vignette);
    pUniform1f(pGetUniformLocation(prog, "uFlicker"), pr.flicker);
    pUniform1f(pGetUniformLocation(prog, "uNoise"), pr.noise);
    pUniform1f(pGetUniformLocation(prog, "uTime"), static_cast<float>(SDL_GetTicks() % 100000) / 1000.0f);
    pUniform1f(pGetUniformLocation(prog, "uScanPeriod"), std::max(2.0f, scanPeriod));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    pBindVertexArray(0);
    pUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
}

}  // namespace saw
