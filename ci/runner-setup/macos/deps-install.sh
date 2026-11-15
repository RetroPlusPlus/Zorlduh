#!/usr/bin/env bash
# macOS runner dependency installer (Apple Silicon). Run once on the runner machine. Requires Homebrew;
# the Xcode Command Line Tools provide clang.
#
# SDL3 on macOS needs no extra system libraries — the engine builds it from its own submodule. The
# engine's shaders compile at build time through glslang and spirv-cross into a Metal library, so the
# Metal toolchain has to run (`metal --version`), not only resolve.
set -euo pipefail

brew update
brew install cmake ninja ccache glslang spirv-cross

if ! xcrun metal --version >/dev/null 2>&1; then
    echo "The Metal toolchain is missing: run 'xcodebuild -downloadComponent MetalToolchain'."
    exit 1
fi

echo "Done. Verify: cmake --version && ninja --version && clang --version && glslang --version && spirv-cross --help"
