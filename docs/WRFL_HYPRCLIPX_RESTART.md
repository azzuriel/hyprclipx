# Restarting HyprClipX-UI

## Correct Method

```bash
# Stop old instance
killall -9 hyprclipx-ui
rm -f /tmp/hyprclipx-ui.sock

# Start HyprClipX-UI in the background
setsid -f hyprclipx-ui --show >/dev/null 2>&1
```

## One-Liner

```bash
killall -9 hyprclipx-ui; sleep 0.5; rm -f /tmp/hyprclipx-ui.sock; setsid -f hyprclipx-ui --show >/dev/null 2>&1
```

## Testing Dev Build (without hyprpm install)

```bash
killall -9 hyprclipx-ui; sleep 0.5; rm -f /tmp/hyprclipx-ui.sock; setsid -f /mnt/code/SRC/GITHUB/hyprclipx/build/hyprclipx-ui --show >/dev/null 2>&1
```

## WRONG - Do Not Use

```bash
# WRONG: timeout kills hyprclipx-ui after X seconds
timeout 5 hyprclipx-ui --show

# WRONG: Without setsid it stays attached to the terminal
hyprclipx-ui --show &

# WRONG: nohup does not work reliably
nohup hyprclipx-ui --show &

# WRONG: Without socket cleanup the new instance connects to the old one
hyprclipx-ui --toggle
```

## Why setsid?

- `setsid -f` starts HyprClipX-UI as its own session, independent from the terminal
- UI keeps running even when the terminal is closed
- No hanging, no blocking

## Why Socket Cleanup?

- HyprClipX-UI uses `/tmp/hyprclipx-ui.sock` for IPC
- Without `rm -f /tmp/hyprclipx-ui.sock` a new instance tries to use the old socket
- `--toggle` sends commands to the running instance instead of starting a new one

## Check if HyprClipX-UI Is Running

```bash
pgrep -f hyprclipx-ui
```

## When to Restart HyprClipX-UI?

- After changes to UI code (ClipboardRenderer.cpp)
- After changes to CSS styles
- After building with new features
- To test the dev build without hyprpm install
