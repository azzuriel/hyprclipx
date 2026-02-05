#pragma once
// HyprClipX UI — compact horizontal clipboard manager
// GTK4 Layer-Shell window — standalone Wayland client (NOT in compositor!)

#include "Forward.hpp"
#include "Config.hpp"
#include "ClipboardEntry.hpp"
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <string>
#include <vector>
#include <atomic>

namespace hyprclipx {

class ClipboardRenderer {
public:
    explicit ClipboardRenderer(Config& config, ClipboardManager& manager);
    ~ClipboardRenderer();

    void initialize();
    void show();
    void hide();
    void toggle();
    bool isVisible() const;
    void setOffset(int x, int y);
    void refresh();

private:
    Config& m_config;
    ClipboardManager& m_manager;

    // GTK widgets
    GtkWidget* m_window       = nullptr;
    GtkWidget* m_listBox      = nullptr;
    GtkWidget* m_favBox       = nullptr;
    GtkWidget* m_searchEntry  = nullptr;
    GtkWidget* m_scrolled     = nullptr;
    GtkWidget* m_offsetLabel  = nullptr;
    GtkWidget* m_countLabel   = nullptr;
    GtkWidget* m_filterButtons[4] = {};

    // State
    std::string m_filter = "all";
    std::string m_search;
    std::vector<ClipboardEntry> m_items;
    int m_selectedIndex = 0;
    int m_filterIndex   = 0;
    std::atomic<bool> m_visible{false};
    std::string m_previousWindowAddress;

    // UI assembly
    void buildUI();
    GtkWidget* createSidebarHeader();
    GtkWidget* createSidebarBody();
    GtkWidget* createSearchBar();
    GtkWidget* createHintBar();

    // List management
    void updateList();
    void updateSelection(int newIndex);
    void updateFilterIcons();
    void scrollToIndex(int index);
    void updateOffsetOverlay();

    // Smart paste (1:1 from AGS)
    void pasteItem(const std::string& uuid, const std::string& itemType);

    // Window helpers
    void repositionWindow();
    void loadCaretOffset();
    void saveCaretOffset();

    // Window detection (1:1 from AGS)
    struct WindowInfo {
        std::string windowClass, initialClass, title, initialTitle, address;
        int pid = 0;
        bool xwayland = false;
    };
    WindowInfo getActiveWindowInfo();
    bool isTerminal(const WindowInfo& win);
    bool isKittyTerminal(const WindowInfo& win);
    bool isBrowser(const WindowInfo& win);

    // Keyboard handler
    static gboolean onKeyPress(GtkEventControllerKey*, guint, guint,
                               GdkModifierType, gpointer);

    // Helpers
    void removeAllChildren(GtkWidget* box);
    std::string exec(const std::string& cmd);

    static constexpr int ITEM_HEIGHT  = 28;
    static constexpr int OFFSET_STEP  = 20;
};

} // namespace hyprclipx
