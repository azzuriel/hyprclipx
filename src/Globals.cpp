// Global instance management (plugin side - NO GTK!)
// Uses Hyprland internal APIs for window info, fork+exec for UI

#include "hyprclipx/Globals.hpp"
#include "hyprclipx/IPCHandler.hpp"
#include "hyprclipx/ConfigParser.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/desktop/view/Window.hpp>

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
    g_config.clipmanClient = home + "/.config/iconmanager/helpers/clipman-client.py";
    g_config.caretHelper = home + "/.config/iconmanager/helpers/get-caret-position.py";
    g_config.userSettingsFile = home + "/.config/iconmanager/user-settings.json";

    g_ipcHandler = std::make_unique<IPCHandler>();
}

void cleanupGlobals() {
    g_ipcHandler.reset();
}

void reloadConfig() {
    g_config = loadConfig();
}

// ============================================================================
// Toggle helpers (matching ags-toggle-clipboard.template flow EXACTLY)
// 1. Get caret position via AT-SPI (PRIMARY)
// 2. Fallback to mouse ONLY if AT-SPI fails
// 3. Save previous window
// 4. Toggle UI
// ============================================================================

void captureAndSendUI(const std::string& cmd) {
    // Reap zombie children from previous calls
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}

    // Save previous window address BEFORE opening UI (must capture now,
    // because focus changes once clipboard window opens)
    std::string windowAddr;
    {
        auto monitor = g_pCompositor->getMonitorFromCursor();
        if (monitor && monitor->m_activeWorkspace) {
            auto pWindow = monitor->m_activeWorkspace->getLastFocusedWindow();
            if (pWindow) {
                windowAddr = std::format("0x{:x}", (uintptr_t)pWindow.get());
            }
        }
    }

    if (!windowAddr.empty()) {
        std::ofstream f(g_config.prevWindowFile);
        if (f.is_open()) f << windowAddr;
    }

    // Capture monitor bounds from compositor (only available in parent process)
    int monX = 0, monY = 0, monW = 1920, monH = 1080;
    {
        auto monitor = g_pCompositor->getMonitorFromCursor();
        if (monitor) {
            monX = static_cast<int>(monitor->m_position.x);
            monY = static_cast<int>(monitor->m_position.y);
            monW = static_cast<int>(monitor->m_size.x);
            monH = static_cast<int>(monitor->m_size.y);
        }
    }

    // Fork child: capture CARET position + exec UI
    // All blocking calls happen in child (never blocks compositor!)
    std::string caretHelper = g_config.caretHelper;
    std::string caretPosFile = g_config.caretPosFile;
    std::string uiArg = "--" + cmd;

    if (fork() == 0) {
        setsid();  // Detach from compositor process group

        // Format: caretX,caretY,monX,monY,monW,monH
        auto writeCaretFile = [&](int cx, int cy) {
            std::ofstream f(caretPosFile);
            if (f.is_open())
                f << cx << "," << cy << "," << monX << "," << monY << "," << monW << "," << monH;
        };

        // Check if a position is within monitor bounds
        auto isInsideMonitor = [&](int px, int py) -> bool {
            return px >= monX && px < monX + monW && py >= monY && py < monY + monH;
        };

        // Get cursor position via hyprctl (always in Hyprland logical coordinates)
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

        // 1. AT-SPI caret capture (PRIMARY - matching AGS get-caret-position.py)
        bool caretFound = false;
        FILE* pipe = popen(("/usr/bin/python3 " + caretHelper + " 2>/dev/null").c_str(), "r");
        if (pipe) {
            char buf[256] = {};
            std::string result;
            while (fgets(buf, sizeof(buf), pipe)) result += buf;
            int rc = pclose(pipe);
            if (rc == 0 && !result.empty()) {
                // Parse {"x": 123, "y": 456}
                int cx = -1, cy = -1;
                size_t xp = result.find("\"x\":");
                size_t yp = result.find("\"y\":");
                if (xp != std::string::npos) cx = std::atoi(result.c_str() + xp + 4);
                if (yp != std::string::npos) cy = std::atoi(result.c_str() + yp + 4);
                if (cx >= 0 && cy >= 0) {
                    // Validate AT-SPI position against monitor bounds
                    // AT-SPI can return coordinates in wrong space (physical pixels
                    // vs logical) on multi-monitor setups with scaling
                    if (isInsideMonitor(cx, cy)) {
                        writeCaretFile(cx, cy);
                        caretFound = true;
                    }
                    // AT-SPI position outside monitor → fall through to cursor
                }
            }
        }

        // 2. Fallback to cursor position if AT-SPI failed or returned out-of-bounds
        if (!caretFound) {
            int mx = -1, my = -1;
            if (getCursorPos(mx, my)) {
                writeCaretFile(mx, my);
            }
        }

        // Exec UI binary (replaces this child process)
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
