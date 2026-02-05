#pragma once
// Single Responsibility: Global instance management (RAII)

#include "Forward.hpp"
#include "Config.hpp"
#include <memory>

namespace hyprclipx {

// Global instances (initialized in Globals.cpp)
extern std::unique_ptr<ClipboardManager> g_clipboardManager;
extern std::unique_ptr<ClipboardRenderer> g_clipboardRenderer;
extern std::unique_ptr<IPCHandler> g_ipcHandler;
extern Config g_config;

// Hyprland plugin handle
extern void* g_handle;

// Lifecycle
void initGlobals();
void cleanupGlobals();
void reloadConfig();

} // namespace hyprclipx
