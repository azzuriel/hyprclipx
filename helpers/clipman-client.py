#!/usr/bin/env python3
"""
Clipman Client - IPC client for communicating with clipman-daemon
"""

import socket
import json
import sys

SOCKET_PATH = "/tmp/clipman.sock"


def send_command(cmd, args=None):
    """Send a command to the clipman daemon and return the response"""
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.settimeout(5.0)
            sock.connect(SOCKET_PATH)
            request = {"cmd": cmd, "args": args or {}}
            sock.send(json.dumps(request).encode('utf-8'))
            response = sock.recv(65536)
            return json.loads(response.decode('utf-8'))
    except FileNotFoundError:
        return {"status": "error", "error": "Daemon not running (socket not found)"}
    except ConnectionRefusedError:
        return {"status": "error", "error": "Daemon not responding (connection refused)"}
    except socket.timeout:
        return {"status": "error", "error": "Daemon timeout"}
    except Exception as e:
        return {"status": "error", "error": str(e)}


def print_help():
    print("""Clipman Client - Communicate with clipman-daemon

Usage:
    clipman-client.py <command> [args_json]

Commands:
    list [args]     List clipboard items
                    args: {"filter": "all|text|image|favorites", "search": "query", "limit": 50}

    paste <args>    Paste item to clipboard
                    args: {"uuid": "item-uuid"}

    favorite <args> Toggle favorite status
                    args: {"uuid": "item-uuid"}

    delete <args>   Delete item
                    args: {"uuid": "item-uuid"}

    clear           Clear all non-favorite items

    ping            Check if daemon is running

Examples:
    clipman-client.py list '{}'
    clipman-client.py list '{"filter": "favorites"}'
    clipman-client.py paste '{"uuid": "abc-123"}'
    clipman-client.py favorite '{"uuid": "abc-123"}'
    clipman-client.py ping
""")


def main():
    if len(sys.argv) < 2:
        print_help()
        sys.exit(1)

    cmd = sys.argv[1]

    if cmd in ["-h", "--help", "help"]:
        print_help()
        sys.exit(0)

    args = {}
    if len(sys.argv) > 2:
        try:
            args = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            print(json.dumps({"status": "error", "error": f"Invalid JSON args: {e}"}))
            sys.exit(1)

    result = send_command(cmd, args)
    print(json.dumps(result))

    # Exit with error code if command failed
    if result.get("status") != "ok":
        sys.exit(1)


if __name__ == "__main__":
    main()
