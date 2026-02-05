#pragma once
// Single Responsibility: Data structure for clipboard items

#include <string>
#include <chrono>
#include <vector>

namespace hyprclipx {

enum class EntryType {
    Text,
    Image
};

struct ClipboardEntry {
    std::string uuid;
    EntryType type;
    std::string preview;      // Text preview or image dimensions
    std::string content;      // Full text or image path
    std::string thumbPath;    // Thumbnail path for images
    bool favorite = false;
    std::chrono::system_clock::time_point createdAt;

    // Helpers
    bool isText() const { return type == EntryType::Text; }
    bool isImage() const { return type == EntryType::Image; }
};

} // namespace hyprclipx
