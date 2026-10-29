// SYN-ACK WIRE for the desktop: window, OpenGL context and main loop. The game itself is the
// 8-bit clients' common code, on its own thread (desk.c).
//
// Usage: syn-ack-wire [options]
//   --server <host[:port]>     dial this server (as Region > Custom Server)
//   --settings <file>          read and write this settings file instead of the usual one
//   --option <key=value>       theme, font or crt, as in the settings file (repeatable)
//   --size <W>x<H>             the window's size
//   --screenshot <file.bmp>    after --wait seconds, save the window and quit; settings left alone
//   --wait <seconds>           with --screenshot (default 3)
//   --every <seconds>          with --screenshot: also save one this often, as file-NNN.bmp
//   --pick <seconds>:<server>  then choose this server, as from the Region menu: a region's name or
//                              host:port; a ! before the server says yes to logging off (for tests)
//   --seats <host:port>        read the regions' seat counts (GET /SEATS) there (for tests)
//   --keys <text>              type this into the game, a key every 0.15 s: | waits a second, / is
//                              Enter, ^ is Esc, ! is F1 (for tests)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include "../../client/src/common/theme_tab.h"
#include "app.hpp"
#include "desk.h"
#include "fonts.hpp"

namespace {

bool saveScreenshot(const char* path, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    if (!surface)
        return false;
    for (int y = 0; y < height; ++y)  // OpenGL rows run bottom-up
        std::memcpy(static_cast<unsigned char*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch,
                    pixels.data() + static_cast<size_t>(height - 1 - y) * width * 4, static_cast<size_t>(width) * 4);
    const bool ok = SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    return ok;
}

// As in ../4AD_SDL: with the CRT effect on, the screen's draw list goes through the effect; the
// menu bar and dialogs are drawn on top as usual
void render(saw::App& app, int width, int height)
{
    ImDrawData* dd = ImGui::GetDrawData();
    const uint32_t bg = app.backgroundColour();
    glViewport(0, 0, width, height);
    glClearColor(((bg >> 16) & 0xFF) / 255.0f, ((bg >> 8) & 0xFF) / 255.0f, (bg & 0xFF) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!app.crtOn() || !app.crt().beginCapture(width, height, bg)) {
        ImGui_ImplOpenGL3_RenderDrawData(dd);
        return;
    }
    ImDrawData content = *dd, overlay = *dd;
    for (ImDrawData* part : {&content, &overlay}) {
        part->CmdLists.resize(0);
        part->TotalVtxCount = part->TotalIdxCount = 0;
    }
    for (ImDrawList* list : dd->CmdLists) {
        ImDrawData& part = app.isContentDrawList(list) ? content : overlay;
        part.CmdLists.push_back(list);
        part.TotalVtxCount += list->VtxBuffer.Size;
        part.TotalIdxCount += list->IdxBuffer.Size;
    }
    content.CmdListsCount = content.CmdLists.Size;
    overlay.CmdListsCount = overlay.CmdLists.Size;

    ImGui_ImplOpenGL3_RenderDrawData(&content);
    const ImVec2 s = dd->FramebufferScale, o = dd->DisplayPos;
    const ImVec2 a = app.crtMin(), b = app.crtMax();
    const saw::FbRect r{(a.x - o.x) * s.x, float(height) - (b.y - o.y) * s.y, (b.x - o.x) * s.x,
                        float(height) - (a.y - o.y) * s.y};
    app.crt().endCapture(r, app.crtParameters(), bg, app.scanlineSpacing());
    ImGui_ImplOpenGL3_RenderDrawData(&overlay);
}

void usage(const char* argv0)
{
    std::fprintf(stderr,
                 "usage: %s [--server host[:port]] [--settings file] [--option key=value] [--size WxH] "
                 "[--screenshot file.bmp [--wait seconds]]\n",
                 argv0);
}

}  // namespace

int main(int argc, char** argv)
{
    const char* screenshotPath = nullptr;
    const char* settingsFile = nullptr;
    double wait = 3.0, every = 0.0;
    std::string keys, pick;
    double pickAt = 0.0;
    int sizeW = 0, sizeH = 0;
    saw::Server server, seatsFrom;
    bool haveServer = false, haveSeats = false;
    std::vector<std::pair<std::string, std::string>> options;
    for (int i = 1; i < argc; ++i) {
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(argv[i], "--server") == 0 && hasValue) {
            if (!saw::Server::parse(argv[++i], server)) {
                std::fprintf(stderr, "Not a server: %s (host or host:port)\n", argv[i]);
                return 2;
            }
            haveServer = true;
        } else if (std::strcmp(argv[i], "--seats") == 0 && hasValue) {
            haveSeats = saw::Server::parse(argv[++i], seatsFrom);
        } else if (std::strcmp(argv[i], "--settings") == 0 && hasValue) {
            settingsFile = argv[++i];
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && hasValue) {
            screenshotPath = argv[++i];
        } else if (std::strcmp(argv[i], "--wait") == 0 && hasValue) {
            wait = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--every") == 0 && hasValue) {
            every = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--pick") == 0 && hasValue && std::strchr(argv[i + 1], ':')) {
            pickAt = std::atof(argv[++i]);
            pick = std::strchr(argv[i], ':') + 1;
        } else if (std::strcmp(argv[i], "--keys") == 0 && hasValue) {
            keys = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && hasValue) {
            std::sscanf(argv[++i], "%dx%d", &sizeW, &sizeH);
        } else if (std::strcmp(argv[i], "--option") == 0 && hasValue && std::strchr(argv[i + 1], '=')) {
            const std::string kv = argv[++i];
            options.emplace_back(kv.substr(0, kv.find('=')), kv.substr(kv.find('=') + 1));
        } else {
            usage(argv[0]);
            return 2;
        }
    }

#ifdef SAW_VERSION
    SDL_SetAppMetadata(TH_TITLE, SAW_VERSION, "syn-ack-wire");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // A --screenshot run reads the settings but never writes them
    const std::string path = settingsFile ? settingsFile : saw::settingsPath();
    saw::Settings settings = path.empty() ? saw::Settings() : saw::Settings::load(path);

    float displayScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (displayScale <= 0.0f)
        displayScale = 1.0f;
    const int width = sizeW > 0 ? sizeW : settings.width > 0 ? settings.width : int(1280 * displayScale);
    const int height = sizeH > 0 ? sizeH : settings.height > 0 ? settings.height : int(760 * displayScale);
    SDL_Window* window = SDL_CreateWindow(TH_TITLE, width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 640, 400);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "Could not create an OpenGL 3.3 context: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    if (settings.fullscreen && !screenshotPath)
        SDL_SetWindowFullscreen(window, true);
    SDL_StartTextInput(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const std::string dataDir = saw::findDataDir();
    saw::loadFonts(dataDir);
    io.FontDefault = saw::menuFont();
    ImGui::GetStyle().FontSizeBase = 17.0f;
    ImGui::GetStyle().FontScaleDpi = SDL_GetWindowDisplayScale(window) / SDL_GetWindowPixelDensity(window);
    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 330");

    {
        saw::App app(window, std::move(settings), screenshotPath ? std::string() : path, dataDir);
        for (const auto& [key, value] : options)
            app.setOption(key, value);
        if (haveSeats)
            app.setSeatsSource(seatsFrom);
        app.start(haveServer ? &server : nullptr);
        const Uint64 begun = SDL_GetTicks(), shotAt = begun + Uint64(wait * 1000);
        Uint64 nextKey = begun + 1500, nextShot = every > 0 ? begun + Uint64(every * 1000) : 0;
        size_t keyAt = 0;
        int shots = 0;

        while (!app.quitRequested()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)))
                    app.requestClose();
                else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                    ImGui::GetStyle().FontScaleDpi = SDL_GetWindowDisplayScale(window) / SDL_GetWindowPixelDensity(window);
                else
                    app.event(event);
            }
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
                SDL_Delay(20);
                continue;
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            app.frame();
            ImGui::Render();
            int fbWidth = 0, fbHeight = 0;
            SDL_GetWindowSizeInPixels(window, &fbWidth, &fbHeight);
            render(app, fbWidth, fbHeight);
            const Uint64 now = SDL_GetTicks();
            if (keyAt < keys.size() && now >= nextKey) {
                const char k = keys[keyAt++];
                nextKey = now + (k == '|' ? 1000 : 150);
                if (k != '|')
                    key_push(k == '/' ? '\r' : k == '^' ? 0x1B : k == '!' ? 0x02 : static_cast<unsigned char>(k));
            }
            if (!pick.empty() && now >= begun + Uint64(pickAt * 1000)) {
                app.pick(pick);
                pick.clear();
            }
            if (nextShot && now >= nextShot) {
                char name[1024];
                std::snprintf(name, sizeof name, "%.*s-%03d.bmp", int(std::strlen(screenshotPath) - 4), screenshotPath, shots++);
                saveScreenshot(name, fbWidth, fbHeight);
                nextShot = now + Uint64(every * 1000);
            }
            if (screenshotPath && now >= shotAt) {
                if (!saveScreenshot(screenshotPath, fbWidth, fbHeight))
                    std::fprintf(stderr, "Could not save the screenshot: %s\n", SDL_GetError());
                break;
            }
            SDL_GL_SwapWindow(window);
        }
        if (!screenshotPath)
            app.finish();
    }

    // The game thread is still in its loop: the process ends under it, and the server sees the
    // connection drop (the seat goes to its bot until the player comes back)
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    std::_Exit(0);
}
