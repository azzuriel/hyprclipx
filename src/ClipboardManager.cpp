// Single Responsibility: Clipboard operations

#include "hyprclipx/ClipboardManager.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <cstdlib>
#include <array>
#include <filesystem>

namespace fs = std::filesystem;

namespace hyprclipx {

ClipboardManager::ClipboardManager(const Config& config)
    : m_config(config) {
}

ClipboardManager::~ClipboardManager() {
    stopMonitoring();
}

std::string ClipboardManager::generateUUID() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << '-';
        }
        ss << dis(gen);
    }
    return ss.str();
}

std::string ClipboardManager::getDataFilePath() const {
    return m_config.dataDir + "/clipboard.json";
}

std::vector<ClipboardEntry> ClipboardManager::getItems(const std::string& filter,
                                                        const std::string& search,
                                                        int limit) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ClipboardEntry> result;
    result.reserve(limit);

    for (const auto& entry : m_entries) {
        // Apply filter
        if (filter == "favorites" && !entry.favorite) continue;
        if (filter == "text" && entry.type != EntryType::Text) continue;
        if (filter == "image" && entry.type != EntryType::Image) continue;

        // Apply search
        if (!search.empty()) {
            if (entry.preview.find(search) == std::string::npos &&
                entry.content.find(search) == std::string::npos) {
                continue;
            }
        }

        result.push_back(entry);
        if (static_cast<int>(result.size()) >= limit) break;
    }

    return result;
}

bool ClipboardManager::addItem(const std::string& content, EntryType type) {
    if (content.empty()) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate
    for (auto& entry : m_entries) {
        if (entry.content == content) {
            // Move to front (most recent)
            auto it = std::find_if(m_entries.begin(), m_entries.end(),
                [&content](const ClipboardEntry& e) { return e.content == content; });
            if (it != m_entries.begin()) {
                ClipboardEntry temp = std::move(*it);
                m_entries.erase(it);
                m_entries.insert(m_entries.begin(), std::move(temp));
            }
            return true;
        }
    }

    // Create new entry
    ClipboardEntry entry;
    entry.uuid = generateUUID();
    entry.type = type;
    entry.content = content;
    entry.createdAt = std::chrono::system_clock::now();

    // Create preview
    if (type == EntryType::Text) {
        entry.preview = content.substr(0, 100);
        // Replace newlines with spaces
        std::replace(entry.preview.begin(), entry.preview.end(), '\n', ' ');
    } else {
        entry.preview = "[Image]";
        entry.thumbPath = createThumbnail(content);
    }

    // Add to front
    m_entries.insert(m_entries.begin(), entry);

    // Limit total entries
    while (static_cast<int>(m_entries.size()) > m_config.maxItems) {
        // Remove oldest non-favorite
        for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
            if (!it->favorite) {
                m_entries.erase(std::next(it).base());
                break;
            }
        }
    }

    if (m_onChangeCallback) {
        m_onChangeCallback();
    }

    return true;
}

bool ClipboardManager::removeItem(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&uuid](const ClipboardEntry& e) { return e.uuid == uuid; });

    if (it != m_entries.end()) {
        m_entries.erase(it);
        if (m_onChangeCallback) m_onChangeCallback();
        return true;
    }
    return false;
}

bool ClipboardManager::toggleFavorite(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&uuid](const ClipboardEntry& e) { return e.uuid == uuid; });

    if (it != m_entries.end()) {
        it->favorite = !it->favorite;
        return true;
    }
    return false;
}

void ClipboardManager::clearAll(bool keepFavorites) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (keepFavorites) {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [](const ClipboardEntry& e) { return !e.favorite; }),
            m_entries.end());
    } else {
        m_entries.clear();
    }

    if (m_onChangeCallback) m_onChangeCallback();
}

bool ClipboardManager::copyToClipboard(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&uuid](const ClipboardEntry& e) { return e.uuid == uuid; });

    if (it == m_entries.end()) return false;

    // Use wl-copy for Wayland
    std::string cmd;
    if (it->type == EntryType::Text) {
        cmd = "echo -n '" + it->content + "' | wl-copy";
    } else {
        cmd = "wl-copy < '" + it->content + "'";
    }

    return std::system(cmd.c_str()) == 0;
}

bool ClipboardManager::pasteFromClipboard() {
    // Use wtype to simulate Ctrl+V
    return std::system("wtype -M ctrl -k v") == 0;
}

std::string ClipboardManager::getCurrentClipboardContent() const {
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen("wl-paste 2>/dev/null", "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    return result;
}

std::string ClipboardManager::createThumbnail(const std::string& imagePath) const {
    // Create thumbnail using ImageMagick
    std::string thumbPath = m_config.dataDir + "/thumbs/" +
                           std::to_string(std::hash<std::string>{}(imagePath)) + ".png";

    fs::create_directories(m_config.dataDir + "/thumbs");

    std::string cmd = "convert '" + imagePath + "' -thumbnail 48x48 '" + thumbPath + "' 2>/dev/null";
    if (std::system(cmd.c_str()) == 0) {
        return thumbPath;
    }
    return "";
}

void ClipboardManager::startMonitoring() {
    m_monitoring = true;
    // TODO: Implement wl-paste --watch integration
}

void ClipboardManager::stopMonitoring() {
    m_monitoring = false;
}

void ClipboardManager::setOnChangeCallback(std::function<void()> callback) {
    m_onChangeCallback = std::move(callback);
}

void ClipboardManager::loadFromDisk() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(getDataFilePath());
    if (!file.is_open()) return;

    // Simple JSON-like parsing (manual, no external dependency)
    // TODO: Implement proper JSON parsing
    m_entries.clear();
}

void ClipboardManager::saveToDisk() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(getDataFilePath());
    if (!file.is_open()) return;

    // Simple JSON-like output
    file << "[\n";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        file << "  {\n";
        file << "    \"uuid\": \"" << e.uuid << "\",\n";
        file << "    \"type\": \"" << (e.isText() ? "text" : "image") << "\",\n";
        file << "    \"content\": \"" << e.content << "\",\n";
        file << "    \"preview\": \"" << e.preview << "\",\n";
        file << "    \"favorite\": " << (e.favorite ? "true" : "false") << "\n";
        file << "  }" << (i < m_entries.size() - 1 ? "," : "") << "\n";
    }
    file << "]\n";
}

} // namespace hyprclipx
