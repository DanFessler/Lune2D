#include "platform_paths.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>

#if defined(__APPLE__)
#include <climits>
#include <cstdlib>
#endif

std::string eng_web_ui_root_path() {
    if (const char* dev = getenv("WEBVIEW_DEV_URL"))
        if (dev[0] != '\0')
            return std::string(dev);
    std::string p = std::string(SDL_GetBasePath()) + "../web/dist";
#if defined(__APPLE__)
    char resolved[PATH_MAX];
    if (realpath(p.c_str(), resolved))
        p = resolved;
#endif
    return p;
}
