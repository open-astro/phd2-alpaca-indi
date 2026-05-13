# OpenAstro PHD2 Alpaca Build (Unofficial)

<img src="icons/oa512.png" alt="OpenAstro Logo" width="125">

## Overview

This repository contains an OpenAstro-maintained build derived from PHD2 that has ASCOM Alpaca Support. It is intended for use with [**AlpacaBridge**](https://github.com/open-astro/AlpacaBridge) and related Alpaca-based workflows.

## Important Notice

- This is **not** an official PHD2 release.
- It is a **private build** supported by OpenAstro, not by the PHD2 developers or community.
- Support requests should be directed to OpenAstro.

## Scope

- ASCOM Alpaca support for cameras, mounts, and rotators
- INDI support (camera, mount, rotator) for users running Linux/Pi rigs or remote INDI servers
- Designed to work alongside AlpacaBridge

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

PHD2 requires INDI 2.0+. On systems with only INDI 1.9.x (Debian trixie, Ubuntu 22.04 stock, current Pi OS), add the indilib PPA before installing `libindi-dev`:

```bash
sudo apt-add-repository ppa:mutlaqja/ppa
sudo apt update
sudo apt install libindi-dev
```

### macOS

Not yet supported by this fork. The upstream `build/build-mac` script may work but is unverified after the 1.2.0 slim refactor; macOS support will be revisited in a later release.

## License

This project remains under the original PHD2 licensing terms. See [LICENSE.txt](LICENSE.txt) for details.
