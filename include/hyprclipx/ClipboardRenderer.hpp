#pragma once
// 1:1 port of ags-clipboard-manager.template
// GTK4 Layer-Shell window - runs as standalone Wayland client process (NOT in compositor!)

#include "Forward.hpp"
#include "Config.hpp"
#include "ClipboardEntry.hpp"
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

namespace hyprclipx {

class ClipboardRenderer {
public:
    explicit ClipboardRenderer(Config& config, ClipboardManager& manager);
    ~ClipboardRenderer();

    void initialize();  // Create window + UI (call AFTER gtk_init)
    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    int getOffsetX() const { return m_config.offsetX; }
    int getOffsetY() const { return m_config.offsetY; }
    void setOffset(int x, int y);
    void refresh();

private:
    Config& m_config;
    ClipboardManager& m_manager;

    // GTK widgets
    GtkWidget* m_window = nullptr;
    GtkWidget* m_listBox = nullptr;
    GtkWidget* m_searchEntry = nullptr;
    GtkWidget* m_offsetLabel = nullptr;
    GtkWidget* m_scrolled = nullptr;
    GtkWidget* m_tabButtons[4] = {};

    // State (matching AGS createState variables)
    std::string m_filter = "all";
    std::string m_search;
    std::vector<ClipboardEntry> m_items;
    int m_selectedIndex = 0;
    int m_filterIndex = 0;
    std::atomic<bool> m_visible{false};

    // Previous window for focus restore (matching AGS)
    std::string m_previousWindowAddress;

    // UI building (matching AGS widget tree exactly)
    void buildUI();
    GtkWidget* createHeader();
    GtkWidget* createTabs();
    GtkWidget* createSearchBox();
    GtkWidget* createFooter();
    GtkWidget* createOffsetControls();

    // List management (matching AGS updateList/updateSelection)
    void updateList();
    void updateSelection(int newIndex);
    void updateTabs();
    void scrollToIndex(int index);

    // Smart paste (1:1 from AGS pasteItem)
    void pasteItem(const std::string& uuid, const std::string& itemType);

    // Window helpers (matching AGS)
    void repositionWindow();
    void loadCaretOffset();
    void saveCaretOffset();

    // Terminal/browser detection (1:1 from AGS)
    struct WindowInfo {
        std::string windowClass;
        std::string initialClass;
        std::string title;
        std::string initialTitle;
        std::string address;
        int pid = 0;
        bool xwayland = false;
    };
    WindowInfo getActiveWindowInfo();
    bool isTerminal(const WindowInfo& win);
    bool isKittyTerminal(const WindowInfo& win);
    bool isBrowser(const WindowInfo& win);

    // Keyboard handler (matching AGS keyController)
    static gboolean onKeyPress(GtkEventControllerKey* controller,
                               guint keyval, guint keycode,
                               GdkModifierType state, gpointer data);

    // Helpers
    void removeAllChildren(GtkWidget* box);
    std::string exec(const std::string& cmd);

    static constexpr int ITEM_HEIGHT = 54;
    static constexpr int OFFSET_STEP = 20;
};

} // namespace hyprclipx
