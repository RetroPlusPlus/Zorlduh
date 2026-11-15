# Continuous Integration

**Status:** In testing — five build jobs, each building the whole tree and running every test.

## Concept

Every push to a `ci/**` branch builds the project and runs its tests on five platforms, on a
self-hosted runner fleet (`.github/workflows/ci.yml`). `main` receives source only by a squash-merge
from a `ci/**` branch that has gone green, and is never built on its own: the same source would rebuild
for no new signal.

Each job builds every target — the game, every module's example program and every test binary — and
runs `ctest` over the whole build directory.

## Platforms

| Job | Runner | Toolchain |
|---|---|---|
| Linux (x64) | `beefserve` | GCC + Ninja |
| Windows (x64) | `WINDOWS-BVDA56O` | clang-cl (VS 2022, `-T ClangCL`) |
| macOS (ARM64) | `ericmacmini` | AppleClang + Ninja |
| Linux (ARM64) | `linmac-arm64` | GCC + Ninja |
| Windows (ARM64) | `WINMACARM64` | clang-cl (VS 2022, `-A ARM64 -T ClangCL`) |

The runners are the organization's, in a runner group that serves this repository. Each job is pinned
by its machine's name label as well as its platform labels, so a runner added later can never pick up
a job meant for another machine.

The two x64 jobs run in parallel. The three ARM64 jobs share one physical host — macOS on the metal,
the Linux and Windows ARM64 runners in VMs on it — so they run **in series**: macOS first, then Linux,
then Windows, the slowest, last. Each `needs:` the one before it with `if: always()`, so a failed link
still yields every other platform's result.

## What a run builds and runs

Every job configures with `-DCMAKE_BUILD_TYPE=Release` (Ninja on Linux and macOS, Visual Studio with
clang-cl on Windows), builds every target, and runs `ctest` with `--output-on-failure`. The tests that
open a GPU device run on every platform; the Linux jobs set `SDL_VIDEODRIVER=offscreen`, so SDL's video
starts with no display and the GPU device is Vulkan's software driver (lavapipe).

## Baseline

The same 334 tests are registered on every platform, and a run is green only when every job reports
**334/334/0** (passed/total/skipped).

## Design decisions

- **Trigger is `push` on `ci/**` only.** No `main` trigger and — deliberately — no
  `pull_request`-family trigger. The repository is public and the runners are personal machines, so
  anyone can open a fork pull request; with push-only triggers a fork pull request schedules nothing
  onto the fleet, and only someone who can push to the repository can start a run. Any addition of
  `pull_request` / `pull_request_target` / `workflow_run` must be weighed against that.
- **Every outside contributor's workflow run needs approval,** the repository's Actions setting, as a
  second line behind the triggers.
- **One run per branch at a time.** The workflow's `concurrency` group is the workflow and the branch,
  with `cancel-in-progress`: each runner has one workspace and one fixed build directory per platform,
  so a new push to a branch cancels the run still going on it.
- **The engine is a submodule, initialized in its own step.** The engine and its own submodules (SDL3,
  SameBoy, Snaggletooth) are public, so no credential is needed. A runner keeps its workspace between
  runs, so each job syncs the submodule's URL first and, if the update still fails, removes the
  leftover clone and fetches it again.
- **Unix jobs build in the workspace's normal `build/` directory** — the same `cmake -S . -B build` a
  developer runs — and keep it between runs, so builds are incremental. Their checkouts set
  `clean: false`, because the checkout action's default `git clean -ffdx` would delete the gitignored
  `build/` on every run. After every build, each Unix job replaces a staged copy of the **entire** build
  directory at `/tmp/zorlduh-build`, where the machine's owner can run the result without touching the
  runner service account's workspace.
- **Windows builds at `C:\zorlduh-ci-build`,** a short root path: a FetchContent `_deps` tree nested
  under the runner's workspace passes the 260-character path limit. A configure that fails on a cache
  another generator or toolset wrote removes that directory and configures again.
- **Test steps propagate failure through the log-capture pipe.** `set -o pipefail` on Linux and macOS
  and a `$LASTEXITCODE` check on Windows make a failing `ctest` fail the step — `tee` and `Tee-Object`
  alone would report green regardless.
- **Windows results file names are run-unique and carry the project's name**
  (`zorlduh-windows-test-results-<run_id>-<run_attempt>.txt`): a stale file handle from an earlier run
  can never block the write, and the machines' shared `C:\ci-output` keeps other projects' results apart.
- **Build trees are never cleaned at the end of a run.** When a run needs a clean state, the erase is
  the first step of the run that needs it, scoped as narrowly as possible.

## Runner provisioning

**One-time software install** — run the matching script once per runner:

- Linux: `ci/runner-setup/linux/deps-install.sh` (as root)
- macOS: `ci/runner-setup/macos/deps-install.sh` (Homebrew)
- Windows: `ci/runner-setup/windows/deps-install.ps1` (elevated PowerShell)

Beyond the compiler, CMake and Ninja, the build needs the engine's shader compilers — `glslang` on
Linux, `glslang` and `spirv-cross` with the Metal toolchain on macOS, `dxc` from the Windows SDK on
Windows. The Linux script also installs Mesa's Vulkan drivers, for the GPU tests run headless. The
build needs no Python.

## Reading results

Each job uploads its test output as a build artifact (`linux-test-results`, `windows-test-results`,
`macos-test-results`, `linux-arm64-test-results`, `windows-arm64-test-results`), uploaded even when the
tests fail. Green is confirmed by reading `ctest`'s own summary in that output — the pass total, the
same on every platform — not by a step's status icon.

## Open questions / future work

- A sanitizer job (address and undefined-behavior) on Linux x64.
- Uploading each platform's executable as an artifact beside its test results.
