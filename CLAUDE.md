# CLAUDE.md

## MANDATORY WORKFLOW

- **QUESTIONS ALWAYS HAVE PRIORITY** - If the user's request contains questions, answer ALL questions FIRST before implementing anything
- **NEVER implement while questions are unanswered** - This ensures the implementation matches user intent
- **NEVER ask questions you can answer yourself** - Research first using web search, file reads, or other tools before asking the user
- **Only ask questions that require user decision** - Technical facts, versions, documentation can be researched independently
- **NEVER invent values when you have correct data** - If you researched a value (version, number, name), use EXACTLY that value, do not make up different values

## STRICTLY FORBIDDEN

- **NEVER use `sudo`** - no exceptions, no matter what
- **NO workarounds** - always implement the correct solution
- **NO hacks** - clean, correct code only
- **NO shortcuts** - complete implementations required
- **NO tricks** - use standard solutions
- **NEVER delete code to avoid implementation** - implement the code, don't remove it
- **NO stubs** - all functions must be fully implemented
- **NO TODOs** - do it right immediately
- **NO orphaned code** - all functions/methods must be used; if you implement something, use it
- **NO dead code** - remove unused code, don't leave it hanging

## FORBIDDEN COMMANDS

- **NEVER copy files to `/var/cache/hyprpm/`** - this is a system cache managed by hyprpm
- **NEVER use `hyprctl plugin load`** - the user manages plugin loading themselves
- **NEVER use `hyprpm update`** - this pulls from GitHub, not local changes
- **NEVER write to system directories** - stay within the project directory

## FORBIDDEN PHRASES IN CODE/COMMENTS

These phrases indicate incomplete work and are NOT allowed:

- "for now"
- "for the moment"
- "temporarily"
- "temp"
- "quick fix"
- "will do later"
- "later"
- "eventually"
- "at some point"
- "in the future"
- "TODO"
- "FIXME"
- "XXX"
- "HACK"
- "WIP"
- "work in progress"
- "not yet"
- "placeholder"
- "dummy"
- "mock"
- "fake"
- "stub"
- "skeleton"
- "boilerplate to be filled"
- "implement me"
- "needs implementation"
- "not implemented"
- "unimplemented"
- "pending"
- "TBD"
- "to be done"
- "to be decided"
- "figure out"
- "sort out"
- "come back to"
- "revisit"
- "refactor later"
- "clean up later"
- "good enough"
- "works for now"
- "quick and dirty"
- "simple version"
- "basic version"
- "minimal version"
- "v1"
- "first pass"
- "initial implementation"
- "rough"
- "draft"

## PROJECT: HyprClipX

Layer-shell clipboard manager for Hyprland. 1:1 port of the AGS clipboard manager (TypeScript) to C++.

### Architecture (TWO components)

1. **hyprclipx.so** - Hyprland plugin (loaded into compositor)
   - NO GTK, NO threads, NO blocking calls
   - Dispatchers: `hyprclipx:toggle`, `hyprclipx:show`, `hyprclipx:hide`
   - IPC via `hyprctl hyprclipx <command>`
   - Uses Hyprland internal APIs for window info
   - Communicates with UI via `fork()` + `execlp("hyprclipx-ui")`
   - Follow patterns from hyprzones (`/mnt/code/SRC/GITHUB/hyprzones/`)

2. **hyprclipx-ui** - Standalone GTK4 binary (separate Wayland client process)
   - GTK4 + gtk4-layer-shell for overlay window
   - Receives commands via Unix socket `/tmp/hyprclipx-ui.sock`
   - Talks to `clipman-daemon.py` on `/tmp/clipman.sock` for clipboard data
   - Smart paste: terminal (Ctrl+Shift+V), kitty (remote), browser (delay+Ctrl+V), xwayland (xdotool)
   - Caret positioning via AT-SPI (primary), mouse fallback only if AT-SPI fails

### Critical Safety Rules

- **NEVER call `gtk_init()` inside the compositor plugin** - this WILL deadlock and freeze the entire system
- **NEVER use threads in the plugin** - Hyprland is single-threaded
- **NEVER use `popen()`/`system()` in the plugin main thread** - use `fork()` + `execlp()` instead
- **NEVER block the compositor** - all slow operations must happen in forked child processes

### Build

```bash
./build.sh          # Release build (both targets)
./build.sh debug    # Debug build
./build.sh clean    # Clean build directory
./build.sh install  # Build + install plugin to /usr/lib/hyprland/plugins/
```

### Tech Stack

- C++23, CMake, PkgConfig
- Hyprland 0.53.1+ plugin API
- GTK4 + gtk4-layer-shell (UI binary only)
- Pango + Cairo (text rendering in UI)

### Key Files

- `src/main.cpp` - Plugin entry point (dispatchers, IPC, lifecycle)
- `src/Globals.cpp` - Plugin globals, caret capture, fork+exec UI
- `src/IPCHandler.cpp` - hyprctl command handling
- `src/main_ui.cpp` - UI binary entry point (socket listener, GTK main loop)
- `src/ClipboardRenderer.cpp` - GTK4 layer-shell window, all UI logic
- `src/ClipboardManager.cpp` - clipman-daemon IPC client
- `src/ConfigParser.cpp` - Hyprland config value reader

### Reference

- Correct plugin patterns: `/mnt/code/SRC/GITHUB/hyprzones/`
