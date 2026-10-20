// IPC command handling (plugin side - NO GTK!)
// Forwards UI commands to hyprclipx-ui via fork+exec

#include "hyprclipx/IPCHandler.hpp"
#include "hyprclipx/Globals.hpp"

#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>

namespace hyprclipx {

IPCHandler::IPCHandler() {
    registerCommand("show", cmdShow);
    registerCommand("hide", cmdHide);
    registerCommand("toggle", cmdToggle);
    registerCommand("reload", cmdReload);
    registerCommand("focus", cmdFocus);
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

std::string IPCHandler::cmdShow(const std::string&) {
    captureAndSendUI("show");
    return "ok";
}

std::string IPCHandler::cmdHide(const std::string&) {
    sendUICommand("hide");
    return "ok";
}

std::string IPCHandler::cmdToggle(const std::string&) {
    captureAndSendUI("toggle");
    return "ok";
}

std::string IPCHandler::cmdReload(const std::string&) {
    reloadConfig();
    return "config reloaded";
}

// Focus a window by Hyprland selector (e.g. "address:0x..."). Runs in the
// compositor, so the UI needs no dispatcher syntax of either config format.
std::string IPCHandler::cmdFocus(const std::string& args) {
    const auto window = Desktop::viewState()->query().selector(args).runWindow();
    if (!window)
        return "error: no window matches " + args;

    const auto result = ::Config::Actions::focus(window);
    if (!result)
        return "error: " + result.error().message;
    return "ok";
}

} // namespace hyprclipx
