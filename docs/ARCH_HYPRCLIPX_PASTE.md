# Architecture: Smart Paste in HyprClipX

## Layer Stack

```
┌─────────────────────────────────────────────────────────────────┐
│ Hyprland (Wayland Compositor)                                   │
│   - wlroots (Wayland Library)                                   │
│   - Renders all windows via DRM/KMS                             │
│   - HyprClipX plugin: dispatchers, IPC, fork+exec              │
└─────────────────────┬───────────────────────────────────────────┘
                      │ Wayland Protocol (wl_surface, xdg_shell)
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ XWayland (X11 Compatibility Layer)                              │
│   - Xorg server for X11 apps under Wayland                     │
│   - JetBrains IDEs, Electron apps, etc.                        │
└─────────────────────┬───────────────────────────────────────────┘
                      │ X11 Protocol
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ Target Application                                              │
│   - Terminal (kitty, foot, alacritty, ...)                      │
│   - Browser (Firefox, Chromium, ...)                            │
│   - IDE (CLion, VSCode, ...)                                    │
│   - Any Wayland/XWayland client                                 │
└─────────────────────┬───────────────────────────────────────────┘
                      │ PTY / Input Events
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ Shell / Application Input                                       │
│   - Receives pasted text via simulated keystrokes               │
│   - Or via clipboard protocol + paste shortcut                  │
└─────────────────────────────────────────────────────────────────┘
```

## Smart Paste Flow

HyprClipX detects the target window type and uses the appropriate paste method:

### 1. Terminal Detection (Ctrl+Shift+V)
```
clipman-daemon (paste UUID) → wl-copy "text" → wtype -M ctrl -M shift -k v
```
Standard terminal paste shortcut. Works with: foot, alacritty, wezterm, etc.

### 2. Kitty Terminal (Remote Paste)
```
clipman-daemon (paste UUID) → kitty @ --to=unix:/tmp/kitty-* paste-to-window --match recent:0
```
Uses kitty's remote control protocol for native paste. Falls back to terminal paste (Ctrl+Shift+V) if remote control is unavailable.

### 3. Browser Detection (Delayed Ctrl+V)
```
clipman-daemon (paste UUID) → wl-copy "text" → sleep 100ms → wtype -M ctrl -k v
```
Browsers use Ctrl+V (not Ctrl+Shift+V). Small delay ensures clipboard is ready.

### 4. XWayland Apps (xdotool)
```
clipman-daemon (paste UUID) → wl-copy "text" → xdotool key ctrl+shift+v
```
X11 apps under XWayland need xdotool for keyboard simulation instead of wtype.

### 5. Default (wtype Ctrl+V)
```
clipman-daemon (paste UUID) → wl-copy "text" → wtype -M ctrl -k v
```
Fallback for all other Wayland-native applications.

## Window Detection Logic

```
getActiveWindowInfo() via hyprctl activewindow -j
    │
    ├─ isKittyTerminal()?  → kitty remote paste (with fallback)
    ├─ isTerminal()?       → Ctrl+Shift+V via wtype
    ├─ isBrowser()?        → delay + Ctrl+V via wtype
    ├─ xwayland == true?   → xdotool key ctrl+shift+v
    └─ default             → Ctrl+V via wtype
```

### Terminal Detection
Matches window class/title against: `kitty`, `foot`, `alacritty`, `wezterm`, `termite`, `xterm`, `urxvt`, `st-256color`, `konsole`, `gnome-terminal`, `tilix`

### Browser Detection
Matches window class against: `firefox`, `chromium`, `google-chrome`, `brave`, `vivaldi`, `opera`, `microsoft-edge`, `zen`, `floorp`, `librewolf`

## Dependencies

| App Type | Clipboard Write | Paste Method |
|----------|----------------|--------------|
| Wayland Terminal | `wl-copy` | `wtype` (Ctrl+Shift+V) |
| Kitty Terminal | kitty remote control | `kitty @ paste-to-window` |
| Wayland Browser | `wl-copy` | `wtype` (Ctrl+V, with delay) |
| XWayland (X11) | `wl-copy` | `xdotool` (Ctrl+Shift+V) |
| Wayland Default | `wl-copy` | `wtype` (Ctrl+V) |

## Installation

```bash
# Wayland tools (required)
sudo pacman -S wl-clipboard wtype

# X11/XWayland tools (for CLion, JetBrains IDEs, etc.)
sudo pacman -S xdotool xclip

# Kitty remote control (enable in kitty.conf)
# allow_remote_control yes
# listen_on unix:/tmp/kitty-{kitty_pid}
```

## Why JetBrains IDEs Need XWayland

JetBrains IDEs are based on Java Swing/AWT, which has no native Wayland support. They run through XWayland - an X11 compatibility layer within Wayland.

Check if an app runs under XWayland:
```bash
hyprctl clients -j | jq '.[] | select(.class == "jetbrains-clion") | .xwayland'
# true = XWayland, false = Native Wayland
```

## Troubleshooting

**Paste not working in CLion Terminal:**
1. Check: `which xdotool` - if missing: `sudo pacman -S xdotool`
2. Check: `which wl-copy` - if missing: `sudo pacman -S wl-clipboard`

**Paste not working in Kitty:**
1. Check kitty remote control: `kitty @ ls` - if error, add to `kitty.conf`:
   ```
   allow_remote_control yes
   listen_on unix:/tmp/kitty-{kitty_pid}
   ```
2. Fallback: uses `wtype -M ctrl -M shift -k v` automatically

**Paste not working in native Wayland apps:**
1. Check: `which wtype` - if missing: `sudo pacman -S wtype`

**Paste not working in browsers:**
1. Ensure `wtype` is installed
2. If timing issue, the 100ms delay may need adjustment in config
