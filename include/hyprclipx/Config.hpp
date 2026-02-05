#pragma once
// Single Responsibility: Configuration data structure

#include <string>

namespace hyprclipx {

struct Config {
    // Window dimensions
    int windowWidth = 450;
    int windowHeight = 550;

    // Position offset (relative to caret/cursor)
    int offsetX = 0;
    int offsetY = 0;

    // Appearance
    std::string bgColor = "#1e1e2e";
    std::string fgColor = "#cdd6f4";
    std::string accentColor = "#89b4fa";
    std::string borderColor = "#313244";
    int borderWidth = 2;
    int borderRadius = 12;

    // Behavior
    int maxItems = 50;
    bool showImages = true;
    bool showFavorites = true;
    std::string hotkey = "SUPER+V";

    // Paths
    std::string dataDir;      // ~/.local/share/hyprclipx/
    std::string configPath;   // ~/.config/hypr/hyprclipx.toml
};

} // namespace hyprclipx
