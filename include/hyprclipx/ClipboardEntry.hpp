#pragma once
// Data structure matching clipman-daemon's JSON response

#include <string>

namespace hyprclipx {

struct ClipboardEntry {
    std::string uuid;
    std::string type;         // "text" or "image"
    std::string preview;
    std::string thumb;        // Full path to thumbnail (images only)
    bool favorite = false;
    std::string createdAt;
};

} // namespace hyprclipx
