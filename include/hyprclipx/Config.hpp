#pragma once
#include <string>

namespace hyprclipx {

struct Config {
    // Window dimensions (compact horizontal layout)
    int windowWidth = 600;
    int windowHeight = 220;

    // Caret offset (persisted in user-settings.json)
    int offsetX = 0;
    int offsetY = 0;

    // Behavior
    int maxItems = 50;
    std::string hotkey = "SUPER V";

    // Paths
    std::string clipmanClient;    // path to clipman-client.py
    std::string caretHelper;      // path to get-caret-position.py
    std::string userSettingsFile;  // path to user-settings.json
    std::string caretPosFile = "/tmp/clipboard-manager-caret-pos";
    std::string prevWindowFile = "/tmp/clipboard-manager-prev-window";
    std::string socketPath = "/tmp/clipman.sock";
};

} // namespace hyprclipx
