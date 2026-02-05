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
        echo "Installed to /usr/lib/hyprland/plugins/"
        ;;
    *)
        cmake -DCMAKE_BUILD_TYPE=Release -B build
        cmake --build build -j$(nproc)
        echo "Built: build/hyprclipx.so"
        ;;
esac
