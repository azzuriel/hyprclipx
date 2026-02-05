// Single Responsibility: Configuration file parsing

#include "hyprclipx/ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace hyprclipx {

std::string getConfigPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::string configDir;

    if (xdgConfig) {
        configDir = xdgConfig;
    } else {
        const char* home = std::getenv("HOME");
        configDir = home ? std::string(home) + "/.config" : "/tmp";
    }

    return configDir + "/hypr/hyprclipx.toml";
}

std::string getDataDir() {
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    std::string dataDir;

    if (xdgData) {
        dataDir = xdgData;
    } else {
        const char* home = std::getenv("HOME");
        dataDir = home ? std::string(home) + "/.local/share" : "/tmp";
    }

    dataDir += "/hyprclipx";

    // Create directory if it doesn't exist
    fs::create_directories(dataDir);

    return dataDir;
}

// Simple TOML-like parser (manual, no external dependency)
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

static std::string parseString(const std::string& value) {
    std::string v = trim(value);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        return v.substr(1, v.size() - 2);
    }
    return v;
}

static int parseInt(const std::string& value) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return 0;
    }
}

static bool parseBool(const std::string& value) {
    std::string v = trim(value);
    return v == "true" || v == "1";
}

Config loadConfig() {
    Config config;
    config.configPath = getConfigPath();
    config.dataDir = getDataDir();

    std::ifstream file(config.configPath);
    if (!file.is_open()) {
        // Return defaults if config doesn't exist
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '[') {
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        // Parse known keys
        if (key == "window_width") config.windowWidth = parseInt(value);
        else if (key == "window_height") config.windowHeight = parseInt(value);
        else if (key == "offset_x") config.offsetX = parseInt(value);
        else if (key == "offset_y") config.offsetY = parseInt(value);
        else if (key == "bg_color") config.bgColor = parseString(value);
        else if (key == "fg_color") config.fgColor = parseString(value);
        else if (key == "accent_color") config.accentColor = parseString(value);
        else if (key == "border_color") config.borderColor = parseString(value);
        else if (key == "border_width") config.borderWidth = parseInt(value);
        else if (key == "border_radius") config.borderRadius = parseInt(value);
        else if (key == "max_items") config.maxItems = parseInt(value);
        else if (key == "show_images") config.showImages = parseBool(value);
        else if (key == "show_favorites") config.showFavorites = parseBool(value);
        else if (key == "hotkey") config.hotkey = parseString(value);
    }

    return config;
}

bool saveConfig(const Config& config) {
    std::ofstream file(config.configPath);
    if (!file.is_open()) {
        return false;
    }

    file << "# HyprClipX Configuration\n\n";
    file << "[general]\n";
    file << "hotkey = \"" << config.hotkey << "\"\n";
    file << "max_items = " << config.maxItems << "\n";
    file << "show_images = " << (config.showImages ? "true" : "false") << "\n";
    file << "show_favorites = " << (config.showFavorites ? "true" : "false") << "\n\n";

    file << "[window]\n";
    file << "window_width = " << config.windowWidth << "\n";
    file << "window_height = " << config.windowHeight << "\n";
    file << "offset_x = " << config.offsetX << "\n";
    file << "offset_y = " << config.offsetY << "\n\n";

    file << "[appearance]\n";
    file << "bg_color = \"" << config.bgColor << "\"\n";
    file << "fg_color = \"" << config.fgColor << "\"\n";
    file << "accent_color = \"" << config.accentColor << "\"\n";
    file << "border_color = \"" << config.borderColor << "\"\n";
    file << "border_width = " << config.borderWidth << "\n";
    file << "border_radius = " << config.borderRadius << "\n";

    return true;
}

} // namespace hyprclipx
