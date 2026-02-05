// 1:1 port of ags-clipboard-manager.template
// GTK4 Layer-Shell window - standalone Wayland client (NOT in compositor!)
// No threads needed: gtk_init() is safe because we're a separate process.

#include "hyprclipx/ClipboardRenderer.hpp"
#include "hyprclipx/ClipboardManager.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <array>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>

namespace hyprclipx {

// Terminal identifiers (1:1 from AGS TERMINAL_IDENTIFIERS)
static const std::vector<std::string> TERMINAL_IDENTIFIERS = {
    "kitty", "alacritty", "foot", "wezterm", "konsole",
    "gnome-terminal", "xterm", "urxvt", "terminator", "tilix",
    "st", "rxvt", "sakura", "terminology", "guake", "tilda",
    "hyper", "tabby", "contour", "cool-retro-term", "claude",
    "jetbrains-pycharm", "jetbrains-clion", "jetbrains-idea",
    "jetbrains-webstorm", "jetbrains-phpstorm", "jetbrains-goland",
    "jetbrains-rider", "jetbrains-rubymine", "jetbrains-datagrip",
    "jetbrains-studio"
};

// Filter names (1:1 from AGS filterNames)
static const std::string FILTER_NAMES[] = {"all", "favorites", "text", "image"};
static const std::string FILTER_LABELS[] = {"All", "\xe2\x98\x86", "Text", "Images"};

// CSS (1:1 from AGS style.scss - HyprZones muted dark theme)
static const char* CLIPBOARD_CSS = R"CSS(
.ClipboardManager {
  background: transparent;
}

.cm-container {
  background: #0a0a0a;
  border: 1px solid #2a2a2a;
  border-radius: 0;
  padding: 12px;
  min-width: 420px;
  min-height: 500px;
}

.cm-header {
  padding-bottom: 8px;
  border-bottom: 1px solid #2a2a2a;
}

.cm-header .cm-icon {
  font-size: 20px;
  color: #3a6a3a;
}

.cm-header .cm-title {
  font-size: 14px;
  font-weight: 600;
  font-family: "Fira Code", monospace;
  color: #8a9a9a;
}

.cm-tabs {
  background: #121212;
  border: 1px solid #1f1f1f;
  padding: 4px;
  border-radius: 0;
}

.cm-tabs .cm-tab {
  padding: 6px 14px;
  border-radius: 0;
  border: 1px solid #2a2a2a;
  color: #6a7a7a;
  background: #1a1a1a;
  font-size: 12px;
  font-family: "Fira Code", monospace;
}

.cm-tabs .cm-tab:hover {
  background: #242424;
  border-color: #3a5a3a;
  color: #8a9a9a;
}

.cm-tabs .cm-tab.active {
  background: #2a5a2a;
  border-color: #3a6a3a;
  color: #8a9a9a;
}

.cm-search {
  background: #1a1a1a;
  border: 1px solid #2a2a2a;
  padding: 6px 10px;
  border-radius: 0;
}

.cm-search label {
  color: #4a5a5a;
}

.cm-search .cm-search-input {
  background: transparent;
  border: none;
  color: #8a9a9a;
  font-family: "Fira Code", monospace;
  font-size: 12px;
  caret-color: #3a6a3a;
}

.cm-search .cm-search-input:focus {
  outline: none;
}

.cm-scroll {
  min-height: 350px;
}

.cm-list {
  padding: 4px;
  padding-bottom: 20px;
}

.cm-item-row {
  margin-bottom: 4px;
}

.cm-item {
  background: #1a1a1a;
  padding: 10px 12px;
  border-radius: 0;
  border: 1px solid #2a2a2a;
}

.cm-item:hover {
  background: #242424;
  border-color: #3a5a3a;
}

.cm-item.selected {
  background: rgba(42, 90, 42, 0.3);
  border: 1px solid #3a6a3a;
}

.cm-item.selected:hover {
  background: rgba(58, 106, 58, 0.3);
}

.cm-item .cm-preview {
  font-family: "Fira Code", monospace;
  font-size: 12px;
  color: #8a9a9a;
}

.cm-item .cm-thumb {
  min-width: 80px;
  min-height: 50px;
  border: 1px solid #2a2a2a;
}

.cm-fav-btn {
  background: #1a1a1a;
  color: #4a5a5a;
  padding: 10px 16px;
  border-radius: 0;
  border: 1px solid #2a2a2a;
  min-width: 44px;
}

.cm-fav-btn:hover {
  background: #242424;
  border-color: #3a5a3a;
  color: #f9e2af;
}

.cm-fav-btn.active {
  color: #f9e2af;
  border-color: rgba(249, 226, 175, 0.3);
}

.cm-fav-btn.active:hover {
  background: rgba(249, 226, 175, 0.15);
}

.cm-footer {
  padding-top: 8px;
  border-top: 1px solid #2a2a2a;
}

.cm-footer .cm-clear-btn {
  background: rgba(138, 74, 74, 0.2);
  color: #cc9999;
  padding: 8px 16px;
  border-radius: 0;
  border: 1px solid rgba(138, 74, 74, 0.5);
  font-size: 12px;
  font-family: "Fira Code", monospace;
}

.cm-footer .cm-clear-btn:hover {
  background: rgba(138, 74, 74, 0.4);
  border-color: rgba(138, 74, 74, 0.7);
  color: #ffaaaa;
}

.cm-footer .cm-offset-btn {
  background: #1a1a1a;
  color: #6a7a7a;
  padding: 8px 16px;
  border-radius: 0;
  border: 1px solid #2a2a2a;
  font-size: 12px;
  font-family: "Fira Code", monospace;
}

.cm-footer .cm-offset-btn:hover {
  background: #242424;
  border-color: #3a5a3a;
  color: #8a9a9a;
}

.cm-offset-label {
  font-size: 11px;
  font-family: "Fira Code", monospace;
  color: #6a7a7a;
}

.cm-offset-controls {
  padding: 0 4px;
}
)CSS";

ClipboardRenderer::ClipboardRenderer(Config& config, ClipboardManager& manager)
    : m_config(config)
    , m_manager(manager) {
}

ClipboardRenderer::~ClipboardRenderer() {
    if (m_window) {
        gtk_window_destroy(GTK_WINDOW(m_window));
        m_window = nullptr;
    }
}

// ============================================================================
// Helper: exec command and return stdout (matching AGS execAsync)
// ============================================================================

std::string ClipboardRenderer::exec(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

// ============================================================================
// Initialization (call AFTER gtk_init)
// ============================================================================

void ClipboardRenderer::initialize() {
    loadCaretOffset();

    // Load CSS (matching AGS style.scss - HyprZones theme)
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(cssProvider, CLIPBOARD_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(cssProvider);

    // Create layer-shell window (matching AGS Astal.Window config)
    m_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(m_window), "Clipboard Manager");
    gtk_window_set_default_size(GTK_WINDOW(m_window),
                                m_config.windowWidth, m_config.windowHeight);

    // Layer-shell (matching AGS: anchor TOP|LEFT, layer TOP, keymode ON_DEMAND)
    gtk_layer_init_for_window(GTK_WINDOW(m_window));
    gtk_layer_set_layer(GTK_WINDOW(m_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(m_window),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_namespace(GTK_WINDOW(m_window), "clipboard-manager");

    // CSS class (matching AGS: win.add_css_class("ClipboardManager"))
    gtk_widget_add_css_class(m_window, "ClipboardManager");

    buildUI();

    // Close-request (matching AGS: win.connect("close-request"))
    g_signal_connect(m_window, "close-request",
                     G_CALLBACK(+[](GtkWindow*, gpointer data) -> gboolean {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         gtk_widget_set_visible(self->m_window, FALSE);
                         self->m_visible = false;
                         return TRUE;
                     }), this);

    // Map signal: reposition window (matching AGS: win.connect("map"))
    g_signal_connect(m_window, "map",
                     G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         self->repositionWindow();
                     }), this);

    // Show signal: reset state and refresh (matching AGS: win.connect("show"))
    g_signal_connect(m_window, "show",
                     G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);

                         // Read previous window address (matching AGS)
                         std::ifstream f(self->m_config.prevWindowFile);
                         if (f.is_open()) {
                             std::getline(f, self->m_previousWindowAddress);
                         }

                         self->m_selectedIndex = 0;
                         self->m_search.clear();
                         if (self->m_searchEntry) {
                             gtk_editable_set_text(GTK_EDITABLE(self->m_searchEntry), "");
                         }
                         if (self->m_scrolled) {
                             GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(
                                 GTK_SCROLLED_WINDOW(self->m_scrolled));
                             if (vadj) gtk_adjustment_set_value(vadj, 0);
                         }
                         self->updateList();
                     }), this);
}

// ============================================================================
// UI Building (matching AGS widget tree exactly)
// ============================================================================

void ClipboardRenderer::buildUI() {
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(mainBox, "cm-container");

    gtk_box_append(GTK_BOX(mainBox), createHeader());
    gtk_box_append(GTK_BOX(mainBox), createTabs());
    gtk_box_append(GTK_BOX(mainBox), createSearchBox());

    // Scrollable item list (matching AGS ScrolledWindow)
    m_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(m_scrolled, TRUE);
    gtk_widget_add_css_class(m_scrolled, "cm-scroll");

    m_listBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(m_listBox, "cm-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled), m_listBox);
    gtk_box_append(GTK_BOX(mainBox), m_scrolled);

    gtk_box_append(GTK_BOX(mainBox), createFooter());
    gtk_window_set_child(GTK_WINDOW(m_window), mainBox);

    // Keyboard handler (matching AGS EventControllerKey)
    GtkEventController* keyController = gtk_event_controller_key_new();
    g_signal_connect(keyController, "key-pressed",
                     G_CALLBACK(onKeyPress), this);
    gtk_widget_add_controller(m_window, keyController);
}

GtkWidget* ClipboardRenderer::createHeader() {
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header, "cm-header");

    GtkWidget* icon = gtk_label_new("\xf0\x9f\x93\x8b");
    gtk_widget_add_css_class(icon, "cm-icon");

    GtkWidget* title = gtk_label_new("Clipboard Manager");
    gtk_widget_add_css_class(title, "cm-title");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);

    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(header), title);
    return header;
}

GtkWidget* ClipboardRenderer::createTabs() {
    GtkWidget* tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(tabs, "cm-tabs");

    struct TabData { ClipboardRenderer* self; int index; };

    for (int i = 0; i < 4; i++) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_set_can_focus(btn, FALSE);
        GtkWidget* label = gtk_label_new(FILTER_LABELS[i].c_str());
        gtk_button_set_child(GTK_BUTTON(btn), label);
        gtk_widget_add_css_class(btn, "cm-tab");
        if (i == 0) gtk_widget_add_css_class(btn, "active");

        auto* td = new TabData{this, i};
        g_signal_connect(btn, "clicked",
                         G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* td = static_cast<TabData*>(data);
                             td->self->m_filterIndex = td->index;
                             td->self->m_filter = FILTER_NAMES[td->index];
                             td->self->m_selectedIndex = 0;
                             td->self->updateTabs();
                             td->self->updateList();
                         }), td);

        m_tabButtons[i] = btn;
        gtk_box_append(GTK_BOX(tabs), btn);
    }
    return tabs;
}

GtkWidget* ClipboardRenderer::createSearchBox() {
    GtkWidget* searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(searchBox, "cm-search");

    GtkWidget* searchIcon = gtk_label_new("\xf0\x9f\x94\x8d");

    m_searchEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_searchEntry), "Type to search...");
    gtk_widget_set_hexpand(m_searchEntry, TRUE);
    gtk_widget_set_can_focus(m_searchEntry, FALSE);
    gtk_widget_add_css_class(m_searchEntry, "cm-search-input");

    g_signal_connect(m_searchEntry, "changed",
                     G_CALLBACK(+[](GtkEditable*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         const char* text = gtk_editable_get_text(GTK_EDITABLE(self->m_searchEntry));
                         self->m_search = text ? text : "";
                         self->m_selectedIndex = 0;
                         self->updateList();
                     }), this);

    gtk_box_append(GTK_BOX(searchBox), searchIcon);
    gtk_box_append(GTK_BOX(searchBox), m_searchEntry);
    return searchBox;
}

GtkWidget* ClipboardRenderer::createFooter() {
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer, "cm-footer");

    GtkWidget* clearBtn = gtk_button_new();
    gtk_widget_set_can_focus(clearBtn, FALSE);
    gtk_widget_add_css_class(clearBtn, "cm-clear-btn");
    GtkWidget* clearBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* trashIcon = gtk_label_new("\xf0\x9f\x97\x91");
    GtkWidget* clearLabel = gtk_label_new("Clear All");
    gtk_box_append(GTK_BOX(clearBox), trashIcon);
    gtk_box_append(GTK_BOX(clearBox), clearLabel);
    gtk_button_set_child(GTK_BUTTON(clearBtn), clearBox);
    g_signal_connect(clearBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         self->m_manager.clearAll();
                         self->updateList();
                     }), this);

    gtk_box_append(GTK_BOX(footer), clearBtn);
    gtk_box_append(GTK_BOX(footer), createOffsetControls());
    return footer;
}

GtkWidget* ClipboardRenderer::createOffsetControls() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(box, "cm-offset-controls");

    m_offsetLabel = gtk_label_new("");
    gtk_widget_add_css_class(m_offsetLabel, "cm-offset-label");
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d,%d", m_config.offsetX, m_config.offsetY);
        gtk_label_set_text(GTK_LABEL(m_offsetLabel), buf);
    }

    auto makeBtn = [&](const char* label, int dx, int dy) -> GtkWidget* {
        GtkWidget* btn = gtk_button_new_with_label(label);
        gtk_widget_set_can_focus(btn, FALSE);
        gtk_widget_add_css_class(btn, "cm-offset-btn");

        struct OffsetData { ClipboardRenderer* self; int dx; int dy; };
        auto* od = new OffsetData{this, dx, dy};
        g_signal_connect(btn, "clicked",
                         G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* od = static_cast<OffsetData*>(data);
                             od->self->m_config.offsetX += od->dx;
                             od->self->m_config.offsetY += od->dy;
                             od->self->saveCaretOffset();
                             char buf[32];
                             snprintf(buf, sizeof(buf), "%d,%d",
                                      od->self->m_config.offsetX,
                                      od->self->m_config.offsetY);
                             gtk_label_set_text(GTK_LABEL(od->self->m_offsetLabel), buf);
                             od->self->repositionWindow();
                         }), od);
        return btn;
    };

    GtkWidget* resetBtn = gtk_button_new_with_label("\xe2\x9f\xb2");
    gtk_widget_set_can_focus(resetBtn, FALSE);
    gtk_widget_add_css_class(resetBtn, "cm-offset-btn");
    g_signal_connect(resetBtn, "clicked",
                     G_CALLBACK(+[](GtkButton*, gpointer data) {
                         auto* self = static_cast<ClipboardRenderer*>(data);
                         self->m_config.offsetX = 0;
                         self->m_config.offsetY = 0;
                         self->saveCaretOffset();
                         gtk_label_set_text(GTK_LABEL(self->m_offsetLabel), "0,0");
                         self->repositionWindow();
                     }), this);

    gtk_box_append(GTK_BOX(box), makeBtn("\xe2\x97\x80", -OFFSET_STEP, 0));
    gtk_box_append(GTK_BOX(box), makeBtn("\xe2\x96\xb2", 0, -OFFSET_STEP));
    gtk_box_append(GTK_BOX(box), m_offsetLabel);
    gtk_box_append(GTK_BOX(box), makeBtn("\xe2\x96\xbc", 0, OFFSET_STEP));
    gtk_box_append(GTK_BOX(box), makeBtn("\xe2\x96\xb6", OFFSET_STEP, 0));
    gtk_box_append(GTK_BOX(box), resetBtn);
    return box;
}

// ============================================================================
// List Management (matching AGS updateList / updateSelection)
// ============================================================================

void ClipboardRenderer::removeAllChildren(GtkWidget* box) {
    GtkWidget* child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

void ClipboardRenderer::updateList() {
    m_items = m_manager.fetchItems(m_filter, m_search, m_config.maxItems);

    if (!m_listBox) return;
    removeAllChildren(m_listBox);

    for (size_t i = 0; i < m_items.size(); i++) {
        const auto& item = m_items[i];

        GtkWidget* itemRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(itemRow, "cm-item-row");

        GtkWidget* contentBtn = gtk_button_new();
        gtk_widget_set_hexpand(contentBtn, TRUE);
        gtk_widget_set_can_focus(contentBtn, FALSE);
        gtk_widget_add_css_class(contentBtn, "cm-item");
        gtk_widget_add_css_class(contentBtn, item.type.c_str());
        if (static_cast<int>(i) == m_selectedIndex) {
            gtk_widget_add_css_class(contentBtn, "selected");
        }

        GtkWidget* innerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

        if (item.type == "image" && !item.thumb.empty()) {
            // Image thumbnail (matching AGS thumbImage)
            GtkWidget* thumbImage = gtk_image_new_from_file(item.thumb.c_str());
            gtk_image_set_pixel_size(GTK_IMAGE(thumbImage), 48);
            gtk_widget_add_css_class(thumbImage, "cm-thumb");
            gtk_box_append(GTK_BOX(innerBox), thumbImage);

            // Image info label (matching AGS: no max_width_chars, fallback "[Image]")
            GtkWidget* infoLabel = gtk_label_new(
                item.preview.empty() ? "[Image]" : item.preview.c_str());
            gtk_widget_set_hexpand(infoLabel, TRUE);
            gtk_label_set_xalign(GTK_LABEL(infoLabel), 0);
            gtk_widget_add_css_class(infoLabel, "cm-preview");
            gtk_box_append(GTK_BOX(innerBox), infoLabel);
        } else {
            // Text preview (matching AGS: max_width_chars 45, ellipsize END)
            GtkWidget* preview = gtk_label_new(
                item.preview.empty() ? "[Empty]" : item.preview.c_str());
            gtk_widget_set_hexpand(preview, TRUE);
            gtk_label_set_xalign(GTK_LABEL(preview), 0);
            gtk_label_set_max_width_chars(GTK_LABEL(preview), 45);
            gtk_label_set_ellipsize(GTK_LABEL(preview), PANGO_ELLIPSIZE_END);
            gtk_widget_add_css_class(preview, "cm-preview");
            gtk_box_append(GTK_BOX(innerBox), preview);
        }

        gtk_button_set_child(GTK_BUTTON(contentBtn), innerBox);

        struct ClickData { ClipboardRenderer* self; std::string uuid; std::string type; };
        auto* cd = new ClickData{this, item.uuid, item.type};
        g_signal_connect(contentBtn, "clicked",
                         G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* cd = static_cast<ClickData*>(data);
                             cd->self->pasteItem(cd->uuid, cd->type);
                         }), cd);

        GtkWidget* favBtn = gtk_button_new();
        gtk_widget_set_valign(favBtn, GTK_ALIGN_FILL);
        gtk_widget_set_can_focus(favBtn, FALSE);
        gtk_widget_add_css_class(favBtn, "cm-fav-btn");
        if (item.favorite) gtk_widget_add_css_class(favBtn, "active");
        GtkWidget* starLabel = gtk_label_new(
            item.favorite ? "\xe2\x98\x85" : "\xe2\x98\x86");
        gtk_button_set_child(GTK_BUTTON(favBtn), starLabel);

        struct FavData { ClipboardRenderer* self; std::string uuid; };
        auto* fd = new FavData{this, item.uuid};
        g_signal_connect(favBtn, "clicked",
                         G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* fd = static_cast<FavData*>(data);
                             fd->self->m_manager.toggleFavorite(fd->uuid);
                             fd->self->updateList();
                         }), fd);

        gtk_box_append(GTK_BOX(itemRow), contentBtn);
        gtk_box_append(GTK_BOX(itemRow), favBtn);
        gtk_box_append(GTK_BOX(m_listBox), itemRow);
    }
}

void ClipboardRenderer::updateSelection(int newIndex) {
    if (newIndex == m_selectedIndex) return;
    if (!m_listBox) return;

    int idx = 0;
    GtkWidget* child = gtk_widget_get_first_child(m_listBox);
    while (child) {
        GtkWidget* contentBtn = gtk_widget_get_first_child(child);
        if (idx == m_selectedIndex && contentBtn) {
            gtk_widget_remove_css_class(contentBtn, "selected");
        }
        if (idx == newIndex && contentBtn) {
            gtk_widget_add_css_class(contentBtn, "selected");
        }
        child = gtk_widget_get_next_sibling(child);
        idx++;
    }

    m_selectedIndex = newIndex;
    scrollToIndex(newIndex);
}

void ClipboardRenderer::scrollToIndex(int index) {
    if (!m_scrolled) return;
    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(m_scrolled));
    if (!vadj) return;

    double scrollHeight = gtk_adjustment_get_page_size(vadj);
    double itemTop = index * ITEM_HEIGHT;
    double itemBottom = itemTop + ITEM_HEIGHT;
    double scrollY = gtk_adjustment_get_value(vadj);

    if (itemBottom > scrollY + scrollHeight) {
        gtk_adjustment_set_value(vadj, itemBottom - scrollHeight);
    } else if (itemTop < scrollY) {
        gtk_adjustment_set_value(vadj, itemTop);
    }
}

void ClipboardRenderer::updateTabs() {
    for (int i = 0; i < 4; i++) {
        if (i == m_filterIndex) {
            gtk_widget_add_css_class(m_tabButtons[i], "active");
        } else {
            gtk_widget_remove_css_class(m_tabButtons[i], "active");
        }
    }
}

// ============================================================================
// Keyboard Handler (1:1 from AGS keyController.connect("key-pressed"))
// ============================================================================

gboolean ClipboardRenderer::onKeyPress(GtkEventControllerKey*,
                                        guint keyval, guint,
                                        GdkModifierType, gpointer data) {
    auto* self = static_cast<ClipboardRenderer*>(data);
    int itemCount = static_cast<int>(self->m_items.size());

    if (keyval == GDK_KEY_Escape) {
        gtk_widget_set_visible(self->m_window, FALSE);
        self->m_visible = false;
        return TRUE;
    }

    if (keyval == GDK_KEY_Down) {
        int newIdx = std::min(self->m_selectedIndex + 1, itemCount - 1);
        self->updateSelection(newIdx);
        return TRUE;
    }

    if (keyval == GDK_KEY_Up) {
        int newIdx = std::max(self->m_selectedIndex - 1, 0);
        self->updateSelection(newIdx);
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (!self->m_items.empty() && self->m_selectedIndex < itemCount) {
            const auto& item = self->m_items[self->m_selectedIndex];
            self->pasteItem(item.uuid, item.type);
        }
        return TRUE;
    }

    if (keyval == GDK_KEY_Home) {
        self->updateSelection(0);
        return TRUE;
    }

    if (keyval == GDK_KEY_End) {
        self->updateSelection(std::max(0, itemCount - 1));
        return TRUE;
    }

    if (keyval == GDK_KEY_Tab) {
        int newIdx = (self->m_filterIndex + 1) % 4;
        self->m_filterIndex = newIdx;
        self->m_filter = FILTER_NAMES[newIdx];
        self->m_selectedIndex = 0;
        self->updateTabs();
        self->updateList();
        return TRUE;
    }

    if (keyval == GDK_KEY_ISO_Left_Tab) {
        int newIdx = (self->m_filterIndex - 1 + 4) % 4;
        self->m_filterIndex = newIdx;
        self->m_filter = FILTER_NAMES[newIdx];
        self->m_selectedIndex = 0;
        self->updateTabs();
        self->updateList();
        return TRUE;
    }

    if (keyval == GDK_KEY_BackSpace) {
        const char* text = gtk_editable_get_text(GTK_EDITABLE(self->m_searchEntry));
        std::string current = text ? text : "";
        if (!current.empty()) {
            current.pop_back();
            gtk_editable_set_text(GTK_EDITABLE(self->m_searchEntry), current.c_str());
        }
        return TRUE;
    }

    guint32 ch = gdk_keyval_to_unicode(keyval);
    if (ch > 31 && ch < 127) {
        const char* text = gtk_editable_get_text(GTK_EDITABLE(self->m_searchEntry));
        std::string current = text ? text : "";
        current += static_cast<char>(ch);
        gtk_editable_set_text(GTK_EDITABLE(self->m_searchEntry), current.c_str());
        return TRUE;
    }

    return FALSE;
}

// ============================================================================
// Smart Paste (1:1 from AGS pasteItem)
// Runs blocking parts in a detached thread to avoid freezing GTK event loop.
// ============================================================================

void ClipboardRenderer::pasteItem(const std::string& uuid, const std::string& itemType) {
    // 1. Hide window FIRST (matching AGS) - GTK call on main thread
    gtk_widget_set_visible(m_window, FALSE);
    m_visible = false;

    // 2. Run the rest in a detached thread (all non-GTK operations)
    std::string prevAddr = m_previousWindowAddress;
    ClipboardManager* mgr = &m_manager;

    std::thread([this, uuid, itemType, prevAddr, mgr]() {
        // Restore focus to previous window (matching AGS)
        if (!prevAddr.empty()) {
            exec("hyprctl dispatch focuswindow address:" + prevAddr);
        }

        // Wait for focus (matching AGS: 150ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Get target window info (matching AGS)
        WindowInfo targetWin = getActiveWindowInfo();

        // Copy to clipboard via daemon (matching AGS)
        mgr->paste(uuid);

        // Wait for clipboard (matching AGS: 200ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Smart paste (matching AGS paste logic exactly)
        bool isXWayland = targetWin.xwayland;

        if (isKittyTerminal(targetWin) && itemType == "text") {
            int ret = system("kitty @ send-text --from-clipboard 2>/dev/null");
            if (ret == 0) return;
            // Fallback to normal terminal paste (matching AGS catch block)
        }

        if (isTerminal(targetWin) && itemType == "text") {
            if (isXWayland) {
                exec("xdotool key --clearmodifiers ctrl+shift+v");
            } else {
                exec("wtype -d 20 -M ctrl -M shift -k v");
            }
        } else if (isBrowser(targetWin)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (isXWayland) {
                exec("xdotool key --clearmodifiers ctrl+v");
            } else {
                exec("wtype -d 25 -M ctrl -k v");
            }
        } else {
            if (isXWayland) {
                exec("xdotool key --clearmodifiers ctrl+v");
            } else {
                exec("wtype -d 15 -M ctrl -k v");
            }
        }
    }).detach();
}

// ============================================================================
// Window Detection (1:1 from AGS)
// ============================================================================

ClipboardRenderer::WindowInfo ClipboardRenderer::getActiveWindowInfo() {
    WindowInfo info;
    std::string json = exec("hyprctl activewindow -j 2>/dev/null");
    if (json.empty()) return info;

    auto extract = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        while (pos < json.size() && json[pos] == ' ') pos++;
        if (pos < json.size() && json[pos] == '"') {
            pos++;
            std::string val;
            while (pos < json.size() && json[pos] != '"') {
                val += json[pos++];
            }
            return val;
        }
        std::string val;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') {
            val += json[pos++];
        }
        return val;
    };

    info.windowClass = extract("class");
    info.initialClass = extract("initialClass");
    info.title = extract("title");
    info.initialTitle = extract("initialTitle");
    info.address = extract("address");
    info.xwayland = extract("xwayland") == "true";

    std::string pidStr = extract("pid");
    if (!pidStr.empty()) info.pid = std::atoi(pidStr.c_str());

    return info;
}

bool ClipboardRenderer::isKittyTerminal(const WindowInfo& win) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    if (lower(win.initialTitle) == "kitty") return true;
    if (win.pid > 0) {
        std::string comm = exec("cat /proc/" + std::to_string(win.pid) + "/comm 2>/dev/null");
        if (lower(comm) == "kitty") return true;
    }
    return false;
}

bool ClipboardRenderer::isTerminal(const WindowInfo& win) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    std::vector<std::string> fields = {
        lower(win.windowClass), lower(win.initialClass),
        lower(win.title), lower(win.initialTitle)
    };
    for (const auto& field : fields) {
        if (field.empty()) continue;
        for (const auto& term : TERMINAL_IDENTIFIERS) {
            if (field.find(term) != std::string::npos) return true;
        }
    }
    if (win.pid > 0) {
        std::string comm = lower(exec("cat /proc/" + std::to_string(win.pid) + "/comm 2>/dev/null"));
        for (const auto& term : TERMINAL_IDENTIFIERS) {
            if (comm.find(term) != std::string::npos) return true;
        }
    }
    return false;
}

bool ClipboardRenderer::isBrowser(const WindowInfo& win) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    std::string cls = lower(win.windowClass);
    return cls.find("firefox") != std::string::npos ||
           cls.find("chrome") != std::string::npos ||
           cls.find("chromium") != std::string::npos ||
           cls.find("brave") != std::string::npos ||
           cls.find("edge") != std::string::npos;
}

// ============================================================================
// Positioning (matching AGS repositionWindow)
// ============================================================================

void ClipboardRenderer::repositionWindow() {
    if (!m_window) return;

    int caretX = 400, caretY = 400;
    std::ifstream f(m_config.caretPosFile);
    if (f.is_open()) {
        char comma;
        f >> caretX >> comma >> caretY;
    }

    int x = caretX - m_config.windowWidth / 2 + m_config.offsetX;
    int y = caretY - m_config.windowHeight / 2 + m_config.offsetY;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, x);
    gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, y);
}

// ============================================================================
// Offset Persistence (matching AGS loadCaretOffsetSync / saveCaretOffset)
// ============================================================================

void ClipboardRenderer::loadCaretOffset() {
    if (m_config.userSettingsFile.empty()) return;
    std::ifstream f(m_config.userSettingsFile);
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    size_t cmPos = content.find("\"clipboard_manager\"");
    if (cmPos == std::string::npos) return;
    auto extractInt = [&](const std::string& key) -> int {
        size_t pos = content.find("\"" + key + "\"", cmPos);
        if (pos == std::string::npos) return 0;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return 0;
        pos++;
        while (pos < content.size() && content[pos] == ' ') pos++;
        return std::atoi(content.c_str() + pos);
    };
    m_config.offsetX = extractInt("caret_offset_x");
    m_config.offsetY = extractInt("caret_offset_y");
}

void ClipboardRenderer::saveCaretOffset() {
    if (m_config.userSettingsFile.empty()) return;
    std::string content;
    {
        std::ifstream f(m_config.userSettingsFile);
        if (f.is_open()) {
            content.assign((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        }
    }
    size_t cmPos = content.find("\"clipboard_manager\"");
    if (cmPos != std::string::npos) {
        auto replaceInt = [&](const std::string& key, int value) {
            size_t pos = content.find("\"" + key + "\"", cmPos);
            if (pos == std::string::npos) return;
            size_t colonPos = content.find(':', pos);
            if (colonPos == std::string::npos) return;
            size_t valStart = colonPos + 1;
            while (valStart < content.size() && content[valStart] == ' ') valStart++;
            size_t valEnd = valStart;
            if (valEnd < content.size() && content[valEnd] == '-') valEnd++;
            while (valEnd < content.size() && content[valEnd] >= '0' && content[valEnd] <= '9') valEnd++;
            content.replace(valStart, valEnd - valStart, std::to_string(value));
        };
        replaceInt("caret_offset_x", m_config.offsetX);
        replaceInt("caret_offset_y", m_config.offsetY);
    } else {
        if (content.empty() || content == "{}") {
            content = "{\n  \"clipboard_manager\": {\n"
                      "    \"caret_offset_x\": " + std::to_string(m_config.offsetX) + ",\n"
                      "    \"caret_offset_y\": " + std::to_string(m_config.offsetY) + "\n"
                      "  }\n}";
        } else {
            size_t lastBrace = content.rfind('}');
            if (lastBrace != std::string::npos) {
                std::string insert = ",\n  \"clipboard_manager\": {\n"
                    "    \"caret_offset_x\": " + std::to_string(m_config.offsetX) + ",\n"
                    "    \"caret_offset_y\": " + std::to_string(m_config.offsetY) + "\n"
                    "  }\n";
                content.insert(lastBrace, insert);
            }
        }
    }
    std::ofstream f(m_config.userSettingsFile);
    if (f.is_open()) f << content;
}

// ============================================================================
// Public API (direct GTK calls - we're on the main thread)
// ============================================================================

void ClipboardRenderer::show() {
    if (m_window && !m_visible) {
        gtk_window_present(GTK_WINDOW(m_window));
        m_visible = true;
    }
}

void ClipboardRenderer::hide() {
    if (m_window && m_visible) {
        gtk_widget_set_visible(m_window, FALSE);
        m_visible = false;
    }
}

void ClipboardRenderer::toggle() {
    if (!m_window) return;
    if (m_visible) {
        gtk_widget_set_visible(m_window, FALSE);
        m_visible = false;
    } else {
        gtk_window_present(GTK_WINDOW(m_window));
        m_visible = true;
    }
}

bool ClipboardRenderer::isVisible() const {
    return m_visible;
}

void ClipboardRenderer::setOffset(int x, int y) {
    m_config.offsetX = x;
    m_config.offsetY = y;
    saveCaretOffset();
    char buf[32];
    snprintf(buf, sizeof(buf), "%d,%d", m_config.offsetX, m_config.offsetY);
    if (m_offsetLabel) gtk_label_set_text(GTK_LABEL(m_offsetLabel), buf);
    repositionWindow();
}

void ClipboardRenderer::refresh() {
    updateList();
}

} // namespace hyprclipx
