# OpenAstro PHD2

<img src="icons/oa512.png" alt="OpenAstro Logo" width="125">

## Overview

This repository contains an OpenAstro-maintained build derived from PHD2 that supports **ASCOM Alpaca** and **INDI** equipment drivers only. It is intended for use with [**AlpacaBridge**](https://github.com/open-astro/AlpacaBridge) and related Alpaca-based workflows, with [**INDI**](https://indilib.org/) as the alternative path for Linux/Pi rigs and remote INDI servers.

## Important Notice

- This is **not** an official PHD2 release.
- It is a **private build** supported by OpenAstro, not by the PHD2 developers or community.
- Support requests should be directed to OpenAstro.

## Scope

This fork supports **only** ASCOM Alpaca and INDI for cameras, mounts, and rotators:

- **ASCOM Alpaca** — cameras, mounts, rotators (network-based; works on Windows, Linux, macOS, Raspberry Pi).
- **INDI** — cameras, mounts, rotators (native INDI client; ideal for Linux/Pi rigs or remote INDI servers).
- Designed to work alongside AlpacaBridge.

**Removed compared to upstream PHD2:** native ASCOM (COM/Windows-only), all vendor SDK camera backends (ZWO, QHY, SBIG, Altair, ToupTek, SVBony, PlayerOne, Moravian, etc.), adaptive optics / step-guiders, on-camera ST4, and auxiliary mounts. If you need those, use upstream OpenPHDGuiding/phd2 instead.

## Building

Every platform follows the same two-script pattern: `run_<artifact>` configures and (with `--build`) compiles for fast dev iteration; `build-<artifact>` produces the redistributable installer.

### Windows

```cmd
run_exe.bat                    REM incremental build (default)
run_exe.bat -rebuild           REM wipe tmp\ and full rebuild
run_exe.bat -launch            REM build then start phd2.exe
run_exe.bat -help              REM all options
```

First clean build takes 10–60 minutes depending on hardware — vcpkg builds OpenCV from source. Subsequent incremental builds are 1–5 minutes. Requires Visual Studio 2022 and `git` on `PATH`.

To produce a redistributable installer (`.exe`), use `build-exe.ps1`.

### Linux / Debian / Raspberry Pi

Supported: **Debian 13 Trixie** and **Raspberry Pi OS Trixie**, on amd64 or arm64. 32-bit ARM (armhf, e.g. Pi Zero / Pi 1 / Pi 2) and i386 are not supported; on a 64-bit-capable Pi (3/4/5) install the 64-bit Raspberry Pi OS.

```bash
./run_deb.sh                   # configure only
./run_deb.sh --build           # configure + parallel build
```

Then run the binary at `tmp/phd2.bin` (or via the `tmp/phd2` wrapper). To produce an installable `.deb` package instead, use `./build-deb.sh` — that's the full packaging script.

INDI 2.0+ is required. `run_deb.sh` auto-detects your system `libindi` via `pkg-config` and picks one of two paths:

- **System libindi ≥ 2.0** — the build links against it directly. Fastest incremental builds.
- **Missing or older libindi** (e.g. Pi OS stock 1.9.x) — the build automatically fetches INDI 2.2.1.1 and compiles it as a static client library. No manual setup needed; first build adds ~3–5 min for the INDI compile.

To force a specific path:

```bash
USE_SYSTEM_LIBINDI=0 ./run_deb.sh --build   # always from-source
USE_SYSTEM_LIBINDI=1 ./run_deb.sh --build   # always system (fails if < 2.0)
```

### macOS

Supported: **macOS 26 Tahoe or newer** on **Apple Silicon (arm64)**. Intel Macs and pre-Tahoe macOS are not supported.

Install build dependencies via [Homebrew](https://brew.sh):

```bash
brew install cmake wxwidgets cfitsio libnova gettext
# or, equivalently:
./build-dmg.sh --install-deps
```

Then build:

```bash
./run_dmg.sh                   # configure only
./run_dmg.sh --build           # configure + parallel build
```

The bundle is at `tmp/PHD2.app`. To produce a redistributable `.dmg`, use `./build-dmg.sh` — it builds, then recursively bundles every Homebrew dylib (cfitsio, wxWidgets, image libs, etc.) into `PHD2.app/Contents/Frameworks/` and rewrites install names so end users don't need Homebrew installed to run the app. Result: `tmp/PHD2-<version>-macOS-arm64.dmg`.

The `.dmg` is unsigned — first launch requires right-click → Open (or `xattr -dr com.apple.quarantine /Applications/PHD2.app`). This is normal for unsigned open-source apps; Apple Developer ID signing is a future addition.

## Testing

The fork ships a unit-test suite under `tests/` that runs alongside the upstream Gaussian-process tests under `contributions/MPI_IS_gaussian_process/tests/`. Tests are wired into CMake's `enable_testing()`, so once you've configured a build (any of the three `run_*` scripts above) you can run them via `ctest`:

```bash
# from the build directory (tmp/ for the run_*.sh / run_exe.bat scripts)
cd tmp
ctest --output-on-failure                  # all tests
ctest -R test_alpaca_schema -V             # one suite, verbose
ctest -L "Unit tests"                      # by label

# or build & run individual suites directly
cmake --build . --target test_json_parser
./tests/test_json_parser
```

Currently 10 test executables (4 from upstream GP + 6 added by this fork). The fork's suites are read-only and need no devices, network, or wxWidgets — they cover the JSON parser, the event-server JSON-RPC schema downstream consumers depend on (NINA / KStars / web UI), the Alpaca client/discovery JSON contracts, the INDI-specific subnet/scan math (loopback skip, prefix clamping, network/broadcast-skip, always-probe-127.0.0.1), the shared host:port parsing + dedupe model, and math-twin pinning of the simple guide algorithms (identity / hysteresis / resistswitch). See `tests/README.md` for the architectural notes and what's deferred.

If you want to disable the test build (e.g. for a packaging build), pass `-DBUILD_TESTING=OFF` to cmake.

## License

This project remains under the original PHD2 licensing terms. See [LICENSE.txt](LICENSE.txt) for details.
