#pragma once
// Single Responsibility: GTK4 Layer-shell window rendering

#include "Forward.hpp"
#include "Config.hpp"
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <functional>
#include <string>

namespace hyprclipx {

class ClipboardRenderer {
public:
    explicit ClipboardRenderer(const Config& config);
    ~ClipboardRenderer();

    // Window lifecycle
    void initialize();
    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    // Positioning (layer-shell margins)
    void setPosition(int x, int y);
    void updatePosition();  // Recalculate from caret + offset

    // Content updates
    void refresh();
    void setItems(const std::vector<ClipboardEntry>& items);
    void setSelectedIndex(int index);

    // Callbacks
    void setOnItemClick(std::function<void(const std::string& uuid)> callback);
    void setOnFavoriteClick(std::function<void(const std::string& uuid)> callback);
    void setOnOffsetChange(std::function<void(int dx, int dy)> callback);
    void setOnClose(std::function<void()> callback);

    // Offset controls
    int getOffsetX() const { return m_offsetX; }
    int getOffsetY() const { return m_offsetY; }
    void setOffset(int x, int y);

private:
    const Config& m_config;
    GtkWidget* m_window = nullptr;
    GtkWidget* m_listBox = nullptr;
    GtkWidget* m_searchEntry = nullptr;
    GtkWidget* m_offsetLabel = nullptr;

    int m_offsetX = 0;
    int m_offsetY = 0;
    int m_selectedIndex = 0;
    bool m_visible = false;

    std::function<void(const std::string&)> m_onItemClick;
    std::function<void(const std::string&)> m_onFavoriteClick;
    std::function<void(int, int)> m_onOffsetChange;
    std::function<void()> m_onClose;

    // UI building
    void buildUI();
    GtkWidget* createHeader();
    GtkWidget* createSearchBox();
    GtkWidget* createItemList();
    GtkWidget* createFooter();
    GtkWidget* createOffsetControls();

    // Event handlers
    static gboolean onKeyPress(GtkEventControllerKey* controller,
                               guint keyval, guint keycode,
                               GdkModifierType state, gpointer data);
    static void onSearchChanged(GtkEditable* editable, gpointer data);

    // Helpers
    void updateOffsetLabel();
    std::pair<int, int> getCaretPosition() const;
};

} // namespace hyprclipx
