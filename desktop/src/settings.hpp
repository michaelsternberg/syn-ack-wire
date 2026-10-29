#pragma once

#include <string>
#include <vector>

// What the desktop client remembers between runs, as key = value lines in synack.ini in the
// per-user application data folder.
namespace saw {

struct Server {
    std::string host;
    unsigned short port = 6502;
    std::string label() const;                   // "host:port"
    static bool parse(const std::string& s, Server& out);  // "host:port" or "host" (port 6502)
    bool operator==(const Server& o) const { return host == o.host && port == o.port; }
};

struct Settings {
    std::string theme = "dos";
    std::string font = "normal";
    std::string crt = "low";
    std::string server;                          // a region's name, or "host:port"
    std::vector<std::string> custom;             // custom servers used, newest first (at most 4)
    std::string handle;
    std::string token;                           // 8 hex digits: a seat to take back...
    std::string tokenServer;                     // ...on this server
    bool hints = true;
    bool sound = true;
    int width = 0, height = 0;
    bool fullscreen = false;
    bool autoHideMenu = false;                   // the menu bar shows only when the mouse is at the top

    static Settings load(const std::string& path);
    bool save(const std::string& path) const;
};

std::string settingsPath();                      // empty if there's no per-user folder

}  // namespace saw
