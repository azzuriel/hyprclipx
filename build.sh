#!/bin/bash
set -e

cd "$(dirname "$0")"

case "$1" in
    clean)
        rm -rf build
        echo "Cleaned build directory"
        ;;
    release)
        cmake -DCMAKE_BUILD_TYPE=Release -B build
        cmake --build build -j$(nproc)
        echo "Built: build/hyprclipx.so"
        ;;
    debug)
        cmake -DCMAKE_BUILD_TYPE=Debug -B build
        cmake --build build -j$(nproc)
        echo "Built (debug): build/hyprclipx.so"
        ;;
    install)
        $0 release
        mkdir -p "$HOME/.local/bin"
        cp build/hyprclipx-ui "$HOME/.local/bin/"
        cp helpers/clipman-daemon.py "$HOME/.local/bin/"
        cp helpers/clipman-client.py "$HOME/.local/bin/"
        cp helpers/get-caret-position.py "$HOME/.local/bin/"
        mkdir -p "$HOME/.config/hyprclipx"
        mkdir -p "$HOME/.config/systemd/user"
        cp helpers/clipman.service "$HOME/.config/systemd/user/"
        systemctl --user daemon-reload
        systemctl --user enable clipman.service
        echo "Installed UI, daemon, client and service to ~/.local/bin/"
        echo "Use 'hyprpm add' + 'hyprpm enable hyprclipx' to load the plugin"
        ;;
    *)
        cmake -DCMAKE_BUILD_TYPE=Release -B build
        cmake --build build -j$(nproc)
        echo "Built: build/hyprclipx.so"
        ;;
esac
