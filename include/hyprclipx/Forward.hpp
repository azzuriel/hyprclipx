#pragma once
// Forward declarations for loose coupling (SOLID: Dependency Inversion)

namespace hyprclipx {

// Data structures
struct ClipboardEntry;
struct Config;

// Managers (Single Responsibility each)
class ClipboardManager;
class ClipboardRenderer;
class IPCHandler;

} // namespace hyprclipx
