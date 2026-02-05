// Single Responsibility: Global instance management

#include "hyprclipx/Globals.hpp"
#include "hyprclipx/ClipboardManager.hpp"
#include "hyprclipx/ClipboardRenderer.hpp"
#include "hyprclipx/IPCHandler.hpp"
#include "hyprclipx/ConfigParser.hpp"

namespace hyprclipx {

// Global instances
std::unique_ptr<ClipboardManager> g_clipboardManager;
std::unique_ptr<ClipboardRenderer> g_clipboardRenderer;
std::unique_ptr<IPCHandler> g_ipcHandler;
Config g_config;
void* g_handle = nullptr;

void initGlobals() {
    // Load configuration first
    reloadConfig();

    // Initialize managers in dependency order
    g_clipboardManager = std::make_unique<ClipboardManager>(g_config);
    g_clipboardRenderer = std::make_unique<ClipboardRenderer>(g_config);
    g_ipcHandler = std::make_unique<IPCHandler>();

    // Load persisted clipboard data
    g_clipboardManager->loadFromDisk();

    // Initialize renderer
    g_clipboardRenderer->initialize();

    // Wire up callbacks
    g_clipboardRenderer->setOnItemClick([](const std::string& uuid) {
        if (g_clipboardManager->copyToClipboard(uuid)) {
            g_clipboardRenderer->hide();
            g_clipboardManager->pasteFromClipboard();
        }
    });

    g_clipboardRenderer->setOnFavoriteClick([](const std::string& uuid) {
        g_clipboardManager->toggleFavorite(uuid);
        g_clipboardRenderer->refresh();
    });

    g_clipboardRenderer->setOnOffsetChange([](int dx, int dy) {
        g_config.offsetX += dx;
        g_config.offsetY += dy;
        g_clipboardRenderer->setOffset(g_config.offsetX, g_config.offsetY);
        g_clipboardRenderer->updatePosition();
        // TODO: Save to config file
    });

    g_clipboardRenderer->setOnClose([]() {
        g_clipboardRenderer->hide();
    });

    // Start clipboard monitoring
    g_clipboardManager->startMonitoring();
}

void cleanupGlobals() {
    if (g_clipboardManager) {
        g_clipboardManager->stopMonitoring();
        g_clipboardManager->saveToDisk();
    }

    g_ipcHandler.reset();
    g_clipboardRenderer.reset();
    g_clipboardManager.reset();
}

void reloadConfig() {
    g_config = loadConfig();
}

} // namespace hyprclipx
