# Architecture: Clipboard Paste in CLion Terminal

## Layer Stack

```
┌─────────────────────────────────────────────────────────────────┐
│ Hyprland (Wayland Compositor)                                   │
│   - wlroots (Wayland Library)                                   │
│   - Renders all windows via DRM/KMS                             │
└─────────────────────┬───────────────────────────────────────────┘
                      │ Wayland Protocol (wl_surface, xdg_shell)
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ XWayland (X11 Compatibility Layer)                              │
│   - Xorg server for X11 apps under Wayland                     │
│   - CLion requires X11 (no native Wayland support)             │
└─────────────────────┬───────────────────────────────────────────┘
                      │ X11 Protocol
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ CLion (JetBrains IDE)                                           │
│   - JVM (Java Virtual Machine)                                  │
│   - AWT/Swing UI Toolkit                                        │
└─────────────────────┬───────────────────────────────────────────┘
                      │ JNI (Java Native Interface)
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ JediTerm (Terminal Emulator Library)                            │
│   - VT100/xterm Escape Sequence Parser                          │
│   - PTY Master Side                                             │
└─────────────────────┬───────────────────────────────────────────┘
                      │ PTY (Pseudo-Terminal) - /dev/pts/X
                      │ ioctl(), read(), write()
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ zsh (Shell)                                                     │
│   - PTY Slave Side                                              │
│   - stdin/stdout/stderr                                         │
│   - Job Control, Line Editing (ZLE)                             │
└─────────────────────┬───────────────────────────────────────────┘
                      │ fork() + execve()
                      │ Pipes: stdin/stdout/stderr
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ Claude Code (Node.js CLI)                                       │
│   - Node.js Runtime (libuv for I/O)                             │
│   - HTTPS → api.anthropic.com                                   │
│   - child_process for bash commands                             │
└─────────────────────────────────────────────────────────────────┘
```

## Clipboard Paste Flow

### Wayland Native Apps
```
wl-copy "text"  →  Wayland Clipboard  →  wtype -M ctrl -M shift -k v
```
Required package: `wtype`

### XWayland Apps (CLion, JetBrains IDEs)
```
wl-copy "text"  →  XWayland Clipboard Bridge  →  xdotool key ctrl+shift+v
```
Required package: `xdotool`

## Smart Paste Flow (HyprClipX)

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
    ├─ isKittyTerminal()?  → kitty remote paste (with fallback to Ctrl+Shift+V)
    ├─ isTerminal()?       → Ctrl+Shift+V (wtype or xdotool if xwayland)
    ├─ isBrowser()?        → delay + Ctrl+V (wtype or xdotool if xwayland)
    └─ default             → Ctrl+V (wtype or xdotool if xwayland)
```

### Terminal Detection
Matches window class/title against: `kitty`, `foot`, `alacritty`, `wezterm`, `konsole`, `gnome-terminal`, `xterm`, `urxvt`, `terminator`, `tilix`, `st`, `rxvt`, `sakura`, `terminology`, `guake`, `tilda`, `hyper`, `tabby`, `contour`, `cool-retro-term`, `claude`

Note: JetBrains IDEs (PyCharm, CLion, etc.) are NOT terminals. They use standard Ctrl+V for paste. Their embedded JediTerm terminal also accepts Ctrl+V because the IDE intercepts it.

### Browser Detection
Matches window class against: `firefox`, `chromium`, `google-chrome`, `brave`, `vivaldi`, `opera`, `microsoft-edge`, `zen`, `floorp`, `librewolf`

## Dependencies

| App Type | Clipboard Write | Paste Method (Keyboard Simulation) |
|----------|----------------|-------------------------------------|
| Wayland Native | `wl-copy` | `wtype` |
| Kitty Terminal | kitty remote control | `kitty @ paste-to-window` |
| XWayland (X11) | `wl-copy` / `xclip` | `xdotool` |

## Installation

```bash
# Wayland tools
sudo pacman -S wl-clipboard wtype

# X11/XWayland tools (for CLion, JetBrains IDEs, etc.)
sudo pacman -S xdotool xclip

# Kitty remote control (enable in kitty.conf)
# allow_remote_control yes
# listen_on unix:/tmp/kitty-{kitty_pid}
```

## Why CLion Needs XWayland

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
