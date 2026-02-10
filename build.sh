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
        sudo cp build/hyprclipx.so /usr/lib/hyprland/plugins/
        mkdir -p "$HOME/.local/bin" && cp build/hyprclipx-ui "$HOME/.local/bin/"
        mkdir -p "$HOME/.config/iconmanager/helpers" && cp helpers/clipman-daemon.py "$HOME/.config/iconmanager/helpers/"
        echo "Installed to /usr/lib/hyprland/plugins/"
        ;;
    *)
        cmake -DCMAKE_BUILD_TYPE=Release -B build
        cmake --build build -j$(nproc)
        echo "Built: build/hyprclipx.so"
        ;;
esac
