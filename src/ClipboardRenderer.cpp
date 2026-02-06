// HyprClipX UI — compact horizontal clipboard manager
// GTK4 Layer-Shell window — standalone Wayland client (NOT in compositor!)

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

// ── Terminal identifiers (1:1 from AGS) ─────────────────────────────────────
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

// ── Filter definitions (SSOT) ───────────────────────────────────────────────
static const std::string FILTER_NAMES[] = {"all", "favorites", "text", "image"};
static const char* FILTER_ICONS[] = {"\xe2\x8a\x9b", "\xe2\x98\x86", "\xf0\x9d\x90\x93", "\xf0\x9f\x96\xbc"};

// ── Browser identifiers (SSOT) ─────────────────────────────────────────────
static const std::vector<std::string> BROWSER_IDENTIFIERS = {
    "firefox", "chrome", "chromium", "brave", "vivaldi",
    "opera", "zen", "floorp", "librewolf", "edge"
};

// ── CSS — compact horizontal layout, HyprZones dark theme ───────────────────
static const char* CLIPBOARD_CSS = R"CSS(
.ClipboardManager { background: transparent; }

.cm-root {
  background: #0a0a0a;
  border: 1px solid #2a2a2a;
}

/* ── Sidebar ── */
.cm-sidebar-header {
  background: #0e0e0e;
  border-right: 1px solid #1a1a1a;
  border-bottom: 1px solid #1a1a1a;
  padding: 2px;
  min-width: 36px;
}

.cm-sidebar-body {
  background: #0e0e0e;
  border-right: 1px solid #1a1a1a;
  padding: 2px;
  min-width: 36px;
}

.cm-sidebar-icon {
  min-width: 32px;
  min-height: 28px;
  padding: 2px;
  border-radius: 0;
  border: 1px solid transparent;
  background: transparent;
  color: #4a5a5a;
  font-size: 13px;
}
.cm-sidebar-icon:hover {
  background: #1a1a1a;
  border-color: #2a3a2a;
  color: #7a9a7a;
}
.cm-sidebar-icon.active {
  background: #1a2a1a;
  border-color: #3a6a3a;
  color: #8aba8a;
}

.cm-sidebar-sep {
  background: #1a1a1a;
  min-height: 1px;
  margin: 3px 4px;
}

.cm-count {
  font-size: 9px;
  font-family: "Fira Code", monospace;
  color: #3a4a4a;
  padding: 2px;
}

/* ── Search ── */
.cm-search {
  background: #111111;
  border-bottom: 1px solid #1a1a1a;
  padding: 4px 8px;
}
.cm-search label {
  color: #3a4a3a;
  font-size: 12px;
}
.cm-search-input {
  background: transparent;
  border: none;
  color: #8a9a9a;
  font-family: "Fira Code", monospace;
  font-size: 11px;
  caret-color: #3a6a3a;
}
.cm-search-input:focus { outline: none; }

/* ── Item list ── */
.cm-scroll { min-height: 120px; }
.cm-list { padding: 2px 4px; }

.cm-item {
  padding: 3px 8px;
  border-radius: 0;
  border: 1px solid transparent;
  background: transparent;
}
.cm-item:hover {
  background: #161616;
  border-color: #1a2a1a;
}
.cm-item.selected {
  background: rgba(42, 90, 42, 0.25);
  border-color: #3a6a3a;
}
.cm-item.selected:hover {
  background: rgba(58, 106, 58, 0.25);
}

.cm-triangle {
  font-size: 9px;
  color: #2a3a3a;
  min-width: 10px;
}
.cm-item.selected .cm-triangle {
  color: #5a9a5a;
}

.cm-preview {
  font-family: "Fira Code", monospace;
  font-size: 11px;
  color: #7a8a8a;
}
.cm-item.selected .cm-preview {
  color: #9aaa9a;
}

.cm-fav-column {
  border-left: 1px solid #1a1a1a;
  min-width: 26px;
  padding: 2px 0;
}

.cm-fav-indicator {
  font-size: 10px;
  color: #2a3a3a;
  min-width: 26px;
  padding: 3px 0;
  border: 1px solid transparent;
  border-radius: 0;
  background: transparent;
}
.cm-fav-indicator:hover {
  background: transparent;
  border-color: transparent;
}
.cm-fav-indicator.starred {
  color: #f9e2af;
}

.cm-img-indicator {
  font-size: 9px;
  color: #3a5a5a;
  margin-right: 4px;
}

/* ── Hint bar ── */
.cm-hints {
  background: #0a0a0a;
  border-top: 1px solid #1a1a1a;
  padding: 2px 8px;
}
.cm-hint {
  font-size: 9px;
  font-family: "Fira Code", monospace;
  color: #2a3a3a;
}

/* ── Offset overlay ── */
.cm-offset-overlay {
  background: rgba(42, 90, 42, 0.85);
  color: #8a9a9a;
  font-size: 9px;
  font-family: "Fira Code", monospace;
  padding: 1px 6px;
  border: 1px solid #3a6a3a;
  margin: 4px;
}
)CSS";

// ── Ctor / Dtor ─────────────────────────────────────────────────────────────

ClipboardRenderer::ClipboardRenderer(Config& config, ClipboardManager& manager)
    : m_config(config), m_manager(manager) {}

ClipboardRenderer::~ClipboardRenderer() {
    if (m_window) { gtk_window_destroy(GTK_WINDOW(m_window)); m_window = nullptr; }
}

// ── Helpers ─────────────────────────────────────────────────────────────────

std::string ClipboardRenderer::exec(const std::string& cmd) {
    std::array<char, 4096> buf;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe)) result += buf.data();
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// ── Initialize ──────────────────────────────────────────────────────────────

void ClipboardRenderer::initialize() {
    loadCaretOffset();

    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, CLIPBOARD_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    m_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(m_window), "HyprClipX");
    gtk_window_set_default_size(GTK_WINDOW(m_window),
                                m_config.windowWidth, m_config.windowHeight);

    gtk_layer_init_for_window(GTK_WINDOW(m_window));
    gtk_layer_set_layer(GTK_WINDOW(m_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(m_window),
                                GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_namespace(GTK_WINDOW(m_window), "clipboard-manager");
    gtk_widget_add_css_class(m_window, "ClipboardManager");

    buildUI();

    g_signal_connect(m_window, "close-request",
        G_CALLBACK(+[](GtkWindow*, gpointer d) -> gboolean {
            auto* s = static_cast<ClipboardRenderer*>(d);
            gtk_widget_set_visible(s->m_window, FALSE);
            s->m_visible = false;
            return TRUE;
        }), this);

    g_signal_connect(m_window, "map",
        G_CALLBACK(+[](GtkWidget*, gpointer d) {
            static_cast<ClipboardRenderer*>(d)->repositionWindow();
        }), this);

    g_signal_connect(m_window, "show",
        G_CALLBACK(+[](GtkWidget*, gpointer d) {
            auto* s = static_cast<ClipboardRenderer*>(d);
            std::ifstream f(s->m_config.prevWindowFile);
            if (f.is_open()) std::getline(f, s->m_previousWindowAddress);
            s->m_selectedIndex = 0;
            s->m_search.clear();
            if (s->m_searchEntry)
                gtk_editable_set_text(GTK_EDITABLE(s->m_searchEntry), "");
            if (s->m_scrolled) {
                auto* vadj = gtk_scrolled_window_get_vadjustment(
                    GTK_SCROLLED_WINDOW(s->m_scrolled));
                if (vadj) gtk_adjustment_set_value(vadj, 0);
            }
            s->updateList();
        }), this);
}

// ── UI Assembly ─────────────────────────────────────────────────────────────
//
//  ╭────┬────────────────────────────────────────────────╮
//  │ 📋 │ 🔍 …                                           │
//  ├────┼────────────────────────────────────────────────┤
//  │ ⊛  │ ▸ item text preview...                      ★  │
//  │ ☆  │   another entry...                          ☆  │
//  │ 𝐓  │   ...                                       ☆  │
//  │ 🖼  │                                                │
//  │    │                                                │
//  │ 🗑  │                                                │
//  │ 7  │                                                │
//  ╰────┴────────────────────────────────────────────────╯
//   ↑↓ ⏎ ⌫ ⇥ ⎋ ⌘⌥↕

void ClipboardRenderer::buildUI() {
    // Root: vertical (header-row | body-row | hints)
    // Split into rows so sidebar buttons align exactly with list items
    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "cm-root");

    // Header row: sidebar-header | search bar (same height)
    GtkWidget* headerRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(headerRow), createSidebarHeader());
    GtkWidget* searchBar = createSearchBar();
    gtk_widget_set_hexpand(searchBar, TRUE);
    gtk_box_append(GTK_BOX(headerRow), searchBar);
    gtk_box_append(GTK_BOX(root), headerRow);

    // Body row: sidebar-body | scrolled list (filter buttons align with items)
    GtkWidget* bodyRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(bodyRow, TRUE);
    gtk_box_append(GTK_BOX(bodyRow), createSidebarBody());

    m_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(m_scrolled, TRUE);
    gtk_widget_set_hexpand(m_scrolled, TRUE);
    gtk_widget_add_css_class(m_scrolled, "cm-scroll");

    // Scroll content: [item-list | fav-column]
    GtkWidget* scrollContent = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    m_listBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_add_css_class(m_listBox, "cm-list");
    gtk_widget_set_hexpand(m_listBox, TRUE);
    gtk_box_append(GTK_BOX(scrollContent), m_listBox);

    m_favBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_add_css_class(m_favBox, "cm-fav-column");
    gtk_widget_set_vexpand(m_favBox, TRUE);
    gtk_box_append(GTK_BOX(scrollContent), m_favBox);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scrolled), scrollContent);
    gtk_box_append(GTK_BOX(bodyRow), m_scrolled);
    gtk_box_append(GTK_BOX(root), bodyRow);

    // Hint bar (full width)
    gtk_box_append(GTK_BOX(root), createHintBar());

    // Overlay for offset indicator
    GtkWidget* overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), root);

    m_offsetLabel = gtk_label_new("");
    gtk_widget_add_css_class(m_offsetLabel, "cm-offset-overlay");
    gtk_widget_set_halign(m_offsetLabel, GTK_ALIGN_END);
    gtk_widget_set_valign(m_offsetLabel, GTK_ALIGN_END);
    gtk_widget_set_visible(m_offsetLabel, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m_offsetLabel);

    gtk_window_set_child(GTK_WINDOW(m_window), overlay);

    // Key handler
    GtkEventController* kc = gtk_event_controller_key_new();
    g_signal_connect(kc, "key-pressed", G_CALLBACK(onKeyPress), this);
    gtk_widget_add_controller(m_window, kc);
}

// ── Sidebar header (logo, shares row with search bar) ───────────────────────

GtkWidget* ClipboardRenderer::createSidebarHeader() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "cm-sidebar-header");

    GtkWidget* logo = gtk_label_new("\xe2\x96\xa0");
    gtk_widget_add_css_class(logo, "cm-sidebar-icon");
    gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(logo, TRUE);
    gtk_box_append(GTK_BOX(box), logo);

    return box;
}

// ── Sidebar body (filter buttons, shares row with list) ─────────────────────

GtkWidget* ClipboardRenderer::createSidebarBody() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(bar, "cm-sidebar-body");

    // Filter icons
    struct FilterData { ClipboardRenderer* self; int idx; };
    for (int i = 0; i < 4; i++) {
        GtkWidget* btn = gtk_button_new_with_label(FILTER_ICONS[i]);
        gtk_widget_set_can_focus(btn, FALSE);
        gtk_widget_add_css_class(btn, "cm-sidebar-icon");
        if (i == 0) gtk_widget_add_css_class(btn, "active");

        auto* fd = new FilterData{this, i};
        g_signal_connect(btn, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer d) {
                auto* fd = static_cast<FilterData*>(d);
                fd->self->m_filterIndex = fd->idx;
                fd->self->m_filter = FILTER_NAMES[fd->idx];
                fd->self->m_selectedIndex = 0;
                fd->self->updateFilterIcons();
                fd->self->updateList();
            }), fd);

        m_filterButtons[i] = btn;
        gtk_box_append(GTK_BOX(bar), btn);
    }

    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bar), spacer);

    // Separator
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(sep, "cm-sidebar-sep");
    gtk_box_append(GTK_BOX(bar), sep);

    // Clear button
    GtkWidget* clearBtn = gtk_button_new_with_label("\xf0\x9f\x97\x91");
    gtk_widget_set_can_focus(clearBtn, FALSE);
    gtk_widget_add_css_class(clearBtn, "cm-sidebar-icon");
    g_signal_connect(clearBtn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            auto* s = static_cast<ClipboardRenderer*>(d);
            s->m_manager.clearAll();
            s->updateList();
        }), this);
    gtk_box_append(GTK_BOX(bar), clearBtn);

    // Item count
    m_countLabel = gtk_label_new("0");
    gtk_widget_add_css_class(m_countLabel, "cm-count");
    gtk_box_append(GTK_BOX(bar), m_countLabel);

    return bar;
}

// ── Search bar ──────────────────────────────────────────────────────────────

GtkWidget* ClipboardRenderer::createSearchBar() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(box, "cm-search");

    GtkWidget* icon = gtk_label_new("\xe2\x97\x8e");
    gtk_box_append(GTK_BOX(box), icon);

    m_searchEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m_searchEntry), "filter search...");
    gtk_widget_set_hexpand(m_searchEntry, TRUE);
    gtk_widget_set_can_focus(m_searchEntry, FALSE);
    gtk_widget_add_css_class(m_searchEntry, "cm-search-input");

    g_signal_connect(m_searchEntry, "changed",
        G_CALLBACK(+[](GtkEditable*, gpointer d) {
            auto* s = static_cast<ClipboardRenderer*>(d);
            const char* t = gtk_editable_get_text(GTK_EDITABLE(s->m_searchEntry));
            s->m_search = t ? t : "";
            s->m_selectedIndex = 0;
            s->updateList();
        }), this);

    gtk_box_append(GTK_BOX(box), m_searchEntry);
    return box;
}

// ── Hint bar (keyboard shortcuts) ───────────────────────────────────────────

GtkWidget* ClipboardRenderer::createHintBar() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(box, "cm-hints");

    const char* hints =
        "\xe2\x86\x91\xe2\x86\x93 nav  \xc2\xb7  "   // ↑↓ nav  ·
        "\xe2\x8f\x8e paste  \xc2\xb7  "               // ⏎ paste  ·
        "\xe2\x8c\xab del  \xc2\xb7  "                 // ⌫ del  ·
        "\xe2\x87\xa5 filter  \xc2\xb7  "              // ⇥ filter  ·
        "\xe2\x8e\x8b close  \xc2\xb7  "               // ⎋ close  ·
        "\xe2\x8c\x98\xe2\x8c\xa5\xe2\x86\x95 move";  // ⌘⌥↕ move

    GtkWidget* label = gtk_label_new(hints);
    gtk_widget_add_css_class(label, "cm-hint");
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_box_append(GTK_BOX(box), label);
    return box;
}

// ── List management ─────────────────────────────────────────────────────────

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
    if (m_favBox) removeAllChildren(m_favBox);

    for (size_t i = 0; i < m_items.size(); i++) {
        const auto& item = m_items[i];

        // Item button (selection highlight only here)
        GtkWidget* btn = gtk_button_new();
        gtk_widget_set_can_focus(btn, FALSE);
        gtk_widget_add_css_class(btn, "cm-item");
        gtk_widget_add_css_class(btn, item.type.c_str());
        if (static_cast<int>(i) == m_selectedIndex)
            gtk_widget_add_css_class(btn, "selected");

        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

        // Selection triangle
        GtkWidget* triangle = gtk_label_new(
            static_cast<int>(i) == m_selectedIndex ? "\xe2\x96\xb8" : " ");
        gtk_widget_add_css_class(triangle, "cm-triangle");
        gtk_box_append(GTK_BOX(hbox), triangle);

        // Image indicator
        if (item.type == "image") {
            GtkWidget* imgIcon = gtk_label_new("\xe2\x96\xa0");
            gtk_widget_add_css_class(imgIcon, "cm-img-indicator");
            gtk_box_append(GTK_BOX(hbox), imgIcon);
        }

        // Preview text
        GtkWidget* preview = gtk_label_new(
            item.preview.empty() ? (item.type == "image" ? item.thumb.c_str() : "[Empty]")
                                 : item.preview.c_str());
        gtk_widget_set_hexpand(preview, TRUE);
        gtk_label_set_xalign(GTK_LABEL(preview), 0);
        gtk_label_set_max_width_chars(GTK_LABEL(preview), 60);
        gtk_label_set_ellipsize(GTK_LABEL(preview), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(preview, "cm-preview");
        gtk_box_append(GTK_BOX(hbox), preview);

        gtk_button_set_child(GTK_BUTTON(btn), hbox);

        // Click → paste
        struct ClickData { ClipboardRenderer* self; std::string uuid; std::string type; };
        auto* cd = new ClickData{this, item.uuid, item.type};
        g_signal_connect(btn, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer d) {
                auto* cd = static_cast<ClickData*>(d);
                cd->self->pasteItem(cd->uuid, cd->type);
            }), cd);

        gtk_box_append(GTK_BOX(m_listBox), btn);

        // Fav star (separate column, same widget type as cm-item = same height)
        GtkWidget* star = gtk_button_new_with_label(
            item.favorite ? "\xe2\x98\x85" : "\xe2\x98\x86");
        gtk_widget_set_can_focus(star, FALSE);
        gtk_widget_add_css_class(star, "cm-fav-indicator");
        if (item.favorite) gtk_widget_add_css_class(star, "starred");
        gtk_box_append(GTK_BOX(m_favBox), star);
    }

    // Update count
    if (m_countLabel) {
        gtk_label_set_text(GTK_LABEL(m_countLabel),
                           std::to_string(m_items.size()).c_str());
    }
}

void ClipboardRenderer::updateSelection(int newIndex) {
    if (newIndex == m_selectedIndex || !m_listBox) return;

    int idx = 0;
    GtkWidget* btn = gtk_widget_get_first_child(m_listBox);
    while (btn) {
        if (idx == m_selectedIndex || idx == newIndex) {
            if (idx == m_selectedIndex) gtk_widget_remove_css_class(btn, "selected");
            if (idx == newIndex)        gtk_widget_add_css_class(btn, "selected");
            // Update triangle indicator
            GtkWidget* hbox = gtk_button_get_child(GTK_BUTTON(btn));
            if (hbox) {
                GtkWidget* tri = gtk_widget_get_first_child(hbox);
                if (tri && GTK_IS_LABEL(tri))
                    gtk_label_set_text(GTK_LABEL(tri),
                        idx == newIndex ? "\xe2\x96\xb8" : " ");
            }
        }
        btn = gtk_widget_get_next_sibling(btn);
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
    double page = gtk_adjustment_get_page_size(vadj);
    double top = index * ITEM_HEIGHT;
    double bot = top + ITEM_HEIGHT;
    double cur = gtk_adjustment_get_value(vadj);
    if (bot > cur + page) gtk_adjustment_set_value(vadj, bot - page);
    else if (top < cur)   gtk_adjustment_set_value(vadj, top);
}

void ClipboardRenderer::updateFilterIcons() {
    for (int i = 0; i < 4; i++) {
        if (i == m_filterIndex) gtk_widget_add_css_class(m_filterButtons[i], "active");
        else                    gtk_widget_remove_css_class(m_filterButtons[i], "active");
    }
}

void ClipboardRenderer::updateOffsetOverlay() {
    if (!m_offsetLabel) return;
    if (m_config.offsetX == 0 && m_config.offsetY == 0) {
        gtk_widget_set_visible(m_offsetLabel, FALSE);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d,%d", m_config.offsetX, m_config.offsetY);
        gtk_label_set_text(GTK_LABEL(m_offsetLabel), buf);
        gtk_widget_set_visible(m_offsetLabel, TRUE);
    }
}

// ── Keyboard handler ────────────────────────────────────────────────────────

gboolean ClipboardRenderer::onKeyPress(GtkEventControllerKey*,
                                        guint keyval, guint,
                                        GdkModifierType state, gpointer data) {
    auto* self = static_cast<ClipboardRenderer*>(data);
    int count = static_cast<int>(self->m_items.size());

    // Super+Alt+Arrow: move window offset
    if ((state & GDK_SUPER_MASK) && (state & GDK_ALT_MASK)) {
        int dx = 0, dy = 0;
        if      (keyval == GDK_KEY_Left)  dx = -OFFSET_STEP;
        else if (keyval == GDK_KEY_Right) dx =  OFFSET_STEP;
        else if (keyval == GDK_KEY_Up)    dy = -OFFSET_STEP;
        else if (keyval == GDK_KEY_Down)  dy =  OFFSET_STEP;
        else if (keyval == GDK_KEY_r || keyval == GDK_KEY_R) {
            self->m_config.offsetX = 0;
            self->m_config.offsetY = 0;
            self->saveCaretOffset();
            self->updateOffsetOverlay();
            self->repositionWindow();
            return TRUE;
        }
        if (dx || dy) {
            self->m_config.offsetX += dx;
            self->m_config.offsetY += dy;
            self->saveCaretOffset();
            self->updateOffsetOverlay();
            self->repositionWindow();
            return TRUE;
        }
    }

    // Ctrl+F: toggle favorite
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_f || keyval == GDK_KEY_F)) {
        if (!self->m_items.empty() && self->m_selectedIndex < count) {
            self->m_manager.toggleFavorite(self->m_items[self->m_selectedIndex].uuid);
            self->updateList();
        }
        return TRUE;
    }

    if (keyval == GDK_KEY_Escape) {
        gtk_widget_set_visible(self->m_window, FALSE);
        self->m_visible = false;
        return TRUE;
    }
    if (keyval == GDK_KEY_Down) {
        self->updateSelection(std::min(self->m_selectedIndex + 1, count - 1));
        return TRUE;
    }
    if (keyval == GDK_KEY_Up) {
        self->updateSelection(std::max(self->m_selectedIndex - 1, 0));
        return TRUE;
    }
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (!self->m_items.empty() && self->m_selectedIndex < count)
            self->pasteItem(self->m_items[self->m_selectedIndex].uuid,
                            self->m_items[self->m_selectedIndex].type);
        return TRUE;
    }
    if (keyval == GDK_KEY_Delete) {
        if (!self->m_items.empty() && self->m_selectedIndex < count) {
            self->m_manager.deleteItem(self->m_items[self->m_selectedIndex].uuid);
            self->updateList();
            if (self->m_selectedIndex >= static_cast<int>(self->m_items.size()))
                self->m_selectedIndex = std::max(0, static_cast<int>(self->m_items.size()) - 1);
        }
        return TRUE;
    }
    if (keyval == GDK_KEY_Home) {
        self->updateSelection(0);
        return TRUE;
    }
    if (keyval == GDK_KEY_End) {
        self->updateSelection(std::max(0, count - 1));
        return TRUE;
    }
    if (keyval == GDK_KEY_Tab) {
        int n = (self->m_filterIndex + 1) % 4;
        self->m_filterIndex = n;
        self->m_filter = FILTER_NAMES[n];
        self->m_selectedIndex = 0;
        self->updateFilterIcons();
        self->updateList();
        return TRUE;
    }
    if (keyval == GDK_KEY_ISO_Left_Tab) {
        int n = (self->m_filterIndex + 3) % 4;
        self->m_filterIndex = n;
        self->m_filter = FILTER_NAMES[n];
        self->m_selectedIndex = 0;
        self->updateFilterIcons();
        self->updateList();
        return TRUE;
    }
    if (keyval == GDK_KEY_BackSpace) {
        const char* t = gtk_editable_get_text(GTK_EDITABLE(self->m_searchEntry));
        std::string cur = t ? t : "";
        if (!cur.empty()) { cur.pop_back(); gtk_editable_set_text(GTK_EDITABLE(self->m_searchEntry), cur.c_str()); }
        return TRUE;
    }
    guint32 ch = gdk_keyval_to_unicode(keyval);
    if (ch > 31 && ch < 127) {
        const char* t = gtk_editable_get_text(GTK_EDITABLE(self->m_searchEntry));
        std::string cur = t ? t : "";
        cur += static_cast<char>(ch);
        gtk_editable_set_text(GTK_EDITABLE(self->m_searchEntry), cur.c_str());
        return TRUE;
    }
    return FALSE;
}

// ── Smart paste (1:1 from AGS) ──────────────────────────────────────────────

void ClipboardRenderer::pasteItem(const std::string& uuid, const std::string& itemType) {
    gtk_widget_set_visible(m_window, FALSE);
    m_visible = false;

    std::string prevAddr = m_previousWindowAddress;
    ClipboardManager* mgr = &m_manager;

    std::thread([this, uuid, itemType, prevAddr, mgr]() {
        if (!prevAddr.empty())
            exec("hyprctl dispatch focuswindow address:" + prevAddr);

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        WindowInfo win = getActiveWindowInfo();
        mgr->paste(uuid);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        bool xw = win.xwayland;

        if (isKittyTerminal(win) && itemType == "text") {
            if (system("kitty @ send-text --from-clipboard 2>/dev/null") == 0) return;
        }

        if (isTerminal(win) && itemType == "text") {
            exec(xw ? "xdotool key --clearmodifiers ctrl+shift+v"
                     : "wtype -d 20 -M ctrl -M shift -k v");
        } else if (isBrowser(win)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            exec(xw ? "xdotool key --clearmodifiers ctrl+v"
                     : "wtype -d 25 -M ctrl -k v");
        } else {
            exec(xw ? "xdotool key --clearmodifiers ctrl+v"
                     : "wtype -d 15 -M ctrl -k v");
        }
    }).detach();
}

// ── Window detection (1:1 from AGS) ─────────────────────────────────────────

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
            while (pos < json.size() && json[pos] != '"') val += json[pos++];
            return val;
        }
        std::string val;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') val += json[pos++];
        return val;
    };

    info.windowClass  = extract("class");
    info.initialClass = extract("initialClass");
    info.title        = extract("title");
    info.initialTitle = extract("initialTitle");
    info.address      = extract("address");
    info.xwayland     = extract("xwayland") == "true";
    std::string pid   = extract("pid");
    if (!pid.empty()) info.pid = std::atoi(pid.c_str());
    return info;
}

bool ClipboardRenderer::isKittyTerminal(const WindowInfo& win) {
    if (toLower(win.initialTitle) == "kitty") return true;
    if (win.pid > 0) {
        if (toLower(exec("cat /proc/" + std::to_string(win.pid) + "/comm 2>/dev/null")) == "kitty")
            return true;
    }
    return false;
}

bool ClipboardRenderer::isTerminal(const WindowInfo& win) {
    std::vector<std::string> fields = {
        toLower(win.windowClass), toLower(win.initialClass),
        toLower(win.title), toLower(win.initialTitle)
    };
    for (const auto& f : fields) {
        if (f.empty()) continue;
        for (const auto& t : TERMINAL_IDENTIFIERS)
            if (f.find(t) != std::string::npos) return true;
    }
    if (win.pid > 0) {
        std::string comm = toLower(exec("cat /proc/" + std::to_string(win.pid) + "/comm 2>/dev/null"));
        for (const auto& t : TERMINAL_IDENTIFIERS)
            if (comm.find(t) != std::string::npos) return true;
    }
    return false;
}

bool ClipboardRenderer::isBrowser(const WindowInfo& win) {
    std::string cls = toLower(win.windowClass);
    for (const auto& b : BROWSER_IDENTIFIERS)
        if (cls.find(b) != std::string::npos) return true;
    return false;
}

// ── Positioning ─────────────────────────────────────────────────────────────

void ClipboardRenderer::repositionWindow() {
    if (!m_window) return;

    // Parse caret position file: caretX,caretY[,monX,monY,monW,monH]
    int cx = 400, cy = 400;
    int monX = 0, monY = 0, monW = 0, monH = 0;
    {
        std::ifstream f(m_config.caretPosFile);
        if (f.is_open()) {
            char c;
            f >> cx >> c >> cy;
            if (f.peek() == ',') {
                f >> c >> monX >> c >> monY >> c >> monW >> c >> monH;
            }
        }
    }

    // If no monitor bounds from plugin, get them from GTK display
    if (monW <= 0 || monH <= 0) {
        GdkDisplay* display = gdk_display_get_default();
        if (display) {
            GListModel* monitors = gdk_display_get_monitors(display);
            guint n = g_list_model_get_n_items(monitors);
            for (guint i = 0; i < n; i++) {
                auto* mon = GDK_MONITOR(g_list_model_get_item(monitors, i));
                GdkRectangle geo;
                gdk_monitor_get_geometry(mon, &geo);
                // Find the monitor that contains (or is nearest to) the caret
                if (cx >= geo.x && cx < geo.x + geo.width &&
                    cy >= geo.y && cy < geo.y + geo.height) {
                    monX = geo.x; monY = geo.y;
                    monW = geo.width; monH = geo.height;
                    g_object_unref(mon);
                    break;
                }
                // Track the first monitor as fallback
                if (i == 0) {
                    monX = geo.x; monY = geo.y;
                    monW = geo.width; monH = geo.height;
                }
                g_object_unref(mon);
            }
        }
    }

    // Clamp caret to monitor bounds before applying offset
    if (monW > 0 && monH > 0) {
        cx = std::max(cx, monX);
        cx = std::min(cx, monX + monW - 1);
        cy = std::max(cy, monY);
        cy = std::min(cy, monY + monH - 1);
    }

    int x = cx - m_config.windowWidth / 2 + m_config.offsetX;
    int y = cy - m_config.windowHeight / 2 + m_config.offsetY;

    // Clamp final window position to monitor bounds
    if (monW > 0 && monH > 0) {
        x = std::max(x, monX);
        y = std::max(y, monY);
        x = std::min(x, monX + monW - m_config.windowWidth);
        y = std::min(y, monY + monH - m_config.windowHeight);
    }

    // Layer-shell margins are relative to the monitor origin, not absolute
    gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_LEFT, x - monX);
    gtk_layer_set_margin(GTK_WINDOW(m_window), GTK_LAYER_SHELL_EDGE_TOP, y - monY);
}

// ── Offset persistence ──────────────────────────────────────────────────────

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
        if (f.is_open())
            content.assign((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    }
    size_t cmPos = content.find("\"clipboard_manager\"");
    if (cmPos != std::string::npos) {
        auto replaceInt = [&](const std::string& key, int value) {
            size_t pos = content.find("\"" + key + "\"", cmPos);
            if (pos == std::string::npos) return;
            size_t col = content.find(':', pos);
            if (col == std::string::npos) return;
            size_t vs = col + 1;
            while (vs < content.size() && content[vs] == ' ') vs++;
            size_t ve = vs;
            if (ve < content.size() && content[ve] == '-') ve++;
            while (ve < content.size() && content[ve] >= '0' && content[ve] <= '9') ve++;
            content.replace(vs, ve - vs, std::to_string(value));
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
            size_t lb = content.rfind('}');
            if (lb != std::string::npos) {
                content.insert(lb, ",\n  \"clipboard_manager\": {\n"
                    "    \"caret_offset_x\": " + std::to_string(m_config.offsetX) + ",\n"
                    "    \"caret_offset_y\": " + std::to_string(m_config.offsetY) + "\n"
                    "  }\n");
            }
        }
    }
    std::ofstream f(m_config.userSettingsFile);
    if (f.is_open()) f << content;
}

// ── Public API ──────────────────────────────────────────────────────────────

void ClipboardRenderer::show() {
    if (m_window && !m_visible) { gtk_window_present(GTK_WINDOW(m_window)); m_visible = true; }
}

void ClipboardRenderer::hide() {
    if (m_window && m_visible) { gtk_widget_set_visible(m_window, FALSE); m_visible = false; }
}

void ClipboardRenderer::toggle() {
    if (!m_window) return;
    if (m_visible) hide(); else show();
}

bool ClipboardRenderer::isVisible() const { return m_visible; }

void ClipboardRenderer::setOffset(int x, int y) {
    m_config.offsetX = x;
    m_config.offsetY = y;
    saveCaretOffset();
    updateOffsetOverlay();
    repositionWindow();
}

void ClipboardRenderer::refresh() { updateList(); }

} // namespace hyprclipx
