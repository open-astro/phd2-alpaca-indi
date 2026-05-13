# Changelog

All notable changes to OpenAstro PHD2 will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - Unreleased

### Added
- **INDI support restored on all platforms**
  - Re-added INDI camera, mount, and rotator drivers (`cam_indi`, `scope_indi`, `rotator_indi`, `config_indi`, `indi_gui`). Available on Windows (via vcpkg-built `indiclient.lib`), macOS, and Linux. AO/stepguider INDI drivers remain removed.
  - Restored `USE_SYSTEM_LIBINDI=1` and `PHD2_ALLOW_INDI_1_9` toggle for Debian builds; INDI 2.x runtime libs bundled into `/usr/lib/phd2-alpaca`.
  - Added `libindi-dev` build dependency to `debian/control`.
- **INDI server discovery**
  - New `INDIDiscovery` class performs parallel non-blocking TCP probes on port 7624 across local /24 subnets. Total scan ~2s.
  - Added Discover Servers button, status label, and discovered-servers combobox to `INDIConfig` dialog. Auto-fires on dialog open when host is empty; picking from the list auto-fills host/port.
- **OpenAstro favicon for the embedded web portal.**
- **Developer tooling**
  - Added `.claude/commands/commit.md` slash command that runs the project's clang-format pass before staging and writes a structured commit message.

### Changed
- **Windows build**
  - Bumped pinned vcpkg to release tag `2024.11.16` (SHA-pinned). Parallelized the Windows build invocation in `run_cmake.bat` so multi-core machines actually use their cores.
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
