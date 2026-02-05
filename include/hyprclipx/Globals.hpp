#pragma once
#include "Forward.hpp"
#include "Config.hpp"
#include <memory>
#include <string>

namespace hyprclipx {

extern std::unique_ptr<IPCHandler> g_ipcHandler;
extern Config g_config;
extern void* g_handle;

void initGlobals();
void cleanupGlobals();
void reloadConfig();

// Capture caret/window using Hyprland APIs, then fork+exec UI command
// (Matching ags-toggle-clipboard.template: capture BEFORE opening window)
void captureAndSendUI(const std::string& cmd);

// Send command to UI without caret capture (e.g., hide)
void sendUICommand(const std::string& cmd);

} // namespace hyprclipx
