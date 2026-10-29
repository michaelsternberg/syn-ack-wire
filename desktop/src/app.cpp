#include "app.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../client/src/common/theme_tab.h"
#define __fastcall__  // cc65's, in platform.h
#include "../../client/src/platform.h"
#include "desk.h"
#include "fonts.hpp"
#include "glyphs.hpp"
#include "regions_tab.h"

#ifndef SAW_VERSION
#define SAW_VERSION "dev"
#endif

// Esc, then Y on the title screen (desk.c, on the game thread): the window closes as File > Quit
extern "C" void desk_request_quit(void)
{
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

namespace saw {

namespace {

constexpr float kCellAspect = 2.25f;  // a cell's height to its width, near a DOS screen's on a 4:3 monitor

ImU32 col(uint32_t rgb)
{
    return IM_COL32(rgb >> 16 & 0xFF, rgb >> 8 & 0xFF, rgb & 0xFF, 0xFF);
}

std::string hex(const unsigned char* b, int n)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < n; ++i) {
        s += digits[b[i] >> 4];
        s += digits[b[i] & 15];
    }
    return s;
}

// A term from theme.toml for a sentence: "LOG OFF" -> "log off"
std::string term(const char* t)
{
    std::string s(t);
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool unhex(const std::string& s, unsigned char* b, int n)
{
    if (static_cast<int>(s.size()) != 2 * n)
        return false;
    for (int i = 0; i < n; ++i) {
        unsigned v = 0;
        if (std::sscanf(s.c_str() + 2 * i, "%2x", &v) != 1)
            return false;
        b[i] = static_cast<unsigned char>(v);
    }
    return true;
}

}  // namespace

App::App(SDL_Window* window, Settings settings, std::string settingsPath, std::string dataDir)
    : window_(window), settings_(std::move(settings)), settingsPath_(std::move(settingsPath)), dataDir_(std::move(dataDir)),
      theme_(&themeByKey(settings_.theme)), crtLevel_(crtLevelFromKey(settings_.crt.c_str())),
      seatsFrom_{REGION_HOST, REGION_SEATS_PORT}
{
    applyImGuiStyle(*theme_);
}

// The server picked last time: a region by name, else a custom "host:port"; the first region if neither
Server App::currentServer(std::string* name) const
{
    for (const region_entry& r : regions)
        if (settings_.server == r.name) {
            if (name)
                *name = r.name;
            return {REGION_HOST, r.port};
        }
    Server s;
    if (Server::parse(settings_.server, s)) {
        if (name)
            *name = s.label();
        return s;
    }
    if (name)
        *name = regions[0].name;
    return {REGION_HOST, regions[0].port};
}

void App::start(const Server* override)
{
    if (override) {
        settings_.server = override->label();
        if (std::find(settings_.custom.begin(), settings_.custom.end(), settings_.server) == settings_.custom.end())
            settings_.custom.insert(settings_.custom.begin(), settings_.server);
        settings_.custom.resize(std::min<size_t>(settings_.custom.size(), 4));
    }
    std::string name;
    const Server s = currentServer(&name);
    settings_.server = name;
    // A seat left by quitting is taken back with its token, on the same server only
    unsigned char token[4] = {0, 0, 0, 0};
    if (settings_.tokenServer != name || !unhex(settings_.token, token, 4))
        std::memset(token, 0, sizeof token);
    game_setup setup{s.host.c_str(), s.port, settings_.handle.c_str(), token, settings_.hints, settings_.sound,
                     dataDir_.c_str()};
    game_start(&setup);
    seats_fetch(seatsFrom_.host.c_str(), seatsFrom_.port);
    started_ = true;
    updateTitle();
}

void App::updateTitle()
{
    std::string name;
    currentServer(&name);
    SDL_SetWindowTitle(window_, (std::string(TH_TITLE) + " - " + name).c_str());
}

void App::setTheme(const std::string& key)
{
    settings_.theme = key;
    theme_ = &themeByKey(key);
    applyImGuiStyle(*theme_);
}

void App::setOption(const std::string& key, const std::string& value)
{
    if (key == "theme")
        setTheme(value);
    else if (key == "font")
        settings_.font = value;
    else if (key == "auto_hide_menu")
        settings_.autoHideMenu = value == "on" || value == "1" || value == "true";
    else if (key == "crt") {
        settings_.crt = value;
        crtLevel_ = crtLevelFromKey(value.c_str());
    }
}

void App::toggleFullscreen()
{
    settings_.fullscreen = !(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(window_, settings_.fullscreen);
}

// ------------------------------------------------------------------ servers

void App::chooseServer(const Server& s, const std::string& name)
{
    std::string current;
    if (currentServer(&current) == s && current == name)
        return;
    if (game_seated()) {  // in a game, or seated in the lobby: ask first
        pending_ = s;
        pendingName_ = name;
        askLeave_ = true;
        return;
    }
    switchTo(s, name);
}

void App::pick(const std::string& what)
{
    const bool yes = !what.empty() && what[0] == '!';
    const std::string name = yes ? what.substr(1) : what;
    Server s;
    for (const region_entry& r : regions)
        if (name == r.name)
            s = {REGION_HOST, r.port};
    if (s.host.empty() && !Server::parse(name, s))
        return;
    if (yes)
        switchTo(s, name);
    else
        chooseServer(s, name);
}

void App::switchTo(const Server& s, const std::string& name)
{
    game_switch(s.host.c_str(), s.port, 1);
    settings_.server = name;
    settings_.token.clear();
    settings_.tokenServer.clear();
    updateTitle();
}

void App::requestClose()
{
    if (game_seated() && !askQuit_ && !quit_) {
        askQuit_ = true;
        return;
    }
    quit_ = true;
}

void App::finish()
{
    char handle[9];
    unsigned char token[4];
    int hints = 1, sound = 1;
    game_state(handle, token, &hints, &sound);
    settings_.handle = handle;
    settings_.hints = hints != 0;
    settings_.sound = sound != 0;
    if (token[0] | token[1] | token[2] | token[3]) {
        settings_.token = hex(token, 4);
        settings_.tokenServer = settings_.server;
    } else {
        settings_.token.clear();
        settings_.tokenServer.clear();
    }
    if (!(SDL_GetWindowFlags(window_) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
        SDL_GetWindowSize(window_, &settings_.width, &settings_.height);
    if (!settingsPath_.empty() && !settings_.save(settingsPath_))
        std::fprintf(stderr, "Could not save %s\n", settingsPath_.c_str());
}

// ------------------------------------------------------------------ keys

bool App::popupOpen() const
{
    return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || askCustom_ ||
           askLeave_ || askQuit_ || showKeys_ || showAbout_;
}

// The keys the MS-DOS client has, as platform.h's codes: printable keys arrive as text (so any
// keyboard layout works), uppercased; the others by key
void App::event(const SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11) {
        toggleFullscreen();
        return;
    }
    if (popupOpen() || ImGui::GetIO().WantCaptureKeyboard)
        return;
    if (e.type == SDL_EVENT_TEXT_INPUT) {
        for (const char* p = e.text.text; *p; ++p) {
            unsigned char c = static_cast<unsigned char>(*p);
            if (c < 0x20 || c > 0x7E)
                continue;
            if (c == '?')
                c = K_HELP;
            else if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
            key_push(c);
        }
        return;
    }
    if (e.type != SDL_EVENT_KEY_DOWN)
        return;
    switch (e.key.key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER: key_push(K_RETURN); break;
    case SDLK_BACKSPACE:
    case SDLK_DELETE: key_push(K_DELETE); break;
    case SDLK_ESCAPE: key_push(K_ESC); break;
    case SDLK_F1: key_push(K_HELP); break;
    case SDLK_F2: key_push(K_OPTION); break;
    case SDLK_F3: key_push(K_SOUND); break;
    case SDLK_LEFT: key_push('<'); break;
    case SDLK_RIGHT: key_push('>'); break;
    default: break;
    }
}

// ------------------------------------------------------------------ menus

// With Auto-Hide Menu Bar on, the bar shows while the mouse is at the window's top edge, and goes
// again once the mouse leaves it and no menu is open (as in ../4AD_SDL)
void App::revealMenuBar()
{
    if (!settings_.autoHideMenu) {
        menuShown_ = true;
        return;
    }
    float x = 0.0f, y = 0.0f;
    SDL_GetMouseState(&x, &y);
    const bool inWindow = SDL_GetMouseFocus() == window_;
    const float edge = 4.0f;
    if (inWindow && y <= edge)
        menuShown_ = true;
    else if (menuShown_ && !menuOpen_ && !(inWindow && y < menuBarHeight_ + edge))
        menuShown_ = false;
}

void App::menus()
{
    menuOpen_ = false;
    if (!ImGui::BeginMainMenuBar())
        return;
    menuBarHeight_ = ImGui::GetFrameHeight();
    if (ImGui::BeginMenu("File")) {
        menuOpen_ = true;
        if (ImGui::MenuItem("Quit"))
            requestClose();
        ImGui::EndMenu();
    }
    const bool regionOpen = ImGui::BeginMenu("Region");
    if (regionOpen && !regionMenuWasOpen_ && seats_age() > 10000)  // counts at most 10 s old when it opens
        seats_fetch(seatsFrom_.host.c_str(), seatsFrom_.port);
    regionMenuWasOpen_ = regionOpen;
    if (regionOpen) {
        menuOpen_ = true;
        std::string current;
        const Server now = currentServer(&current);
        for (const region_entry& r : regions) {
            // The seats free there, as the FujiNet lobby counts them; nothing until the page answers
            int taken = 0, seats = 0;
            std::string free;
            if (seats_of(r.name, &taken, &seats))
                free = taken >= seats ? "full" : std::to_string(seats - taken) + " of " + std::to_string(seats) + " seats free";
            const std::string label = std::string(r.name) + "  (" + r.pace + ")";
            if (ImGui::MenuItem(label.c_str(), free.empty() ? nullptr : free.c_str(), current == r.name))
                chooseServer({REGION_HOST, r.port}, r.name);
        }
        // Counts again now; the menu stays open, and they show as soon as the page answers
        ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
        if (ImGui::MenuItem("Refresh Seats"))
            seats_fetch(seatsFrom_.host.c_str(), seatsFrom_.port);
        ImGui::PopItemFlag();
        ImGui::Separator();
        for (const std::string& c : settings_.custom) {
            Server s;
            if (Server::parse(c, s) && ImGui::MenuItem(c.c_str(), nullptr, current == c))
                chooseServer(s, c);
        }
        if (ImGui::MenuItem("Custom Server...")) {
            Server s = now;
            if (current != s.label())
                s = {"192.168.1.", 6502};  // a server on the local network, most likely
            std::snprintf(customHost_, sizeof customHost_, "%s", s.host.c_str());
            customPort_ = s.port;
            customError_.clear();
            askCustom_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Options")) {
        menuOpen_ = true;
        if (ImGui::BeginMenu("Colour Theme")) {
            for (const Theme& t : themes())
                if (ImGui::MenuItem(t.name, nullptr, theme_ == &t))
                    setTheme(t.key);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Font")) {
            for (const FontTheme& f : fontThemes())
                if (ImGui::MenuItem(f.name, nullptr, settings_.font == f.key))
                    settings_.font = f.key;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("CRT Effects")) {
            for (CrtLevel level : kCrtLevels)
                if (ImGui::MenuItem(crtLevelLabel(level), nullptr, crtLevel_ == level)) {
                    crtLevel_ = level;
                    settings_.crt = crtLevelKey(level);
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Full Screen", "F11", (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0))
            toggleFullscreen();
        if (ImGui::MenuItem("Auto-Hide Menu Bar", nullptr, settings_.autoHideMenu))
            settings_.autoHideMenu = !settings_.autoHideMenu;
        ImGui::Separator();
        if (ImGui::MenuItem("Move Hints", "F2", game_hints() != 0))
            key_push(K_OPTION);
        if (ImGui::MenuItem("Sound", "F3", snd_is_on() != 0))
            key_push(K_SOUND);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        menuOpen_ = true;
        if (ImGui::MenuItem("Instructions", "F1"))
            key_push(K_HELP);
        if (ImGui::MenuItem("Keys"))
            showKeys_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem("About"))
            showAbout_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

// ------------------------------------------------------------------ dialogs

namespace {

// Opens a modal the frame its flag is set; the flag stays up while it shows, so keys stay out of the game
bool beginModal(const char* name, bool& flag)
{
    if (flag && !ImGui::IsPopupOpen(name))
        ImGui::OpenPopup(name);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        return true;
    flag = false;
    return false;
}

void endModal(bool& flag, bool close)
{
    if (close) {
        flag = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

bool escapeOr(bool pressed)
{
    return pressed || ImGui::IsKeyPressed(ImGuiKey_Escape);
}

}  // namespace

void App::dialogs()
{
    const float width = ImGui::GetFontSize() * 26;

    if (beginModal("Custom Server", askCustom_)) {
        ImGui::TextUnformatted("A server on your own network, or anywhere:");
        ImGui::Spacing();
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(width);
        const bool enter = ImGui::InputText("Host", customHost_, sizeof customHost_, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetNextItemWidth(width * 0.4f);
        ImGui::InputInt("Port", &customPort_, 0);
        if (!customError_.empty())
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", customError_.c_str());
        ImGui::Spacing();
        bool close = false;
        if (ImGui::Button("Connect") || enter) {
            Server s;
            if (customPort_ < 1 || customPort_ > 65535 || !Server::parse(customHost_, s) ||
                std::strchr(customHost_, ':')) {
                customError_ = "A host name or address, and a port from 1 to 65535";
            } else {
                s.port = static_cast<unsigned short>(customPort_);
                const std::string label = s.label();
                settings_.custom.erase(std::remove(settings_.custom.begin(), settings_.custom.end(), label),
                                       settings_.custom.end());
                settings_.custom.insert(settings_.custom.begin(), label);
                settings_.custom.resize(std::min<size_t>(settings_.custom.size(), 4));
                close = true;
                endModal(askCustom_, true);
                chooseServer(s, label);
                return;
            }
        }
        ImGui::SameLine();
        close = escapeOr(ImGui::Button("Cancel"));
        endModal(askCustom_, close);
    }

    if (beginModal("Leave the Game?", askLeave_)) {
        ImGui::Text("Going to %s means you %s at this table:", pendingName_.c_str(), term(T_LOGOFF).c_str());
        ImGui::TextUnformatted("a bot takes your seat for good.");
        ImGui::Spacing();
        bool close = false;
        if (ImGui::Button(("Go (" + term(T_LOGOFF) + ")").c_str())) {
            switchTo(pending_, pendingName_);
            close = true;
        }
        ImGui::SameLine();
        close = escapeOr(ImGui::Button("Stay")) || close;
        endModal(askLeave_, close);
    }

    if (beginModal("Quit?", askQuit_)) {
        ImGui::TextUnformatted("You have a seat at the table. Its bot plays for you until your");
        ImGui::Text("next %s: start the game again and press Enter.", term(T_JACKIN).c_str());
        ImGui::Spacing();
        bool close = false;
        if (ImGui::Button("Quit")) {
            quit_ = true;
            close = true;
        }
        ImGui::SameLine();
        close = escapeOr(ImGui::Button("Cancel")) || close;
        endModal(askQuit_, close);
    }

    if (beginModal("Keys", showKeys_)) {
        static const std::string keys[][2] = {
            {"F1 or ?", "instructions, any time (arrows turn pages)"},
            {"F2", "move hints on or off"},
            {"F3", "sound on or off"},
            {"F11", "full screen"},
            {"Enter", term(T_JACKIN) + ", with the handle typed"},
            {"Esc, then Y", term(T_LOGOFF) + " (in a game) or quit (title)"},
            {"2-6", "host in the lobby: start with that many seats"},
            {"1-6", "your turn: run that " + term(T_EXPLOIT)},
            {"letters", "pick a syndicate; buy " + term(T_COIN) + " (Backspace undoes)"},
        };
        if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (const auto& k : keys) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k[0].c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k[1].c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        endModal(showKeys_, escapeOr(ImGui::Button("Close")));
    }

    if (beginModal("About", showAbout_)) {
        ImGui::Text("%s for the desktop, version %s", TH_TITLE, SAW_VERSION);
        ImGui::TextUnformatted("The same tables as the Atari, Apple II, MS-DOS and CoCo.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Free software under the GNU GPL, version 3.");
        ImGui::TextUnformatted("Fonts under the SIL Open Font License.");
        ImGui::Spacing();
        endModal(showAbout_, escapeOr(ImGui::Button("Close")));
    }
}

// ------------------------------------------------------------------ the screen

// The game thread's 80 x 25 cells, as big as fit below the menu bar, centred; each cell's
// background, then its character. Cell edges are on whole pixels, so neighbours meet without seams.
void App::drawScreen()
{
    // An auto-hidden menu bar takes no room: it shows over the top of the screen
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 pos = settings_.autoHideMenu ? vp->Pos : vp->WorkPos;
    const ImVec2 size = settings_.autoHideMenu ? vp->Size : vp->WorkSize;
    const ImVec2 min(pos), max(pos.x + size.x, pos.y + size.y);
    content_ = ImGui::GetBackgroundDrawList();
    crtMin_ = min;
    crtMax_ = max;
    content_->AddRectFilled(min, max, col(theme_->role[R_TEXT].bg));

    const float scale = ImGui::GetIO().DisplayFramebufferScale.y;
    const float cw = std::min(size.x / SCR_COLS, size.y / (SCR_ROWS * kCellAspect));
    const float ch = cw * kCellAspect;
    cellH_ = ch * scale;
    const float x0 = std::round(min.x + (size.x - cw * SCR_COLS) / 2);
    const float y0 = std::round(min.y + (size.y - ch * SCR_ROWS) / 2);
    auto edgeX = [&](int c) { return std::round(x0 + c * cw); };
    auto edgeY = [&](int r) { return std::round(y0 + r * ch); };
    const CellFont cf = fitCell(fontThemeByKey(settings_.font), cw, ch);

    static cell snap[SCR_ROWS][SCR_COLS];
    scr_lock();
    std::memcpy(snap, scr, sizeof snap);
    scr_unlock();

    for (int r = 0; r < SCR_ROWS; ++r)
        for (int c = 0; c < SCR_COLS; ++c) {
            const Pair& p = theme_->role[snap[r][c].role < NROLES ? int(snap[r][c].role) : int(R_TEXT)];
            if (p.bg != theme_->role[R_TEXT].bg)
                content_->AddRectFilled(ImVec2(edgeX(c), edgeY(r)), ImVec2(edgeX(c + 1), edgeY(r + 1)), col(p.bg));
        }
    for (int r = 0; r < SCR_ROWS; ++r)
        for (int c = 0; c < SCR_COLS; ++c) {
            const int role = snap[r][c].role < NROLES ? int(snap[r][c].role) : int(R_TEXT);
            drawGlyph(content_, ImVec2(edgeX(c), edgeY(r)), edgeX(c + 1) - edgeX(c), edgeY(r + 1) - edgeY(r),
                      snap[r][c].ch, col(theme_->role[role].fg), cf, role >= R_HIVE);
        }
}

void App::frame()
{
    revealMenuBar();
    if (menuShown_)
        menus();
    drawScreen();
    dialogs();
}

}  // namespace saw
