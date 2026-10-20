// HyprClipX - Layer-shell clipboard manager for Hyprland
// Plugin entry point - LIGHTWEIGHT (no GTK, no threads)
// UI runs as separate process (hyprclipx-ui)

#define WLR_USE_UNSTABLE
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include <lua.h>

#include "hyprclipx/Globals.hpp"
#include "hyprclipx/IPCHandler.hpp"

using namespace hyprclipx;

inline HANDLE g_pHandle = nullptr;

// ============================================================================
// IPC Command Handler (hyprctl hyprclipx <cmd> [args])
// ============================================================================

static std::string cmdHyprclipx(eHyprCtlOutputFormat, std::string request) {
    std::string cmd = request;
    std::string args;

    size_t spacePos = cmd.find(' ');
    if (spacePos != std::string::npos) {
        args = cmd.substr(spacePos + 1);
        cmd = cmd.substr(0, spacePos);
    }

    if (g_ipcHandler) {
        return g_ipcHandler->handleCommand(cmd, args);
    }
    return "error: not initialized";
}

// ============================================================================
// Dispatchers (matching ags-toggle-clipboard.template flow)
// ============================================================================

static SDispatchResult dispatchShow(std::string) {
    captureAndSendUI("show");
    return {.success = true};
}

static SDispatchResult dispatchHide(std::string) {
    sendUICommand("hide");
    return {.success = true};
}

// Toggle: capture caret BEFORE toggle (matching AGS flow)
static SDispatchResult dispatchToggle(std::string) {
    captureAndSendUI("toggle");
    return {.success = true};
}

// ============================================================================
// Lua functions (hl.plugin.hyprclipx.<fn>) - Lua config mode
// ============================================================================

static int luaShow(lua_State*) {
    captureAndSendUI("show");
    return 0;
}

static int luaHide(lua_State*) {
    sendUICommand("hide");
    return 0;
}

// Toggle: capture caret BEFORE toggle (matching AGS flow)
static int luaToggle(lua_State*) {
    captureAndSendUI("toggle");
    return 0;
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_pHandle = handle;
    g_handle = handle;

    initGlobals();

    // Register IPC command
    HyprlandAPI::registerHyprCtlCommand(g_handle,
        SHyprCtlCommand{"hyprclipx", true, cmdHyprclipx});

    // Register dispatchers (hyprland.conf) or Lua functions (hyprland.lua)
    if (Config::mgr()->type() == Config::CONFIG_LEGACY) {
        HyprlandAPI::addDispatcherV2(handle, "hyprclipx:show", dispatchShow);
        HyprlandAPI::addDispatcherV2(handle, "hyprclipx:hide", dispatchHide);
        HyprlandAPI::addDispatcherV2(handle, "hyprclipx:toggle", dispatchToggle);
    } else {
        HyprlandAPI::addLuaFunction(handle, "hyprclipx", "show", luaShow);
        HyprlandAPI::addLuaFunction(handle, "hyprclipx", "hide", luaHide);
        HyprlandAPI::addLuaFunction(handle, "hyprclipx", "toggle", luaToggle);
    }

    HyprlandAPI::addNotification(handle,
        "[HyprClipX] Loaded successfully!",
        CHyprColor(0.2f, 0.8f, 0.2f, 1.0f),
        5000);

    return {
        "hyprclipx",
        "Layer-shell clipboard manager for Hyprland",
        "HyprClipX",
        "0.1.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT() {
    cleanupGlobals();

    HyprlandAPI::addNotification(g_pHandle,
        "[HyprClipX] Unloaded",
        CHyprColor(0.8f, 0.8f, 0.2f, 1.0f),
        3000);
}
