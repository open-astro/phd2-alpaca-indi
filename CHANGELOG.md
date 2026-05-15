# Changelog

All notable changes to OpenAstro PHD2 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `/release` slash command that bumps `version.md`, closes `## [Unreleased]` into a numbered version section, gathers commit references via `git log`, and creates the release commit.
- macOS Apple Silicon (arm64) build support. New `run_dmg.sh` and `build-dmg.sh` at the project root mirror the Linux/Windows pattern (`run_deb.sh` + `build-deb.sh`, `run_exe.bat` + `build-exe.ps1`). `build-dmg.sh` produces a fully self-contained `PHD2-<version>-macOS-arm64.dmg`: 15 Homebrew dylibs (cfitsio, wxWidgets 3.3, image libs, pcre2, sharpyuv via @rpath) are recursively copied into `PHD2.app/Contents/Frameworks/` and their install names rewritten with `install_name_tool` so end users don't need Homebrew installed to run the app.
- macOS DMG "drag to /Applications" install window. `build-dmg.sh` now builds a writable scratch DMG, lays out an icon-view Finder window via AppleScript (700×600 logical, 128px icons), drops in an `/Applications` symlink as the drag target, and uses a custom background image at `packaging/macos/background.png` so users see the OpenAstro branding next to the install layout instead of a bare Finder window.
- macOS dark mode support. `Info.plist.in` opts in via `NSRequiresAquaSystemAppearance=false` so window chrome, dialogs, menus, and controls follow System Settings → Appearance instead of being forced into the legacy light Aqua theme.
- Alpaca rotator support on macOS (`ROTATOR_ALPACA` enabled in `rotators.h`) and Alpaca guide-output support on macOS (`GUIDE_ALPACA` added to `scopes.h`), matching the fork's "Alpaca + INDI only" philosophy.

### Changed
- `/commit` slash command now appends an entry to `## [Unreleased]` in `CHANGELOG.md` so release notes accumulate per-commit instead of being backfilled at release time.
- macOS minimum deployment target raised `10.14` → `26.0` (Tahoe). Matches the fork's pattern of dropping legacy floors and lets the build delete the now-dead "Sonoma+" conditional code in `run_cmake-osx` / `build/build-mac`. Tahoe also drops the last Intel Macs from Apple's supported list, aligning with the arm64-only target.
- macOS scripts restructured: `build/build-mac` and `run_cmake-osx` removed; `run_dmg.sh` and `build-dmg.sh` at the project root are now the entry points (mirroring `run_deb.sh` + `build-deb.sh`). `MacOSXBundleInfo.plist.in` renamed to `Info.plist.in` to match Apple's canonical bundle filename.
- macOS build wiring: `cfitsio` now links against the Homebrew dylib (the previous static `.a`-only constraint matched no Homebrew bottle); `libnova` lookup drops the hardcoded `/usr/local/lib` path (Intel Homebrew) in favor of `CMAKE_PREFIX_PATH`, so the same code path works on `/opt/homebrew` (arm64) and `/usr/local` (Intel); zlib explicitly added to the macOS link line for INDI 2.x's `basedevice.cpp` (`uncompress`); Eigen3 include path now queried from the `Eigen3::Eigen` target so Eigen 5.x (which dropped `EIGEN3_INCLUDE_DIR`) works.
- "OSX" terminology cleanup in CMake comments / section headers and script names — Apple retired the "OS X" branding in 2016. User-facing wire-protocol identifiers (`PHD_OSNAME`, update-check URL paths) deliberately left unchanged to avoid breaking update notifications for existing installs.
- macOS app bundle rebranded "PHD2" → "OpenAstro PHD2". Bundle name, identifier (`org.openphdguiding.phd2` → `net.openastro.phd2`), icon (`PHD_OSX_icon.icns` → `AppIcon.icns` with new OpenAstro raspberry artwork), and copyright string all updated. `run_phd2_macos` updated to match the new bundle ID when toggling `NSAppSleepDisabled`.
- INDI/Alpaca server discovery moved off the main thread. `INDIConfig::OnDiscover` and `AlpacaConfig::OnDiscover` now run `DiscoverServers()` via `std::async` and pump the UI event loop while waiting, instead of blocking the GUI thread for the 2–4s discovery window. On macOS that block was over the beachball threshold and the dialog visibly froze; the cursor is also forcibly held to the standard pointer each tick so wxOSX/AppKit doesn't swap it for the wait/watch cursor mid-discovery.
- INDI device setup dialogs (`CameraINDI`, `ScopeINDI`, `RotatorINDI`) no longer auto-connect to the configured server when the dialog opens. Auto-connect blocked the main thread for 3–6s waiting for a server that often wasn't running — on macOS surfaced as a beachball that users perceived as a hang. The dialog already has its own Discover Servers and Connect buttons; users now trigger the connection deliberately.

- Linux build scripts (`build-deb.sh`, `run_deb.sh`) now target Debian 13 Trixie / Raspberry Pi OS Trixie only, with fail-fast architecture checks rejecting hosts that aren't amd64 or arm64.
- Windows build modernized: C++14 → C++20, and wxWidgets minimum pinned to 3.2 (was unpinned and silently accepted 3.0). `run_win.bat` now configures with `-A x64`.
- C++20 conformance fixes across the source tree: ternary common-type ambiguities resolved in seven files (camera, gear_dialog, image_math, mount, myframe, rotator, guide_algorithm_gaussian_process); `DispatchObj` / `DispatchClass` in `comdispatch.h`/`.cpp` now take `const OLECHAR *` to satisfy `/Zc:strictStrings`; missing `#include "CVTrace.h"` added to `thirdparty/VidCapture/Source/VidCapture/CVImage.h` so the `CVAssert` macro is visible to `/permissive-`'s template-body parsing.
- About dialog refreshed: Joey Troy is the sole Project maintainer; Andy Galasso and Bruce Waddington moved to Past maintainers alongside Craig Stark and Bret McKee; `Copyright 2026 OpenAstro` added to the copyright list.
- Windows build wiring rewired for x64-only: `thirdparty.cmake` replaces the `WINDOWS_ARCH` x86/x64 detection with a hardcoded `x64` plus a configure-time `FATAL_ERROR` if `-A Win32` (or anything else) is passed; the VidCapture compile block, the x86 wxWidgets lib-dir branch, and the x86-only `msvcr120.dll` copy are all gone. `build-installer.ps1` and `build/build-win` updated for x64-only — `build/build-win`'s `-a arch` option removed since there's only one architecture. `phd2-x86.iss.in` renamed to `phd2.iss.in`.
- vcpkg pin bumped `2024.11.16` → `2026.03.18` (18 months of port updates in one shot). Notable bundled-library bumps: cfitsio `3.49` → `4.6.3` (major version), curl `8.18.0` → `8.19.0`, plus refreshed OpenCV 4.x and Eigen3. The 2026.03.18 release also carries a security fix for OpenSSL on Windows (ZDI-CAN-29616).
- libINDI ExternalProject bumped from `v2.1.6` to `v2.2.1.1` (current active 2.2 maintenance line; the 2.1 line dead-ended at v2.1.9). v2.2.1.1 is a same-day hotfix on top of v2.2.1. Verified end-to-end against the INDI-DEV OVA with a Player One camera — server discovery, device connect, and frame capture all clean. This is the line that contains the meaningful Player One driver fixes the 1.3.0 INDI-1.9-removal commit was alluding to.
- GoogleTest FetchContent pin bumped `v1.14.0` → `v1.17.0` (~33 months of test framework updates in one step). Required floor moves to C++17 (we're on C++20, so already covered). All four existing test executables (`GaussianProcessTest`, `MathToolboxTest`, `GPGuiderTest`, `GuidePerformanceTest`) build and run against the new version.

### Removed
- Ubuntu PPA guidance, wxWidgets 3.0 fallback paths, and armhf/i386 build support from the Linux build scripts.
- 32-bit Windows build target. `run_win.bat` now passes `-A x64`.
- `WinLibs/x86/` — 9 32-bit redist DLLs and legacy libraries (`msvcr120.dll` VS2013 runtime, `inpout32.dll` legacy port I/O, `wxVidCapLib_wx29`).
- `thirdparty/VidCapture/` — vendored DirectShow video-capture library (~2003), only compiled on Windows x86 and unreferenced from `src/`.
- `upload.cmd` — legacy buildbot upload script targeting openphdguiding.org's `phd2buildbot` putty session; not used by this fork's release flow.
- `upload.sh` and `build/make-release` — Linux/macOS twins of the already-removed `upload.cmd`. Same `phd2buildbot@openphdguiding.org` SFTP/buildbot infrastructure that this fork doesn't use.
- `APPLE32` branch in `cmake_modules/compiler_options.cmake` (forced i386 architecture; macOS dropped i386 in 10.15) and the QuickTime framework reference in `thirdparty/thirdparty.cmake` (32-bit-only framework removed in macOS 10.15).
- Legacy guide-output drivers on macOS: `GUIDE_GPUSB`, `GUIDE_GCUSBST4`, `GUIDE_EQUINOX`, `GUIDE_EQMAC` defines dropped from `scopes.h`. USB-direct outputs that haven't worked on modern macOS in years and aren't reachable from this fork's Alpaca + INDI guide-output story.
- `IOMainPort` / `IOMasterPort` compatibility shim in `serialport_mac.cpp`. `IOMainPort` was introduced in macOS 12 Monterey; the fork's minimum is now 26 Tahoe.
- `build/build.cfg.sample` — orphaned template referencing the long-removed `build/build-{linux,win,mac}` driver scripts.
- `thirdparty/include/libdc1394-2.2.2/config.h` — autoconf-generated header for the vendored libdc1394 build; unreachable since vendored libdc1394 was retired.
- Intel Mac support. The macOS build is now arm64-only — there is no universal binary, no Rosetta path. Intel users on this fork's Mac path are no longer supported.
- libusb dependency and the vendored `openssag` StarShoot AutoGuider driver. Neither is reachable from `src/` after the 1.2.0 native-camera-SDK cleanup; this fork's cameras come over Alpaca / INDI / ASCOM, none of which need direct USB. Drops `libusb-1.0-0-dev` and `libudev-dev` from Debian build deps, drops `USE_SYSTEM_LIBUSB` from CMake options and `run_deb.sh`, drops the macOS `libusb_openphd.dylib` framework-copy step, and removes 27 vendored files (`libusb-1.0.21.tar.bz2`, `thirdparty/include/libusb-1.0.21/`, entire `thirdparty/openssag/` tree) plus ~140 lines of CMake wiring.

### Fixed
- Dialogs and tool windows opened in random places (often the primary display) instead of next to the main PHD2 window — frustrating on multi-monitor setups. Every `wxDialog` / `wxFrame` subclass in `src/` now centers on its parent: About, Advanced/Preferences, Manual Guide, Calibration Assistant, Build Dark Library, Refine Bad-pixel Map, Connect Equipment, profile wizard + its `ConnectDialog` sub-dialog, `INDIConfig`, `AlpacaConfig`, `IndiGui`, calibration-assistant inner dialogs (Custom / Sanity / Explanation), slit-properties dialog, backlash graph, calstep dialog, calreview dialog, camera-cal import dialog, confirm dialog, manual cal dialog, star-cross test dialog, Gaussian-process expert dialog, log uploader, auto-updater, pier-flip cal tool, scope-pointing dialog, new-profile dialog. `MyFrame::PlaceWindowOnScreen()` (the helper that restores cached tool-window positions) also falls back to `CentreOnParent()` instead of primary-display center when the saved coords are off-screen.
- Profile wizard's Camera and Mount selectors fell through to whatever sorted first alphabetically (Alpaca) when no choice had been made, making the wizard look pre-selected and forcing users to click Alpaca a second time to actually open the configure flow. Both now default to `None`, matching the AuxMount / AO / Rotator / Switch defaults.
- INDI Options dialog (`IndiGui`) wedged the app when closed via the window's X button while running modal — `Show(false)` hid the dialog without ending the modal loop, leaving the app in a state where every click beeped because an invisible modal dialog was "active." Now always `EndModal(wxID_CANCEL)` in the modal case.
- INDI server discovery missed servers bound only to loopback. `INDIDiscovery::DiscoverServers()` now always probes `127.0.0.1:7624` as a unicast target. `EnumerateLocalSubnets()` skips the loopback interface by design (IFF_LOOPBACK on Linux/macOS, 127.0.0.0/8 check on Windows), so when PHD2 and the INDI server both run on the same Linux box — the typical Raspberry Pi setup — the local server was invisible to the Discover Servers button. Same shape of bug as the 1.3.0 `AlpacaDiscovery::BuildBroadcastTargets()` loopback fix.
- Stale `https://github.com/open-astro/phd2-alpaca` Homepage URLs in `debian/control` and `debian/phd2-alpaca.service` corrected to `https://github.com/open-astro/phd2-alpaca-indi` so apt's package metadata and the systemd unit's Documentation field point at the right repo.
- Visual Leak Detector link-directories path in `CMakeLists.txt` updated from `lib/win32` to `lib/Win64` so VLD actually links when present on an x64 build.
- Debian builds against the new INDI 2.2.1.1 failed at configure with `Could NOT find GSL`. INDI 2.2 introduced `INDI_BUILD_COMMON` (defaults `ON`) which pulls driver-development deps (GSL, USB1, JPEG, Nova, Iconv) that this fork's client-only build doesn't need. Pass `-DINDI_BUILD_COMMON=OFF` in the INDI `ExternalProject_Add` so the dep block is skipped.
- Stale "INDI 2.1.6" references in `build-deb.sh`, `run_deb.sh`, `debian/rules`, and `README.md` updated to "INDI 2.2.1.1" to match the actual pinned source build.
- `build-deb.sh` install instructions and `install_deps` apt-get command still listed `libusb-1.0-0-dev` and `libudev-dev` after those were removed from `debian/control`. Now match the actual build requirements.
- `debian/rules` no longer passes the obsolete `-DUSE_SYSTEM_LIBUSB=1` to `dh_auto_configure`; the option was removed from the CMake project alongside libusb itself.
- x64 architecture guard in `thirdparty/thirdparty.cmake` now also checks `CMAKE_SIZEOF_VOID_P` in addition to `CMAKE_GENERATOR_PLATFORM`, so 32-bit toolchains can't slip through when using Ninja or Unix Makefiles (where `CMAKE_GENERATOR_PLATFORM` is empty).
- Fenced code blocks in `.claude/commands/release.md` now carry language tags (`text` / `markdown`) to satisfy markdownlint MD040.
- Alpaca camera hung PHD2 when Stop was pressed during Looping against servers that ack `AbortExposure` but never flip `imageready` to true (e.g. ASCOM Remote Server forwarding a `StopExposure`-only driver). `CameraAlpaca::Capture()` now sends the abort exactly once per exposure and caps the post-abort wait at 3 s before bailing out, instead of hammering the server every poll iteration until the user force-quits. Fixes #7.
- `GPTest.drawSamples_prior_covariance_test` was flaky — the test draws 20 000 samples and compares empirical to analytical covariance within a 2e-1 tolerance, but Eigen's `setRandom()` (used by `math_tools::generate_normal_random_matrix`) reads from `std::rand()`, so run-to-run variation occasionally pushed the empirical covariance outside the band. The `GPTest` fixture constructor now seeds `std::srand(1)`, making every statistical-expectation test in the fixture reproducible across runs. Test-only change. Fixes #8.

## [1.3.0] - 2026-05-12

### Fixed
- **Alpaca discovery misses loopback-bound servers**
  - `AlpacaDiscovery::BuildBroadcastTargets()` now always probes `127.0.0.1:32227` as a unicast target so Alpaca servers bound only to loopback (e.g. ASCOM Remote Server's default "Loopback" IP setting) are discovered. Previously these were invisible because subnet broadcasts (255.255.255.255 and per-interface broadcasts) never reach loopback-only listeners — the workaround was to manually edit Remote Server to bind to the LAN IP.
  - Added Linux/macOS network-interface enumeration via `getifaddrs`. Previously the non-Windows path sent only to `255.255.255.255`, so multi-NIC Linux/Pi setups (VPN, docker, multiple LANs) missed servers on subnets the default route did not cover. Mirrors the Windows `GetAdaptersAddresses` path and the pattern in `indi_discovery.cpp`.

### Removed
- **`PHD2_ALLOW_INDI_1_9` escape hatch dropped**
  - INDI 2.0+ is now a hard requirement. Removed the option from `thirdparty/thirdparty.cmake`, the env-var plumbing in `run_deb.sh` / `build-deb.sh`, and the `PHD2_ALLOW_INDI_1_9` make variable in `debian/rules`. The Player One driver and several other INDI drivers have meaningful fixes after 1.9.x; keeping the toggle invited subtly broken builds.
  - For Debian/Pi systems that only ship INDI 1.9.x, `run_deb.sh` now auto-detects this and falls back to building INDI 2.1.6 from source — no manual PPA setup required.

### Changed
- **Auto-detect libindi version in all build paths**
  - `run_deb.sh`, `debian/rules`, and `build-deb.sh` now all probe `pkg-config --atleast-version=2.0.0 libindi`. When the system libindi is missing or < 2.0.0, the build sets `USE_SYSTEM_LIBINDI=0` so CMake fetches and builds INDI 2.1.6 from source as a static client library (no shared libs to bundle). The from-source path was already wired into `thirdparty/thirdparty.cmake`; it just wasn't reachable from any of the user-facing scripts. Override with `USE_SYSTEM_LIBINDI=0` or `=1` explicitly.
  - `build-deb.sh` no longer hard-fails on stale libindi-dev: it now prints which path will be taken (system or from-source) and proceeds. Older libindi-dev on Debian trixie / Pi OS no longer blocks `.deb` builds.
- **Alpaca port default is now `0` (unconfigured) instead of `6800`**
  - `pConfig->Profile.GetLong("/alpaca/port", ...)` now defaults to `0` in `cam_alpaca.cpp`, `scope_alpaca.cpp`, `rotator_alpaca.cpp`, `camera.cpp`, `scope.cpp`, `rotator.cpp`, and `event_server.cpp`. The previous `6800` was AlpacaBridge-specific and misleading for users running ASCOM Remote Server (default `11111`) or anything else.
  - `Connect()` "not yet configured" sentinel in the Alpaca camera/mount/rotator drivers simplified from `host == "localhost" && port == 6800 && device == 0` to just `port == 0`. The old triple-check could spuriously re-open the setup dialog for users genuinely running AlpacaBridge at `localhost:6800` with device 0.
  - `AlpacaConfig::SetSettings()` renders an unconfigured port as an empty field instead of literal `"0"`. The dialog's existing auto-discover path fires when the server list is empty, so new profiles open with empty fields and discovery immediately populates them — matching NINA's flow.

### Added
- **INDI support restored on all platforms**
  - Re-added INDI camera, mount, and rotator drivers (`cam_indi`, `scope_indi`, `rotator_indi`, `config_indi`, `indi_gui`). Available on Windows (via vcpkg-built `indiclient.lib`), macOS, and Linux. AO/stepguider INDI drivers remain removed.
  - Restored `USE_SYSTEM_LIBINDI=1` for Debian builds; INDI 2.x runtime libs bundled into `/usr/lib/phd2-alpaca`.
  - Added `libindi-dev` build dependency to `debian/control`.
- **INDI server discovery**
  - New `INDIDiscovery` class performs parallel non-blocking TCP probes on port 7624 across local /24 subnets. Total scan ~2s.
  - Added Discover Servers button, status label, and discovered-servers combobox to `INDIConfig` dialog. Auto-fires on dialog open when host is empty; picking from the list auto-fills host/port.
- **OpenAstro favicon for the embedded web portal.**
- **Developer tooling**
  - Added `.claude/commands/commit.md` slash command that runs the project's clang-format pass before staging and writes a structured commit message.

### Changed
- **Windows build**
  - Bumped pinned vcpkg to release tag `2024.11.16` (SHA-pinned). Parallelized the Windows build invocation in `run_win.bat` (renamed from `run_cmake.bat`) so multi-core machines actually use their cores. Added flags: `-rebuild`, `-config`, `-launch`. Added Linux counterpart `run_deb.sh`.
  - Enforced CRLF for `.bat` files via `.gitattributes` to keep the Windows build script intact across platforms.
- **Wrapper script and systemd service**
  - Restored `LD_LIBRARY_PATH=/usr/lib/phd2-alpaca` in `debian/phd2-alpaca.service` and `phd2.sh.in` so bundled INDI 2.x libs are discoverable at runtime.

### Fixed
- **INDI camera first-exposure disconnect**
  - `CameraINDI::CheckState()` now requires `CCD_INFO` before marking the camera ready, so `Connect()` blocks until `m_maxSize` is populated. Previously the first `Capture()` could send `FRAME` with width=0/height=0, causing the driver to disconnect mid-exposure.
- **Wizard pixel-size auto-fill for INDI cameras**
  - `CameraINDI::GetDevicePixelSize()` now waits briefly for `CCD_INFO` arrival when called immediately after connect, so the profile wizard auto-populates the pixel-size field from the camera (matching Alpaca behavior).
- **Aux-mount cleanup**
  - Collapsed `ScopeINDI`'s aux-mount conditional branches; the gear dialog no longer exposes `AuxScope()` post-slim, so the dead branch is gone.

### Commit References
- `9648537a` - Re-add INDI camera, mount, and rotator support on all platforms
- `b63a1358` - Fix clang-format wrap in scope_indi.cpp Debug.Write
- `a33ccf1c` - INDI server discovery + pixel size + first-exposure fix
- `f2bc9b8f` - Apply clang-format fixes and add /commit slash command
- `5263b10c` - Bump vcpkg to 2024.11.16 and parallelize Windows build
- `c9d57c8c` - Address CodeRabbit review: SHA pin, preserve CL flags, enforce CRLF
- `e399535d` - Add favicon using OpenAstro logo

## [1.2.0] - 2026-03-22

### Removed
- **Legacy camera drivers and SDKs**
  - Removed all non-Alpaca camera drivers and bundled third-party SDK libraries (ZWO, QHY, SBIG, Altair, ToupTek, SVBony, PlayerOne, Moravian, INDI, ASCOM, OpenCV, and remaining legacy backends).
  - Removed associated sources, headers, and SDK references from `CMakeLists.txt` and the `cameras/` tree.
- **Adaptive optics**
  - Removed adaptive optics from the gear dialog, profile wizard, and event server API (StepGuider/SXAO, `stepguider_sbigao_indi`). Stubs retained for backward compatibility.
- **Auxiliary mount and on-camera ST4 guiding**
  - Removed auxiliary mount support and on-camera ST4 guiding (`OnboardST4`, `ScopeOnCamera`, `ScopeOnStepGuider`). Guiding in this fork uses Alpaca only.
- **INDI**
  - Removed INDI support entirely (`scope_indi`, `rotator_indi`, `indi_gui`, `config_indi`).
- **Platform build wiring**
  - Removed macOS framework copy and `install_name_tool` handling for dropped SDKs.
  - Removed `/DELAYLOAD:sbigudrv.dll` from Windows link flags.

### Changed
- **Gear and UI**
  - Simplified gear dialog to camera, mount, and rotator only.
- **Formatting**
  - Applied clang-format across affected files (`event_server.cpp`, `gear_dialog.cpp`, `gear_simulator.cpp`, `profile_wizard.cpp`, `stepguiders.h`, `camera.cpp`).

## [1.1.0] - 2026-03-07

### Added
- **Headless Runtime + API Expansion**
  - Added CLI flags `--headless` and `--headless-auto-connect`.
  - Added auto-connect support for selected equipment at startup.
  - Added Linux systemd unit example: `packaging/systemd/phd2-headless.service`.
  - Added JSON-RPC methods for equipment/config selection, including INDI and Alpaca server/discovery/device-selection methods.
  - Added `get_equipment_choices`.
  - Added JSON-RPC smoke tooling and equipment choice listing scripts.
  - Added `doc/jsonrpc_api.md` API reference.
- **Embedded Web Portal (single-app mode)**
  - Added an in-process HTTP server integrated into `EventServer` lifecycle (start/stop with Server Mode).
  - Added local web access endpoint: `http://127.0.0.1:8080/` for instance 1.
  - Added multi-instance port pattern matching existing server behavior: `8080 + (instanceId - 1)`.
  - Added HTTP routing for:
    - `GET /` and `GET /index.html`
    - `GET /assets/*`
    - `GET /api/setup`
    - `GET /api/discover/alpaca`
    - `POST /api/rpc`
  - Added Tools menu entry to launch the web portal in the default browser.
  - Added platform packaging/install handling for web UI assets on Linux, Windows, and macOS.
- **Web Setup and Profile Flow**
  - Added profile-first wizard flow with create/select behavior before equipment setup.
  - Added profile management actions (select/rename/delete) and dark-file options tied to profile deletion.
  - Added end-of-wizard dark library build flow with progress reporting and completion feedback.
- **API and Discovery Support**
  - Added setup payload endpoint to return combined wizard/profile/equipment/alpaca/dark state needed by web UI.
  - Added Alpaca discovery endpoint integration for server discovery and device listing from the web portal.
  - Added HTTP-to-RPC bridge endpoint using existing JSON-RPC method handling.
- **Version Source of Truth**
  - Added root `version.md`.
  - Added generated compile-time version header (`cmake_modules/phd_version.h.in` -> generated `phd_version.h`).
  - Switched CMake/runtime and build scripts to read version from `version.md`.

### Changed
- **JSON-RPC Contract Hardening**
  - Rejected selection/config changes while equipment is connected.
  - Standardized invalid parameter handling.
  - Expanded `get_connected` coverage for aux mount/AO/rotator.
- **Web Wizard UX**
  - Updated profile-first flow, device discovery, dark-library controls, status messaging, and profile summaries to align more closely with native PHD2 behavior.
  - Updated Alpaca discovery inputs/behavior so host/port are not prefilled before discovery.
  - Updated profile and wizard cards/details styling to match AlpacaHTTP visual behavior.
  - Updated dark library exposure controls and labels to match native wizard defaults and wording.
- **Build and Packaging**
  - Updated Debian build flow to consume version from `version.md` and sync top `debian/changelog` version during configure.
  - Updated `build-deb.sh` to always sync `debian/changelog` from `version.md` + root `CHANGELOG.md` on each run, while preserving older Debian changelog entries.
  - Updated Windows installer build script to consume version from `version.md`.
- **Documentation**
  - Updated JSON-RPC + Web API reference: [doc/jsonrpc_api.md](doc/jsonrpc_api.md).
  - Updated `scripts/README_web_ui.md` to document embedded web mode and current behavior.

### API Links
- JSON-RPC and Web API reference: [doc/jsonrpc_api.md](doc/jsonrpc_api.md)
- Web portal usage/behavior notes: [scripts/README_web_ui.md](scripts/README_web_ui.md)

### Commit References
- `bf0d0349` - Add headless runtime and JSON-RPC equipment selection API
- `9789d2ec` - Expand headless JSON-RPC equipment/config API and docs

## [1.0.0] - 2026-03-04

### Added
- **Initial OpenAstro PHD2 Release**
  - First OpenAstro-branded release line.
  - Linux packaging and Alpaca Linux enablement included.

### Changed
- **Packaging/Build**
  - Updated `phd2.desktop` branding/comment for OpenAstro PHD2 with Alpaca support.
  - Fixed Alpaca build issues around JsonParser usage and include dependencies.
  - Extended `.gitignore` for Debian/CMake build artifacts.

### Commit References
- `e9125e1b` - OpenAstro branding: APPNAME, version 1.0.0, About dialog and status bar
- `4b5ab276` - Linux .deb build, Alpaca on Linux, and packaging fixes

## [2026-01-23]

### Changed
- **OpenAstro Branding and UI**
  - Applied OpenAstro Alpaca-focused branding updates, including icons, app presentation text, and related UI polish.
  - Updated main frame/about presentation and profile/wizard-facing UI elements for OpenAstro identity.

### Commit References
- `33475ebf` - OpenAstro Alpaca-only branding and UI changes
- `8474f1be` - Use OpenAstro icon for main window and disclaimer

## [2026-01-20]

### Changed
- **Alpaca Camera + Profile Flow**
  - Improved Alpaca camera handling and profile-related setup flows.
  - Updated profile wizard and gear/config dialogs for smoother Alpaca setup behavior.
  - Improved Alpaca discovery + camera selection behavior in wizard-related dialogs.

### Commit References
- `77d42cdc` - Enhance Alpaca camera handling and profile flows

## [2026-01-13]

### Added
- **Alpaca Rotator Support**
  - Added Alpaca rotator driver support and integration.

### Changed
- **Alpaca Client Reliability**
  - Improved Alpaca client/discovery behavior and related camera/mount integration reliability.
  - Expanded rotator integration path in core device selection/build registration.

### Commit References
- `a3dee229` - Add Alpaca rotator support and improve Alpaca client reliability

## [2025-11-18]

### Added
- **ASCOM Local Alpaca Support**
  - Added local Alpaca support path for cameras and mounts.
  - Added Alpaca discovery module integration (`alpaca_discovery`) used by selection/configuration flow.

### Commit References
- `254c95fa` - Add ALPACA (ASCOM Local Alpaca) support for cameras and mounts

## [2025-11-16]

### Added
- **Initial ASCOM Alpaca Driver Support**
  - Added initial Alpaca camera and mount driver integration for OpenAstro fork work.
  - Added initial Alpaca client/configuration plumbing and platform build wiring.

### Commit References
- `a0900cd6` - Add Alpaca (ASCOM Alpaca) driver support for cameras and mounts
