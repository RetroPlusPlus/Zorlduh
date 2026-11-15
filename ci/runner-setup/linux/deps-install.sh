#!/usr/bin/env bash
# Linux runner dependency installer (Ubuntu; x64 and ARM64 alike). Run once on the runner machine as
# root. Safe to re-run: apt is idempotent.
#
# The engine builds SDL3, SameBoy and Snaggletooth from its own submodules, so no package of theirs is
# needed — only the platform headers SDL3 compiles against, the shader compiler the engine's build runs,
# and Vulkan's software driver for the tests that open a GPU device with no display.
set -euo pipefail

apt-get update -y
apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ccache \
    glslang-tools \
    libvulkan-dev \
    mesa-vulkan-drivers \
    libwayland-dev \
    libxkbcommon-dev \
    libegl-dev \
    libgles2-mesa-dev \
    libpulse-dev \
    libasound2-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxfixes-dev \
    libxi-dev \
    libxtst-dev

echo "Done. Verify: cmake --version && ninja --version && gcc --version && glslang --version"
