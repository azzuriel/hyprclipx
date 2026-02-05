// Single Responsibility: IPC command handling

#include "hyprclipx/IPCHandler.hpp"
#include "hyprclipx/Globals.hpp"

namespace hyprclipx {

IPCHandler::IPCHandler() {
    // Register standard commands
    registerCommand("show", cmdShow);
    registerCommand("hide", cmdHide);
    registerCommand("toggle", cmdToggle);
    registerCommand("paste", cmdPaste);
    registerCommand("clear", cmdClear);
    registerCommand("reload", cmdReload);
    registerCommand("list", cmdList);
    registerCommand("offset", cmdOffset);
}

IPCHandler::~IPCHandler() = default;

void IPCHandler::registerCommand(const std::string& name,
                                  std::function<std::string(const std::string&)> handler) {
    m_commands[name] = std::move(handler);
}

std::string IPCHandler::handleCommand(const std::string& command,
                                       const std::string& args) {
    auto it = m_commands.find(command);
    if (it != m_commands.end()) {
        return it->second(args);
    }
    return "unknown command: " + command;
}

std::string IPCHandler::cmdShow(const std::string& /*args*/) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->show();
        return "shown";
    }
    return "error: renderer not initialized";
}

std::string IPCHandler::cmdHide(const std::string& /*args*/) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->hide();
        return "hidden";
    }
    return "error: renderer not initialized";
}

std::string IPCHandler::cmdToggle(const std::string& /*args*/) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->toggle();
        return g_clipboardRenderer->isVisible() ? "shown" : "hidden";
    }
    return "error: renderer not initialized";
}

std::string IPCHandler::cmdPaste(const std::string& args) {
    if (g_clipboardManager && !args.empty()) {
        if (g_clipboardManager->copyToClipboard(args)) {
            if (g_clipboardRenderer) g_clipboardRenderer->hide();
            g_clipboardManager->pasteFromClipboard();
            return "pasted";
        }
        return "error: item not found";
    }
    return "error: no uuid provided";
}

std::string IPCHandler::cmdClear(const std::string& args) {
    if (g_clipboardManager) {
        bool keepFavorites = args != "all";
        g_clipboardManager->clearAll(keepFavorites);
        if (g_clipboardRenderer) g_clipboardRenderer->refresh();
        return "cleared";
    }
    return "error: manager not initialized";
}

std::string IPCHandler::cmdReload(const std::string& /*args*/) {
    reloadConfig();
    return "reloaded";
}

std::string IPCHandler::cmdList(const std::string& args) {
    if (g_clipboardManager) {
        auto items = g_clipboardManager->getItems(args.empty() ? "all" : args);
        std::string result;
        for (const auto& item : items) {
            result += item.uuid + " | " + item.preview + "\n";
        }
        return result.empty() ? "no items" : result;
    }
    return "error: manager not initialized";
}

std::string IPCHandler::cmdOffset(const std::string& args) {
    if (g_clipboardRenderer) {
        if (args == "reset") {
            g_config.offsetX = 0;
            g_config.offsetY = 0;
            g_clipboardRenderer->setOffset(0, 0);
            return "offset reset to 0,0";
        }

        // Parse "x,y" format
        size_t comma = args.find(',');
        if (comma != std::string::npos) {
            try {
                int x = std::stoi(args.substr(0, comma));
                int y = std::stoi(args.substr(comma + 1));
                g_config.offsetX = x;
                g_config.offsetY = y;
                g_clipboardRenderer->setOffset(x, y);
                return "offset set to " + std::to_string(x) + "," + std::to_string(y);
            } catch (...) {
                return "error: invalid offset format (use: x,y)";
            }
        }

        // Return current offset
        return "current offset: " +
               std::to_string(g_clipboardRenderer->getOffsetX()) + "," +
               std::to_string(g_clipboardRenderer->getOffsetY());
    }
    return "error: renderer not initialized";
}

} // namespace hyprclipx
