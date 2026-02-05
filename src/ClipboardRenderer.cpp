// Single Responsibility: GTK4 Layer-shell window rendering

#include "hyprclipx/ClipboardRenderer.hpp"
#include "hyprclipx/ClipboardEntry.hpp"
#include <cstdlib>
#include <cstdio>
#include <array>

namespace hyprclipx {

ClipboardRenderer::ClipboardRenderer(const Config& config)
    : m_config(config)
    , m_offsetX(config.offsetX)
    , m_offsetY(config.offsetY) {
}

ClipboardRenderer::~ClipboardRenderer() {
    if (m_window) {
        gtk_window_destroy(GTK_WINDOW(m_window));
    }
}

void ClipboardRenderer::initialize() {
    // Initialize GTK
    gtk_init();

    // Create layer-shell window
    m_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(m_window), "HyprClipX");
    gtk_window_set_default_size(GTK_WINDOW(m_window),
                                m_config.windowWidth,
                                m_config.windowHeight);

    // Configure layer-shell
    gtk_layer_init_for_window(GTK_WINDOW(m_window));
    gtk_layer_set_layer(GTK_WINDOW(m_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(m_window),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    // Anchor to top-left for absolute positioning via margins
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);

    // Build UI
    buildUI();

    // Connect close handler
    g_signal_connect(m_window, "close-request",
                     G_CALLBACK(+[](GtkWindow*, gpointer data) -> gboolean {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         self->hide();
                         return TRUE;  // Prevent destruction
                     }), this);
}

void ClipboardRenderer::buildUI() {
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(mainBox, "cm-container");
    gtk_widget_set_margin_start(mainBox, 12);
    gtk_widget_set_margin_end(mainBox, 12);
    gtk_widget_set_margin_top(mainBox, 12);
    gtk_widget_set_margin_bottom(mainBox, 12);

    // Header
    gtk_box_append(GTK_BOX(mainBox), createHeader());

    // Search box
    gtk_box_append(GTK_BOX(mainBox), createSearchBox());

    // Item list (scrollable)
    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);

    m_listBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), m_listBox);
    gtk_box_append(GTK_BOX(mainBox), scrolled);

    // Footer with offset controls
    gtk_box_append(GTK_BOX(mainBox), createFooter());

    gtk_window_set_child(GTK_WINDOW(m_window), mainBox);

    // Key controller for ESC
    GtkEventController* keyController = gtk_event_controller_key_new();
    g_signal_connect(keyController, "key-pressed",
                     G_CALLBACK(onKeyPress), this);
    gtk_widget_add_controller(m_window, keyController);
}

GtkWidget* ClipboardRenderer::createHeader() {
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header, "cm-header");

    GtkWidget* icon = gtk_label_new("");  // Clipboard icon
    gtk_widget_add_css_class(icon, "cm-icon");

    GtkWidget* title = gtk_label_new("Clipboard Manager");
    gtk_widget_add_css_class(title, "cm-title");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);

    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(header), title);

    return header;
}

GtkWidget* ClipboardRenderer::createSearchBox() {
    GtkWidget* searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(searchBox, "cm-search");

    GtkWidget* searchIcon = gtk_label_new("");  // Search icon

    m_searchEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_searchEntry), "Type to search...");
    gtk_widget_set_hexpand(m_searchEntry, TRUE);
    gtk_widget_add_css_class(m_searchEntry, "cm-search-input");

    g_signal_connect(m_searchEntry, "changed",
                     G_CALLBACK(onSearchChanged), this);

    gtk_box_append(GTK_BOX(searchBox), searchIcon);
    gtk_box_append(GTK_BOX(searchBox), m_searchEntry);

    return searchBox;
}

GtkWidget* ClipboardRenderer::createFooter() {
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer, "cm-footer");

    // Clear button
    GtkWidget* clearBtn = gtk_button_new_with_label("Clear All");
    gtk_widget_add_css_class(clearBtn, "cm-clear-btn");
    g_signal_connect(clearBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer) {
                         // TODO: Implement clear callback
                     }), this);

    // Offset controls
    GtkWidget* offsetControls = createOffsetControls();

    gtk_box_append(GTK_BOX(footer), clearBtn);
    gtk_box_append(GTK_BOX(footer), offsetControls);

    return footer;
}

GtkWidget* ClipboardRenderer::createOffsetControls() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(box, "cm-offset-controls");

    // Left button
    GtkWidget* leftBtn = gtk_button_new_with_label("◀");
    gtk_widget_add_css_class(leftBtn, "cm-offset-btn");
    g_signal_connect(leftBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         if (self->m_onOffsetChange) self->m_onOffsetChange(-20, 0);
                     }), this);

    // Up button
    GtkWidget* upBtn = gtk_button_new_with_label("▲");
    gtk_widget_add_css_class(upBtn, "cm-offset-btn");
    g_signal_connect(upBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         if (self->m_onOffsetChange) self->m_onOffsetChange(0, -20);
                     }), this);

    // Offset label
    m_offsetLabel = gtk_label_new("");
    gtk_widget_add_css_class(m_offsetLabel, "cm-offset-label");
    updateOffsetLabel();

    // Down button
    GtkWidget* downBtn = gtk_button_new_with_label("▼");
    gtk_widget_add_css_class(downBtn, "cm-offset-btn");
    g_signal_connect(downBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         if (self->m_onOffsetChange) self->m_onOffsetChange(0, 20);
                     }), this);

    // Right button
    GtkWidget* rightBtn = gtk_button_new_with_label("▶");
    gtk_widget_add_css_class(rightBtn, "cm-offset-btn");
    g_signal_connect(rightBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         if (self->m_onOffsetChange) self->m_onOffsetChange(20, 0);
                     }), this);

    // Reset button
    GtkWidget* resetBtn = gtk_button_new_with_label("⟲");
    gtk_widget_add_css_class(resetBtn, "cm-offset-btn");
    g_signal_connect(resetBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         self->m_offsetX = 0;
                         self->m_offsetY = 0;
                         self->updateOffsetLabel();
                         if (self->m_onOffsetChange) self->m_onOffsetChange(0, 0);
                     }), this);

    gtk_box_append(GTK_BOX(box), leftBtn);
    gtk_box_append(GTK_BOX(box), upBtn);
    gtk_box_append(GTK_BOX(box), m_offsetLabel);
    gtk_box_append(GTK_BOX(box), downBtn);
    gtk_box_append(GTK_BOX(box), rightBtn);
    gtk_box_append(GTK_BOX(box), resetBtn);

    return box;
}

void ClipboardRenderer::updateOffsetLabel() {
    if (m_offsetLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d,%d", m_offsetX, m_offsetY);
        gtk_label_set_text(GTK_LABEL(m_offsetLabel), buf);
    }
}

void ClipboardRenderer::show() {
    if (m_window && !m_visible) {
        updatePosition();
        gtk_window_present(GTK_WINDOW(m_window));
        m_visible = true;
    }
}

void ClipboardRenderer::hide() {
    if (m_window && m_visible) {
        gtk_widget_set_visible(m_window, FALSE);
        m_visible = false;
        if (m_onClose) m_onClose();
    }
}

void ClipboardRenderer::toggle() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

bool ClipboardRenderer::isVisible() const {
    return m_visible;
}

void ClipboardRenderer::setPosition(int x, int y) {
    if (m_window) {
        gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, x);
        gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, y);
    }
}

void ClipboardRenderer::updatePosition() {
    auto [caretX, caretY] = getCaretPosition();

    int x = caretX - m_config.windowWidth / 2 + m_offsetX;
    int y = caretY - m_config.windowHeight / 2 + m_offsetY;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    setPosition(x, y);
}

std::pair<int, int> ClipboardRenderer::getCaretPosition() const {
    // Try to read caret position from temp file (set by external script)
    FILE* f = fopen("/tmp/hyprclipx-caret-pos", "r");
    if (f) {
        int x = 0, y = 0;
        if (fscanf(f, "%d,%d", &x, &y) == 2) {
            fclose(f);
            return {x, y};
        }
        fclose(f);
    }

    // Fallback: get mouse cursor position via hyprctl
    std::array<char, 128> buffer;
    FILE* pipe = popen("hyprctl cursorpos -j 2>/dev/null", "r");
    if (pipe) {
        std::string result;
        while (fgets(buffer.data(), buffer.size(), pipe)) {
            result += buffer.data();
        }
        pclose(pipe);

        // Simple JSON parsing for {"x":123,"y":456}
        size_t xPos = result.find("\"x\":");
        size_t yPos = result.find("\"y\":");
        if (xPos != std::string::npos && yPos != std::string::npos) {
            int x = std::atoi(result.c_str() + xPos + 4);
            int y = std::atoi(result.c_str() + yPos + 4);
            return {x, y};
        }
    }

    return {400, 400};  // Default fallback
}

void ClipboardRenderer::setOffset(int x, int y) {
    m_offsetX = x;
    m_offsetY = y;
    updateOffsetLabel();
}

void ClipboardRenderer::refresh() {
    // TODO: Refresh item list from ClipboardManager
}

void ClipboardRenderer::setItems(const std::vector<ClipboardEntry>& items) {
    if (!m_listBox) return;

    // Clear existing items
    GtkWidget* child = gtk_widget_get_first_child(m_listBox);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(m_listBox), child);
        child = next;
    }

    // Add new items
    for (const auto& item : items) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row, "cm-item");

        GtkWidget* preview = gtk_label_new(item.preview.c_str());
        gtk_widget_set_hexpand(preview, TRUE);
        gtk_label_set_xalign(GTK_LABEL(preview), 0);
        gtk_label_set_ellipsize(GTK_LABEL(preview), PANGO_ELLIPSIZE_END);

        GtkWidget* favBtn = gtk_button_new_with_label(item.favorite ? "★" : "☆");
        gtk_widget_add_css_class(favBtn, "cm-fav-btn");
        if (item.favorite) gtk_widget_add_css_class(favBtn, "active");

        // Store UUID in widget for callbacks
        g_object_set_data_full(G_OBJECT(row), "uuid",
                               g_strdup(item.uuid.c_str()), g_free);

        gtk_box_append(GTK_BOX(row), preview);
        gtk_box_append(GTK_BOX(row), favBtn);
        gtk_box_append(GTK_BOX(m_listBox), row);
    }
}

void ClipboardRenderer::setSelectedIndex(int index) {
    m_selectedIndex = index;
    // TODO: Update visual selection
}

gboolean ClipboardRenderer::onKeyPress(GtkEventControllerKey* /*controller*/,
                                        guint keyval, guint /*keycode*/,
                                        GdkModifierType /*state*/, gpointer data) {
    auto* self = static_cast<ClipboardRenderer*>(data);

    if (keyval == GDK_KEY_Escape) {
        self->hide();
        return TRUE;
    }

    return FALSE;
}

void ClipboardRenderer::onSearchChanged(GtkEditable* /*editable*/, gpointer data) {
    auto* self = static_cast<ClipboardRenderer*>(data);
    self->refresh();
}

void ClipboardRenderer::setOnItemClick(std::function<void(const std::string&)> callback) {
    m_onItemClick = std::move(callback);
}

void ClipboardRenderer::setOnFavoriteClick(std::function<void(const std::string&)> callback) {
    m_onFavoriteClick = std::move(callback);
}

void ClipboardRenderer::setOnOffsetChange(std::function<void(int, int)> callback) {
    m_onOffsetChange = std::move(callback);
}

void ClipboardRenderer::setOnClose(std::function<void()> callback) {
    m_onClose = std::move(callback);
}

} // namespace hyprclipx
