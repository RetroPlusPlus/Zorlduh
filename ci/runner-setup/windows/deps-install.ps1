# Windows runner dependency installer (x64 and ARM64 alike). Run once in PowerShell as Administrator.
# Installs VS 2022 Build Tools (the C++ toolchain, the Windows SDK — whose bin directory carries dxc,
# the shader compiler the engine's build runs — and the clang-cl toolset), CMake, Ninja and Git. On
# ARM64 it adds the ARM64 C++ tools.
# After running, open a NEW PowerShell window so PATH reflects the new installs, then verify.
#
# CI configures with `-T ClangCL`: SameBoy's Windows shims use #include_next, which MSVC rejects.
#Requires -RunAsAdministrator

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$components = @(
    "--add Microsoft.VisualStudio.Workload.VCTools",
    "--add Microsoft.VisualStudio.Component.Windows11SDK.22621",
    "--add Microsoft.VisualStudio.Component.VC.Llvm.Clang",
    "--add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset"
)
if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
    $components += "--add Microsoft.VisualStudio.Component.VC.Tools.ARM64"
}

Write-Host "Installing Visual Studio 2022 Build Tools (C++ toolchain, Windows SDK, clang-cl toolset)..."
winget install --id Microsoft.VisualStudio.2022.BuildTools `
    --silent --accept-source-agreements --accept-package-agreements `
    --override ("--quiet " + ($components -join " ") + " --includeRecommended")

Write-Host "Installing CMake..."
winget install --id Kitware.CMake --silent --accept-source-agreements --accept-package-agreements

Write-Host "Installing Ninja..."
winget install --id Ninja-build.Ninja --silent --accept-source-agreements --accept-package-agreements

Write-Host "Installing Git..."
winget install --id Git.Git --silent --accept-source-agreements --accept-package-agreements

Write-Host ""
Write-Host "Done. Open a NEW PowerShell window and verify:"
Write-Host "  cmake --version"
Write-Host "  ninja --version"
Write-Host "  clang-cl --version  (from a Developer PowerShell for VS 2022)"
