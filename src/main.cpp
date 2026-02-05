// HyprClipX - Layer-shell clipboard manager for Hyprland
// Plugin entry point and Hyprland API integration

#define WLR_USE_UNSTABLE
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

#include "hyprclipx/Globals.hpp"
#include "hyprclipx/ClipboardManager.hpp"
#include "hyprclipx/ClipboardRenderer.hpp"
#include "hyprclipx/IPCHandler.hpp"

using namespace hyprclipx;

inline HANDLE g_pHandle = nullptr;

// ============================================================================
// IPC Command Handler
// ============================================================================

static SHyprCtlCommand::eHyprCtlOutputFormat g_outputFormat;

static std::string onHyprCtlCommand(std::string request) {
    // Parse command: "hyprclipx:command args"
    size_t colonPos = request.find(':');
    if (colonPos == std::string::npos) {
        return "invalid command format";
    }

    std::string cmd = request.substr(colonPos + 1);
    std::string args;

    size_t spacePos = cmd.find(' ');
    if (spacePos != std::string::npos) {
        args = cmd.substr(spacePos + 1);
        cmd = cmd.substr(0, spacePos);
    }

    if (g_ipcHandler) {
        return g_ipcHandler->handleCommand(cmd, args);
    }
    return "error: IPC handler not initialized";
}

// ============================================================================
// Dispatchers
// ============================================================================

static SDispatchResult dispatchShow(std::string) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->show();
    }
    return {};
}

static SDispatchResult dispatchHide(std::string) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->hide();
    }
    return {};
}

static SDispatchResult dispatchToggle(std::string) {
    if (g_clipboardRenderer) {
        g_clipboardRenderer->toggle();
    }
    return {};
}

// ============================================================================
// Keybind Handler
// ============================================================================

static void onKeyPress(void*, SCallbackInfo& info, std::any data) {
    // Check if our hotkey is pressed
    // TODO: Implement proper hotkey handling based on config
    (void)info;
    (void)data;
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

    // Initialize global instances
    initGlobals();

    // Register IPC commands
    HyprlandAPI::registerHyprCtlCommand(
        handle,
        SHyprCtlCommand {
            .command = "hyprclipx",
            .exact = false,
            .fn = onHyprCtlCommand
        }
    );

    // Register dispatchers
    HyprlandAPI::addDispatcherV2(handle, "hyprclipx:show", dispatchShow);
    HyprlandAPI::addDispatcherV2(handle, "hyprclipx:hide", dispatchHide);
    HyprlandAPI::addDispatcherV2(handle, "hyprclipx:toggle", dispatchToggle);

    // Register config values
    HyprlandAPI::addConfigValue(handle, "plugin:hyprclipx:enabled",
                                Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(handle, "plugin:hyprclipx:hotkey",
                                Hyprlang::STRING{"SUPER V"});

    // Register key callback (for hotkey handling)
    static auto keyCallback = HyprlandAPI::registerCallbackDynamic(
        handle, "keyPress", onKeyPress);

    HyprlandAPI::addNotification(handle,
        "[HyprClipX] Loaded successfully!",
        CColor{0.2f, 0.8f, 0.2f, 1.0f},
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
        CColor{0.8f, 0.8f, 0.2f, 1.0f},
        3000);
}
