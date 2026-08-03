// Global instance management (plugin side - NO GTK!)
// Uses Hyprland internal APIs for window info, fork+exec for UI

#include "hyprclipx/Globals.hpp"
#include "hyprclipx/IPCHandler.hpp"
#include "hyprclipx/ConfigParser.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <format>
#include <unistd.h>
#include <sys/wait.h>

namespace hyprclipx {

std::unique_ptr<IPCHandler> g_ipcHandler;
Config g_config;
void* g_handle = nullptr;

void initGlobals() {
    reloadConfig();

    const char* homeEnv = std::getenv("HOME");
    std::string home = homeEnv ? homeEnv : "/root";
    g_config.clipmanClient = home + "/.local/bin/clipman-client.py";
    g_config.caretHelper = home + "/.local/bin/get-caret-position.py";
    g_config.userSettingsFile = home + "/.config/hyprclipx/settings.json";

    g_ipcHandler = std::make_unique<IPCHandler>();
}

void cleanupGlobals() {
    g_ipcHandler.reset();
}

void reloadConfig() {
    g_config = loadConfig();
}

// ============================================================================
// Caret capture strategy (3-tier):
// 1. text-input-v3: direct struct read in compositor thread (fastest)
//    Works for: Firefox, GTK apps, Qt apps, kitty, foot, alacritty, Electron
// 2. AT-SPI: fork + Python (for XWayland / apps without text-input)
//    Works for: JetBrains, some legacy apps
// 3. Mouse cursor position (last resort)
// ============================================================================

void captureAndSendUI(const std::string& cmd) {
    // Reap zombie children from previous calls
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}

    // Get the keyboard-focused window via SeatManager (not mouse cursor!)
    // This correctly identifies the focused window on multi-monitor setups
    // regardless of where the mouse pointer is
    PHLWINDOW pFocusedWindow;
    {
        auto focusSurface = g_pSeatManager->m_state.keyboardFocus.lock();
        if (focusSurface) {
            // Hyprland 0.56: CCompositor::getWindowFromSurface() was removed.
            // Surface -> view lookup now goes through the view state tracker's
            // query builder. Constraining the query to VIEW_TYPE_WINDOW
            // reproduces the old behaviour exactly: 0.55 resolved the surface's
            // hlSurface->view(), returned nullptr unless its type was
            // VIEW_TYPE_WINDOW, then downcast to CWindow. Without the type
            // constraint the query would fall back to a layer-surface lookup.
            pFocusedWindow = Desktop::viewState()
                                 ->query()
                                 .type(Desktop::View::VIEW_TYPE_WINDOW)
                                 .surface(focusSurface)
                                 .runWindow();
        }
    }

    // Save previous window address BEFORE opening UI (must capture now,
    // because focus changes once clipboard window opens)
    if (pFocusedWindow) {
        std::string windowAddr = std::format("0x{:x}", (uintptr_t)pFocusedWindow.get());
        std::ofstream f(g_config.prevWindowFile);
        if (f.is_open()) f << windowAddr;
    }

    // Capture monitor bounds from the focused window's monitor
    int monX = 0, monY = 0, monW = 1920, monH = 1080;
    {
        PHLMONITOR monitor;
        if (pFocusedWindow)
            monitor = pFocusedWindow->m_monitor.lock();
        if (!monitor) {
            // Hyprland 0.56: CCompositor::getMonitorFromCursor() was removed.
            // 0.55 implemented it as getMonitorFromVector(pointer position), and
            // a vec()-only monitor query resolves through
            // CMonitorQueryCore::closestTo(), which is the same
            // "monitor containing the point, else nearest monitor" logic.
            monitor = State::monitorState()->query().vec(Pointer::mgr()->position()).run();
        }
        if (monitor) {
            monX = static_cast<int>(monitor->m_position.x);
            monY = static_cast<int>(monitor->m_position.y);
            monW = static_cast<int>(monitor->m_size.x);
            monH = static_cast<int>(monitor->m_size.y);
        }
    }

    std::string caretHelper = g_config.caretHelper;
    std::string caretPosFile = g_config.caretPosFile;
    std::string uiArg = "--" + cmd;

    // Format: caretX,caretY,monX,monY,monW,monH
    auto writeCaretFile = [&](int cx, int cy) {
        std::ofstream f(caretPosFile);
        if (f.is_open())
            f << cx << "," << cy << "," << monX << "," << monY << "," << monW << "," << monH;
    };

    // ---- Strategy 1: text-input-v3 (direct compositor read, no fork) ----
    // Apps using text-input-v3 send set_cursor_rectangle automatically.
    // Just a struct read - fast, reliable, no Python, no file-IO roundtrip.
    bool caretCaptured = false;
    {
        // Debug log to /tmp/hyprclipx-debug.log
        std::ofstream dbg("/tmp/hyprclipx-debug.log", std::ios::app);
        dbg << "--- captureAndSendUI ---\n";
        dbg << "pFocusedWindow: " << (pFocusedWindow ? "yes" : "null") << "\n";

        if (pFocusedWindow) {
            dbg << "window class: " << pFocusedWindow->m_class << "\n";
            dbg << "window title: " << pFocusedWindow->m_title << "\n";
            // Hyprland 0.56: CGeometricMovableAnimated::m_realPosition became
            // protected. IGeometric::position() is the public accessor;
            // GEOMETRIC_CURRENT returns m_realPosition->value() verbatim.
            Vector2D winPos = pFocusedWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
            dbg << "window pos: " << winPos.x << "," << winPos.y << "\n";

            CTextInput* ti = g_pInputManager->m_relay.getFocusedTextInput();
            dbg << "getFocusedTextInput: " << (ti ? "yes" : "null") << "\n";

            if (ti) {
                dbg << "  isV3: " << ti->isV3() << "\n";
                dbg << "  isEnabled: " << ti->isEnabled() << "\n";
                dbg << "  hasCursorRectangle: " << ti->hasCursorRectangle() << "\n";
                CBox cb = ti->cursorBox();
                dbg << "  cursorBox: x=" << cb.x << " y=" << cb.y
                    << " w=" << cb.w << " h=" << cb.h << "\n";

                auto surf = ti->focusedSurface();
                dbg << "  focusedSurface: " << (surf ? "yes" : "null") << "\n";

                if (ti->isEnabled() && ti->hasCursorRectangle()) {
                    int cx = static_cast<int>(winPos.x + cb.x);
                    int cy = static_cast<int>(winPos.y + cb.y);
                    dbg << "  global caret: " << cx << "," << cy << "\n";
                    cx = std::clamp(cx, monX, monX + monW - 1);
                    cy = std::clamp(cy, monY, monY + monH - 1);
                    dbg << "  clamped caret: " << cx << "," << cy << "\n";
                    writeCaretFile(cx, cy);
                    caretCaptured = true;
                    dbg << "  STRATEGY 1 SUCCESS\n";
                }
            }
        }
        dbg << "caretCaptured: " << caretCaptured << "\n";
        dbg << std::flush;
    }

    // Fork child: fallback caret strategies + exec UI
    // Only AT-SPI/cursor detection runs in child (blocking calls)
    if (fork() == 0) {
        setsid();

        if (!caretCaptured) {
            std::ofstream dbg("/tmp/hyprclipx-debug.log", std::ios::app);
            dbg << "  entering fallback (forked child)\n" << std::flush;

            auto getCursorPos = [](int& outX, int& outY) -> bool {
                FILE* p = popen("hyprctl cursorpos -j 2>/dev/null", "r");
                if (!p) return false;
                char buf[256] = {};
                std::string result;
                while (fgets(buf, sizeof(buf), p)) result += buf;
                pclose(p);
                size_t xp = result.find("\"x\":");
                size_t yp = result.find("\"y\":");
                if (xp != std::string::npos) outX = std::atoi(result.c_str() + xp + 4);
                if (yp != std::string::npos) outY = std::atoi(result.c_str() + yp + 4);
                return outX >= 0 && outY >= 0;
            };

            // ---- Strategy 2: AT-SPI via Python (XWayland / legacy apps) ----
            dbg << "  trying AT-SPI...\n" << std::flush;
            bool caretFound = false;
            FILE* pipe = popen(("/usr/bin/python3 " + caretHelper + " 2>/dev/null").c_str(), "r");
            if (pipe) {
                char buf[256] = {};
                std::string result;
                while (fgets(buf, sizeof(buf), pipe)) result += buf;
                int rc = pclose(pipe);
                if (rc == 0 && !result.empty()) {
                    int cx = -1, cy = -1;
                    size_t xp = result.find("\"x\":");
                    size_t yp = result.find("\"y\":");
                    if (xp != std::string::npos) cx = std::atoi(result.c_str() + xp + 4);
                    if (yp != std::string::npos) cy = std::atoi(result.c_str() + yp + 4);
                    if (cx >= 0 && cy >= 0) {
                        cx = std::clamp(cx, monX, monX + monW - 1);
                        cy = std::clamp(cy, monY, monY + monH - 1);
                        writeCaretFile(cx, cy);
                        caretFound = true;
                        dbg << "  STRATEGY 2 SUCCESS: " << cx << "," << cy << "\n" << std::flush;
                    }
                }
            }

            // ---- Strategy 3: Mouse cursor position (last resort) ----
            if (!caretFound) {
                dbg << "  AT-SPI failed, trying cursor pos...\n" << std::flush;
                int mx = -1, my = -1;
                if (getCursorPos(mx, my)) {
                    writeCaretFile(mx, my);
                    dbg << "  STRATEGY 3 (cursor): " << mx << "," << my << "\n" << std::flush;
                }
            }
        }

        execlp("hyprclipx-ui", "hyprclipx-ui", uiArg.c_str(), nullptr);
        _exit(1);
    }
}

void sendUICommand(const std::string& cmd) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}

    if (fork() == 0) {
        setsid();
        std::string arg = "--" + cmd;
        execlp("hyprclipx-ui", "hyprclipx-ui", arg.c_str(), nullptr);
        _exit(1);
    }
}

} // namespace hyprclipx
