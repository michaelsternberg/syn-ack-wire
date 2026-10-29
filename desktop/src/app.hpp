#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "crt.hpp"
#include "snow.hpp"
#include "imgui.h"
#include "settings.hpp"
#include "theme.hpp"

// The window: the menu bar, the dialogs, the 80 x 25 screen the game thread draws (desk.h), and
// the keys it is sent.
namespace saw {

class App {
public:
    App(SDL_Window* window, Settings settings, std::string settingsPath, std::string dataDir);

    void start(const Server* override);  // the game thread, on the saved server or `override`
    void frame();                         // one frame of ImGui: menus, dialogs, the screen
    void event(const SDL_Event& e);       // keys for the game, after ImGui has seen the event

    void requestClose();                  // the window's close box, File > Quit, Esc then Y
    bool quitRequested() const { return quit_; }
    void finish();                        // saves the settings: the handle, a seat to take back

    // For the CRT pass (main.cpp): the screen's draw list and where it is
    bool crtOn() const { return crtLevel_ != CrtLevel::Off; }
    CrtEffect& crt() { return crt_; }
    CrtParams crtParameters() const { return crtParams(crtLevel_); }
    bool isContentDrawList(const ImDrawList* list) const { return list == content_; }
    ImVec2 crtMin() const { return crtMin_; }
    ImVec2 crtMax() const { return crtMax_; }
    float scanlineSpacing() const { return cellH_ / 8.0f; }  // framebuffer pixels: 8 scanlines to a text row
    uint32_t backgroundColour() const { return theme_->role[R_TEXT].bg; }

    void setOption(const std::string& key, const std::string& value);  // --option, as in synack.ini
    void pick(const std::string& server);  // --pick: a region's name or host:port, as from the Region menu
    void setSeatsSource(const Server& s) { seatsFrom_ = s; }  // --seats: where to read /SEATS (tests)

private:
    void revealMenuBar();
    void menus();
    void dialogs();
    void drawScreen();
    void chooseServer(const Server& s, const std::string& name);
    void switchTo(const Server& s, const std::string& name);
    void setTheme(const std::string& key);
    void toggleFullscreen();
    void updateTitle();
    Server currentServer(std::string* name = nullptr) const;
    bool popupOpen() const;

    SDL_Window* window_;
    Settings settings_;
    std::string settingsPath_, dataDir_;
    const Theme* theme_;
    CrtLevel crtLevel_;
    CrtEffect crt_;
    TitleSnow snow_;
    bool quit_ = false;
    bool started_ = false;

    // dialogs to open on the next frame, and their state
    bool askCustom_ = false, askLeave_ = false, askQuit_ = false, showKeys_ = false, showAbout_ = false;
    Server pending_;
    std::string pendingName_;
    char customHost_[256] = "";
    int customPort_ = 6502;
    std::string customError_;

    // The menu bar: whether it shows this frame (always, unless auto-hidden), a menu of it is open
    bool menuShown_ = true, menuOpen_ = false;
    bool regionMenuWasOpen_ = false;
    Server seatsFrom_;  // the regions' /SEATS page (regions_tab.h), unless --seats says otherwise
    float menuBarHeight_ = 20.0f;

    ImDrawList* content_ = nullptr;
    ImVec2 crtMin_, crtMax_;
    float cellH_ = 16.0f;  // a cell's height in framebuffer pixels
};

}  // namespace saw
