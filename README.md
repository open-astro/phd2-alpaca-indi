# OpenAstro PHD2 Build (Unofficial)

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

### Windows

```cmd
run_win.bat                    REM incremental build (default)
run_win.bat -rebuild           REM wipe tmp\ and full rebuild
run_win.bat -launch            REM build then start phd2.exe
run_win.bat -help              REM all options
```

First clean build takes 10–60 minutes depending on hardware — vcpkg builds OpenCV from source. Subsequent incremental builds are 1–5 minutes. Requires Visual Studio 2022 and `git` on `PATH`.

### Linux / Debian / Ubuntu / Raspberry Pi

```bash
./run_deb.sh                   # configure only
./run_deb.sh --build           # configure + parallel build
```

Then run the binary at `tmp/phd2.bin` (or via the `tmp/phd2` wrapper). To produce an installable `.deb` package instead, use `./build-deb.sh` — that's the full packaging script.

INDI 2.0+ is required. `run_deb.sh` auto-detects your system `libindi` via `pkg-config` and picks one of two paths:

- **System libindi ≥ 2.0** (e.g. Ubuntu 24.04+ with the indilib PPA) — the build links against it directly. Fastest incremental builds.
- **Missing or older libindi** (e.g. Debian trixie stock 1.9.9, Pi OS stock) — the build automatically fetches INDI 2.2.1.1 and compiles it as a static client library. No manual setup needed; first build adds ~3–5 min for the INDI compile.

To force a specific path:

```bash
USE_SYSTEM_LIBINDI=0 ./run_deb.sh --build   # always from-source
USE_SYSTEM_LIBINDI=1 ./run_deb.sh --build   # always system (fails if < 2.0)
```

On Ubuntu, the indilib PPA gives you a current libindi without compiling: `sudo add-apt-repository ppa:mutlaqja/ppa && sudo apt update && sudo apt install libindi-dev`. The PPA is Launchpad-only and **does not** resolve on Debian — let the auto-detected from-source fallback handle it instead.

### macOS

Not yet supported by this fork. The upstream `build/build-mac` script may work but is unverified after the 1.2.0 slim refactor; macOS support will be revisited in a later release.

## License

This project remains under the original PHD2 licensing terms. See [LICENSE.txt](LICENSE.txt) for details.
