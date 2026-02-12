#!/usr/bin/env python3
"""Get text caret position - tries AT-SPI first, then X11 for XWayland apps."""
import json
import subprocess

def get_caret_atspi():
    """Try AT-SPI for native Wayland apps."""
    try:
        import gi
        gi.require_version('Atspi', '2.0')
        from gi.repository import Atspi

        def find_caret(obj, depth=0):
            if depth > 15:
                return None
            try:
                state = obj.get_state_set()
                if state and state.contains(Atspi.StateType.FOCUSED):
                    text = obj.get_text()
                    if text:
                        offset = text.get_caret_offset()
                        if offset >= 0:
                            rect = text.get_character_extents(offset, Atspi.CoordType.SCREEN)
                            if rect and rect.x >= 0 and rect.y >= 0:
                                return {"x": rect.x, "y": rect.y}
                for i in range(obj.get_child_count()):
                    child = obj.get_child_at_index(i)
                    if child:
                        result = find_caret(child, depth + 1)
                        if result:
                            return result
            except:
                pass
            return None

        desktop = Atspi.get_desktop(0)
        for i in range(desktop.get_child_count()):
            app = desktop.get_child_at_index(i)
            if app:
                result = find_caret(app)
                if result:
                    return result
    except:
        pass
    return None

def get_focused_window_position():
    """Fallback: get focused window position from Hyprland."""
    try:
        result = subprocess.run(['hyprctl', 'activewindow', '-j'],
                              capture_output=True, text=True, timeout=1)
        if result.returncode == 0:
            import json as j
            data = j.loads(result.stdout)
            # Return position inside the window (offset from top-left)
            return {"x": data["at"][0] + 100, "y": data["at"][1] + 100}
    except:
        pass
    return None

if __name__ == "__main__":
    # Try AT-SPI first (native Wayland apps)
    pos = get_caret_atspi()

    # Fallback to focused window position
    if not pos:
        pos = get_focused_window_position()

    if pos:
        print(json.dumps(pos))
    else:
        print(json.dumps({"x": -1, "y": -1}))
