#pragma once
// Single Responsibility: Clipboard operations (CRUD, wl-copy/wl-paste)

#include "Forward.hpp"
#include "ClipboardEntry.hpp"
#include "Config.hpp"
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace hyprclipx {

class ClipboardManager {
public:
    explicit ClipboardManager(const Config& config);
    ~ClipboardManager();

    // CRUD operations
    std::vector<ClipboardEntry> getItems(const std::string& filter = "all",
                                         const std::string& search = "",
                                         int limit = 50) const;
    bool addItem(const std::string& content, EntryType type);
    bool removeItem(const std::string& uuid);
    bool toggleFavorite(const std::string& uuid);
    void clearAll(bool keepFavorites = true);

    // Clipboard operations
    bool copyToClipboard(const std::string& uuid);
    bool pasteFromClipboard();
    std::string getCurrentClipboardContent() const;

    // Monitoring
    void startMonitoring();
    void stopMonitoring();
    void setOnChangeCallback(std::function<void()> callback);

    // Persistence
    void loadFromDisk();
    void saveToDisk() const;

private:
    const Config& m_config;
    std::vector<ClipboardEntry> m_entries;
    mutable std::mutex m_mutex;
    std::function<void()> m_onChangeCallback;
    bool m_monitoring = false;

    std::string generateUUID() const;
    std::string createThumbnail(const std::string& imagePath) const;
    std::string getDataFilePath() const;
};

} // namespace hyprclipx
