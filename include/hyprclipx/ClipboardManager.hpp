#pragma once
// IPC client for clipman-daemon (Unix socket on /tmp/clipman.sock)
// Mirrors AGS's execAsync("python3 clipman-client.py ...") calls

#include "ClipboardEntry.hpp"
#include "Config.hpp"
#include <string>
#include <vector>

namespace hyprclipx {

class ClipboardManager {
public:
    explicit ClipboardManager(const Config& config);

    // Daemon commands (matching clipman-client.py)
    std::vector<ClipboardEntry> fetchItems(const std::string& filter = "all",
                                           const std::string& search = "",
                                           int limit = 50);
    bool paste(const std::string& uuid);
    bool toggleFavorite(const std::string& uuid);
    bool deleteItem(const std::string& uuid);
    bool clearAll();
    bool ping();

private:
    const Config& m_config;

    // Send command to daemon, return JSON response
    std::string sendCommand(const std::string& cmd, const std::string& argsJson = "{}");

    // Parse JSON list response into entries
    std::vector<ClipboardEntry> parseListResponse(const std::string& json);
};

} // namespace hyprclipx
