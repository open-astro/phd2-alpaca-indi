# OpenAstro PHD2 2.0.0

**Released:** 2026-05-15 • **Tag:** `v2.0.0`

First native macOS Apple Silicon build, a new unit test suite that hard-gates every packaging script, and a floor-raising across all three platforms. **Not a drop-in upgrade from 1.3.0** — read [Breaking changes](#breaking-changes) first.

## TL;DR

- **New:** native macOS arm64 build with self-contained `.dmg`, dark mode, drag-to-Applications installer
- **New:** 6 unit test executables (78+ cases) pinning JSON parser, JSON-RPC schema, discovery, guide-algorithm math
- **New:** every `.deb` / `.dmg` / `.exe` build hard-fails if any test fails. No more `--skip-tests` / `nocheck` / warn-and-continue
- **New:** safe side-by-side install with upstream PHD2 — settings, registry hive, install dirs, and package names are all OpenAstro-namespaced; 1.3.0 profiles auto-migrate on first launch
- **Dropped:** Intel Mac, pre-Tahoe macOS, 32-bit Windows
- **Changed:** macOS bundle ID `org.openphdguiding.phd2` → `net.openastro.phd2`

## Breaking changes

| What | Impact |
|---|---|
| **Intel Mac removed** | arm64-only. No universal binary. Use upstream PHD2 if you need Intel. |
| **macOS floor: 26 Tahoe** | Was 10.14. Sonoma / Sequoia users stay on 1.3.0. |
| **macOS bundle ID changed** | `org.openphdguiding.phd2` → `net.openastro.phd2`. Existing macOS-specific preferences (e.g. window positions cached by AppKit) don't auto-migrate; to keep them: `cp ~/Library/Preferences/org.openphdguiding.phd2.plist ~/Library/Preferences/net.openastro.phd2.plist`. (Profile / equipment settings auto-migrate via the wxConfig namespace move below — this row only matters if you've heavily customized macOS-native prefs.) |
| **wxConfig namespace moved to OpenAstro** | Previously OpenAstro PHD2 shared settings storage with upstream PHD2 (`Software\StarkLabs\PHDGuidingV2` on Windows, `~/.PHDGuidingV2` on Linux/macOS). 2.0.0 moves to `Software\OpenAstro\OpenAstroPHD2` / `~/.OpenAstroPHD2` so both apps can be installed side-by-side without stepping on each other (and so uninstalling OpenAstro PHD2 no longer wipes upstream's HKCU key). **Existing OpenAstro PHD2 1.3.0 profiles, calibration, and equipment settings auto-migrate on first 2.0.0 launch** — the legacy store is read-only during migration, so upstream PHD2's settings (if also installed) are preserved. Dark library (`Documents/PHD2`) and defect maps are not migrated automatically; re-point via Manage Dark Library if needed. |
| **Linux package renamed `phd2-alpaca` → `openastro-phd2`** | Fresh install: `sudo dpkg -i openastro-phd2-2.0.0-<arch>.deb`. apt-managed upgrade from an existing `phd2-alpaca` install: `sudo apt install ./openastro-phd2-2.0.0-<arch>.deb ./phd2-alpaca-2.0.0-all.deb` — the transitional metadata package depends on the new one and lets apt retire the old name automatically (no manual `apt remove` needed). System user, install paths, and systemd unit also renamed. `Conflicts`/`Replaces` metadata blocks side-by-side installs of the real binaries. |
| **32-bit Windows removed** | x64 only. `-A Win32` is rejected at configure time. |
| **Legacy macOS guide drivers removed** | `GUIDE_GPUSB`, `GUIDE_GCUSBST4`, `GUIDE_EQUINOX`, `GUIDE_EQMAC` — none worked on modern macOS anyway. Use Alpaca/INDI guide outputs. |
| **`libusb` + `openssag` removed** | Unreachable since 1.2.0; no functional impact. |
| **Linux narrowed** | Debian 13 Trixie / Raspberry Pi OS Trixie only, amd64/arm64. |

## What's new

### macOS Apple Silicon native build

`./build-dmg.sh` produces a self-contained `openastro-phd2-2.0.0-arm64.dmg`. All Homebrew dylibs (cfitsio, wxWidgets, image libs, etc.) bundled into the `.app` so users don't need Homebrew. Dark mode follows System Settings. Drag-to-Applications install window with OpenAstro branding. **Unsigned** — first launch needs right-click → Open.

### Unit test suite + hard CI gate

| Suite | Covers |
|---|---|
| `test_json_parser` | The deserializer behind every Alpaca response and event-server RPC |
| `test_jsonrpc_schema` | Documented event-server message shapes that NINA / KStars / web UI depend on |
| `test_alpaca_schema` | Alpaca standard envelope, error extraction, camera/telescope/management API shapes, UDP discovery reply |
| `test_discovery_logic` | host:port parsing + dedupe model shared by Alpaca and INDI |
| `test_indi_discovery` | INDI subnet math: prefix clamping, loopback skip, always-probe-127.0.0.1 contract |
| `test_guide_algorithm_math` | identity / hysteresis / resistswitch result() curves, formula pinned with production line references |

Every packaging script (`build-deb.sh`, `build-dmg.sh`, `build-exe.ps1`) runs `ctest` and aborts the package build on any failure. Only kill-switch is `cmake -DPHD_BUILD_TESTS=OFF`. Run them yourself with `cd tmp && ctest --output-on-failure`.

### Other additions

- Alpaca rotator (`ROTATOR_ALPACA`) and guide-output (`GUIDE_ALPACA`) now enabled on macOS
- `/release` slash command for cutting future releases

## Improvements

- **Dialog centering on macOS** — every `wxDialog` / `wxFrame` now opens centered on its parent instead of the primary display. Multi-monitor users rejoice.
- **Threaded server discovery** — INDI/Alpaca discovery no longer freezes the UI for 2-4s (beachball on macOS).
- **No more auto-connect on dialog open** — INDI device dialogs used to block 3-6s trying to reach the server. Use the explicit Connect button.
- **Build modernization** — C++14 → C++20, vcpkg 2024.11.16 → 2026.03.18, libINDI 2.1.6 → 2.2.1.1, GoogleTest 1.14 → 1.17, wxWidgets pinned ≥ 3.2.
- **Profile wizard defaults** — Camera/Mount no longer fall through to "whatever sorts first alphabetically"; they default to `None` like the others.

## Bug fixes

- **Alpaca camera hung PHD2 on Stop during Looping** against servers that ack `AbortExposure` but never flip `imageready`. Bounded retry to 3s. ([#7](https://github.com/open-astro/phd2-alpaca-indi/issues/7))
- **INDI loopback servers invisible to Discover Servers** on same-box Pi setups. Now always probes `127.0.0.1:7624`.
- **Debian build failed** at configure (`Could NOT find GSL`) against new INDI 2.2.1.1. Pass `-DINDI_BUILD_COMMON=OFF`.
- **GPTest covariance check was flaky**. Seeded `srand(1)`, widened tolerance to absorb glibc/MSVCRT/BSD libc draw differences. ([#8](https://github.com/open-astro/phd2-alpaca-indi/issues/8))
- **`build-exe.ps1` parse failure on PowerShell 5.1** from em-dashes in UTF-8-without-BOM. Replaced with ASCII hyphens.
- **`build-exe.ps1` CTest path resolution** broken (`Get-Command` returns CommandInfo, not a string). Fixed.

## Installation

All three artifacts follow the unified `openastro-phd2-<version>-<arch>.<ext>` naming.

**Linux (Debian/Pi Trixie):**
```bash
# Fresh install (no existing phd2-alpaca):
sudo dpkg -i openastro-phd2-2.0.0-amd64.deb    # or -arm64.deb
sudo apt-get install -f

# OR, apt-managed upgrade from an existing phd2-alpaca install
# (no manual `apt remove` needed; transitional package handles it):
sudo apt install ./openastro-phd2-2.0.0-amd64.deb ./phd2-alpaca-2.0.0-all.deb
```

**macOS (Tahoe, Apple Silicon):** Open `openastro-phd2-2.0.0-arm64.dmg`, drag to Applications. First launch: right-click → Open.

**Windows (10/11 x64):** Run `openastro-phd2-2.0.0-x64.exe`. SmartScreen → "More info" → "Run anyway".

## Links

- [Full changelog](CHANGELOG.md)
- [Test architecture notes](tests/README.md)
- [Compare 1.3.0 → 2.0.0](https://github.com/open-astro/phd2-alpaca-indi/compare/...v2.0.0)

## Support

Not an official PHD2 release. Maintained by OpenAstro — [file issues here](https://github.com/open-astro/phd2-alpaca-indi/issues), not upstream. If you need vendor camera SDKs, AO, on-camera ST4, or an unsupported platform, use upstream [openphdguiding/phd2](https://github.com/OpenPHDGuiding/phd2) instead.
