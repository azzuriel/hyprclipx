#pragma once
// Single Responsibility: IPC command handling (hyprctl integration)

#include "Forward.hpp"
#include <string>
#include <functional>
#include <unordered_map>

namespace hyprclipx {

class IPCHandler {
public:
    IPCHandler();
    ~IPCHandler();

    // Command registration
    void registerCommand(const std::string& name,
                        std::function<std::string(const std::string&)> handler);

    // Command execution
    std::string handleCommand(const std::string& command, const std::string& args);

    // Standard commands
    static std::string cmdShow(const std::string& args);
    static std::string cmdHide(const std::string& args);
    static std::string cmdToggle(const std::string& args);
    static std::string cmdPaste(const std::string& args);
    static std::string cmdClear(const std::string& args);
    static std::string cmdReload(const std::string& args);
    static std::string cmdList(const std::string& args);
    static std::string cmdOffset(const std::string& args);

private:
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> m_commands;
};

} // namespace hyprclipx
