#include "settings.hpp"

#include <cstdlib>
#include <fstream>

#include <SDL3/SDL.h>

namespace saw {

namespace {

std::string trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

bool flag(const std::string& v)
{
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

}  // namespace

std::string Server::label() const
{
    return host + ":" + std::to_string(port);
}

bool Server::parse(const std::string& text, Server& out)
{
    const std::string s = trim(text);
    const size_t colon = s.rfind(':');
    Server sv;
    sv.host = colon == std::string::npos ? s : s.substr(0, colon);
    if (colon != std::string::npos) {
        char* end = nullptr;
        const long p = std::strtol(s.c_str() + colon + 1, &end, 10);
        if (*end || p < 1 || p > 65535)
            return false;
        sv.port = static_cast<unsigned short>(p);
    }
    if (sv.host.empty() || sv.host.find_first_of(" \t/") != std::string::npos)
        return false;
    out = sv;
    return true;
}

Settings Settings::load(const std::string& path)
{
    Settings s;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#')
            continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (k == "theme") s.theme = v;
        else if (k == "font") s.font = v;
        else if (k == "crt") s.crt = v;
        else if (k == "server") s.server = v;
        else if (k == "custom" && s.custom.size() < 4) s.custom.push_back(v);
        else if (k == "handle") s.handle = v.substr(0, 8);
        else if (k == "token") s.token = v;
        else if (k == "token_server") s.tokenServer = v;
        else if (k == "hints") s.hints = flag(v);
        else if (k == "sound") s.sound = flag(v);
        else if (k == "width") s.width = std::atoi(v.c_str());
        else if (k == "height") s.height = std::atoi(v.c_str());
        else if (k == "fullscreen") s.fullscreen = flag(v);
        else if (k == "auto_hide_menu") s.autoHideMenu = flag(v);
    }
    return s;
}

bool Settings::save(const std::string& path) const
{
    std::ofstream out(path);
    out << "# SYN-ACK WIRE desktop client\n"
        << "theme = " << theme << "\nfont = " << font << "\ncrt = " << crt << "\nserver = " << server << "\n";
    for (const std::string& c : custom)
        out << "custom = " << c << "\n";
    out << "handle = " << handle << "\n";
    if (!token.empty())
        out << "token = " << token << "\ntoken_server = " << tokenServer << "\n";
    out << "hints = " << hints << "\nsound = " << sound << "\nwidth = " << width << "\nheight = " << height
        << "\nfullscreen = " << fullscreen << "\nauto_hide_menu = " << autoHideMenu << "\n";
    return static_cast<bool>(out);
}

std::string settingsPath()
{
    if (char* pref = SDL_GetPrefPath("SynAckWire", "SynAckWire")) {
        std::string path = std::string(pref) + "synack.ini";
        SDL_free(pref);
        return path;
    }
    return {};
}

}  // namespace saw
