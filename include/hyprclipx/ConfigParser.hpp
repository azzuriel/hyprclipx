#pragma once
// Single Responsibility: Configuration file parsing

#include "Config.hpp"
#include <string>

namespace hyprclipx {

// Load config from ~/.config/hypr/hyprclipx.toml
Config loadConfig();

// Save config to file
bool saveConfig(const Config& config);

// Get config file path
std::string getConfigPath();

// Get data directory path
std::string getDataDir();

} // namespace hyprclipx
