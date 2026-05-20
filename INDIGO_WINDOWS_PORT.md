# INDIGO Windows Port — Handoff

Plan for taking the `add/indigo` branch the rest of the way on Windows.
This document is a working note, not a permanent doc — once Windows is
green, fold any durable notes into `README.md` and delete this file.

## Where the branch stands now

Branch `add/indigo` has the macOS/Linux story landed and green. On a
fresh checkout, `cmake -B build && make` clones INDIGO from GitHub and
builds libindigo automatically via `ExternalProject_Add` — same pattern
`thirdparty.cmake` already uses for libindi. No manual pre-build needed.

Verified locally: `OpenAstro PHD2.app/Contents/MacOS/OpenAstro PHD2`
links `libindigo.dylib` and the four new TUs (`cam_indigo.cpp.o`,
`scope_indigo.cpp.o`, `rotator_indigo.cpp.o`, `indigo_client_base.cpp.o`)
all compile clean.

Linux is **untested** as of this writing — the dev environment was macOS
arm64. The code path is identical: same `make` target produces
`libindigo.so` instead of `.dylib`, and the ExternalProject's
`CMAKE_SHARED_LIBRARY_SUFFIX` expansion handles the file extension
automatically. If Linux breaks at build time the culprit is most likely
INDIGO's autotools dependency (autoreconf/automake/autoconf needed for
its bundled libusb/libhidapi), which should already be present on any
Debian dev box but is something to check first.

## What's different about Windows

INDIGO does **not** ship a Unix-shaped `libindigo` for Windows. Instead,
its Windows tree (`indigo/indigo_windows/msvc/`) builds a separate
**client-only SDK** that produces:

- `indigo_client.dll` — the runtime
- `indigo_client.lib` — the import library you link against
- Headers under `indigo/indigo_libs/indigo/*.h` (same as Linux/macOS)

The PHD2 client code does **not** need to change. Verified by spot-checking
the exported symbols in
`indigo/indigo_windows/msvc/indigo_client/indigo_client.def` — every
function `IndigoClientBase` calls is exported:

- `indigo_start`, `indigo_stop`, `indigo_attach_client`, `indigo_detach_client`
- `indigo_connect_server`, `indigo_disconnect_server`
- `indigo_enumerate_properties`, `indigo_get_switch`, `indigo_enable_blob`
- `indigo_change_{number,switch,text}_property` (and `_1` variants)
- `indigo_device_connect`, `indigo_device_disconnect`

**One mismatch to fix in our code:** `src/indigo_client_base.cpp` calls
`indigo_change_text_property_1_raw`, which is **not** exported on Windows.
The unsuffixed `indigo_change_text_property_1` (printf-style) is exported.
Either switch the call, or do an `#ifdef _WIN32` branch. See "Code change"
below — it's a one-line edit.

## Step-by-step

### 1. Build INDIGO's Windows client SDK locally first

Before touching CMake, confirm the SDK builds on the target machine:

```powershell
cd C:\path\to\indigo\indigo_windows\msvc
.\build_x64_release.bat
```

That batch is a one-line `msbuild indigo_windows.sln -target:indigo_client
/t:build /p:Configuration=Release;Platform=x64`. Output lands in a `x64/Release/`
subdir under the solution (or wherever the .vcxproj points). Confirm you have:

- `indigo_client.dll`
- `indigo_client.lib`
- The headers tree at `indigo/indigo_libs/indigo/*.h`

If `msbuild` isn't on PATH, run from a **"x64 Native Tools Command Prompt
for VS"** — same shell PHD2's existing Windows build uses for vcpkg.

### 2. Fix the one symbol mismatch

Edit `src/indigo_client_base.cpp`. Replace:

```cpp
bool IndigoClientBase::SetText1(const char *device, const char *property, const char *item, const char *value)
{
    return indigo_change_text_property_1_raw(&m_client, device, property, item, value) == INDIGO_OK;
}
```

with the cross-platform call:

```cpp
bool IndigoClientBase::SetText1(const char *device, const char *property, const char *item, const char *value)
{
    return indigo_change_text_property_1(&m_client, device, property, item, "%s", value) == INDIGO_OK;
}
```

The `"%s"` format keeps the call safe against `%` characters embedded in
user-supplied text. Verify it still builds on macOS afterwards — the
unsuffixed function is also exported there, so this is a portable change.

### 3. Fill in the Windows branch in `thirdparty/thirdparty.cmake`

The INDIGO block in `thirdparty.cmake` already has a `WIN32` branch
that currently just prints a status message pointing at this doc:

```cmake
elseif(WIN32)
  message(STATUS "INDIGO Windows path is not yet wired — see INDIGO_WINDOWS_PORT.md")
```

Replace that branch with a Windows-flavoured `ExternalProject_Add`
mirroring the macOS/Linux one in the `else()` block immediately below.
The differences from the Unix branch:

- `BUILD_COMMAND` runs `msbuild` against `indigo_windows.sln` instead of
  `make`.
- `INSTALL_COMMAND` copies `indigo_client.lib` and `indigo_client.dll`
  out of the msbuild output dir (not `build/lib/libindigo.{a,dylib}`).
- `PHD_LINK_EXTERNAL` ends with `.../lib/indigo_client.lib`, not
  `libindigo.dylib`.

Pseudocode (verify the msbuild output path layout from Step 1 before
pinning — `Release/x64/` vs `x64/Release/` depends on the vcxproj):

```cmake
elseif(WIN32)
  include(ExternalProject)
  set(indigo_INSTALL_DIR ${CMAKE_BINARY_DIR}/libindigo)
  ExternalProject_Add(
    indigo
    GIT_REPOSITORY https://github.com/indigo-astronomy/indigo.git
    GIT_TAG 22706e492206d29dd3e6fd6fa16fad91650f7491 # match the Unix branch's pin
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND msbuild
      <SOURCE_DIR>/indigo_windows/msvc/indigo_windows.sln
      -target:indigo_client /t:build
      /p:Configuration=Release /p:Platform=x64
    INSTALL_COMMAND
      ${CMAKE_COMMAND} -E make_directory ${indigo_INSTALL_DIR}/lib
      COMMAND ${CMAKE_COMMAND} -E make_directory ${indigo_INSTALL_DIR}/bin
      COMMAND ${CMAKE_COMMAND} -E make_directory ${indigo_INSTALL_DIR}/include
      COMMAND ${CMAKE_COMMAND} -E copy
        <SOURCE_DIR>/indigo_windows/msvc/x64/Release/indigo_client.lib
        ${indigo_INSTALL_DIR}/lib/
      COMMAND ${CMAKE_COMMAND} -E copy
        <SOURCE_DIR>/indigo_windows/msvc/x64/Release/indigo_client.dll
        ${indigo_INSTALL_DIR}/bin/
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        <SOURCE_DIR>/indigo_libs/indigo
        ${indigo_INSTALL_DIR}/include/indigo
  )
  add_definitions(-DHAVE_INDIGO=1)
  include_directories(SYSTEM ${indigo_INSTALL_DIR}/include)
  list(APPEND PHD_LINK_EXTERNAL ${indigo_INSTALL_DIR}/lib/indigo_client.lib)
  list(APPEND PHD_EXTERNAL_PROJECT_DEPENDENCIES indigo)
```

Leave the `else()` branch (Unix `make`) and the `USE_SYSTEM_LIBINDIGO`
opt-in branch alone — they're already correct.

### 4. Bundle `indigo_client.dll` next to `phd2.exe`

Two places need updating:

**`build-exe.ps1`** — same level as where it copies vcpkg DLLs. The
ExternalProject installs the DLL to `${CMAKE_BINARY_DIR}/libindigo/bin/`;
the build script should `cp` it next to `phd2.exe` after the build, the
same way wxWidgets/cfitsio/curl DLLs are handled.

**`phd2.iss.in`** — Inno Setup `[Files]` section. Add a line:

```ini
Source: "{#PhdSrcDir}\indigo_client.dll"; DestDir: "{app}"; Flags: ignoreversion
```

(Match the surrounding entries' shape — same `{#PhdSrcDir}` variable
the other bundled DLLs use.)

### 5. CHANGELOG entry

Append under `## [Unreleased]` → `### Added`:

```markdown
- INDIGO Windows support. Mirrors the libindi ExternalProject pattern in
  thirdparty.cmake: clones INDIGO at configure time and msbuilds the
  indigo_client.sln to produce indigo_client.{dll,lib}. The DLL is
  bundled next to phd2.exe and installed alongside it by phd2.iss.in.
```

## Verification on Windows

1. **Configure**: `.\run_win.bat` (or whatever the project's Windows configure
   step is — check `build-exe.ps1`). Configuration should pull INDIGO via
   ExternalProject, msbuild it, and report INDIGO_FOUND or equivalent.

2. **Compile**: full PHD2 build. The four new TUs to watch:
   - `src/indigo_client_base.cpp.o`
   - `src/cam_indigo.cpp.o`
   - `src/scope_indigo.cpp.o`
   - `src/rotator_indigo.cpp.o`

   If any fails to compile, the most likely culprit is a header
   include-path issue — `<indigo/indigo_bus.h>` should resolve from
   `${indigo_INSTALL_DIR}/include/`.

3. **Link**: `phd2.exe` should reference `indigo_client.dll`. Confirm with:

   ```powershell
   dumpbin /dependents phd2.exe | findstr indigo
   ```

4. **Runtime**: launch `phd2.exe`, open New Profile Wizard. The
   Camera/Mount/Rotator dropdowns should show **"INDIGO Camera"**,
   **"INDIGO Mount"**, **"INDIGO Rotator"** entries (matching the existing
   "INDI Camera" / "INDI Mount" / "INDI Rotator" entries).

5. **End-to-end (optional)**: configure `/indigo/host`, `/indigo/port`,
   `/indigo/{camera,mount,rotator}` via the registry (`HKCU\Software\OpenAstro\OpenAstroPHD2`),
   point at a running `indigo_server` (Windows installer at
   https://github.com/indigo-astronomy/indigo/releases), and confirm Connect
   succeeds against the simulator drivers.

## Risk areas

- **MSBuild output path** — the vcxproj configuration controls whether
  output lands in `x64/Release/` or `Release/x64/`. Verify with Step 1
  before pinning in CMake.
- **Architecture mismatch** — PHD2 is x64-only on Windows (forced by
  `thirdparty.cmake`); INDIGO's `build_x64_release.bat` matches. Don't
  use `build_x86_release.bat`.
- **Runtime CRT** — INDIGO Windows uses MSVC's `/MD` (dynamic CRT). PHD2's
  vcpkg-pinned deps should match. If link fails with `LNK4098` or
  CRT-mismatch warnings, check both projects' RuntimeLibrary setting.
- **`indigo_change_text_property_1_raw` (fixed in Step 2)** — currently a
  blocker; the unsuffixed variant must be used instead.
- **Service discovery** — if a future commit adds INDIGO mDNS discovery,
  Bonjour for Windows is needed (Apple's mDNSResponder.exe). The
  `indigo_start_service_browser` symbol is exported but talks to
  `dns_sd.h` which requires that runtime. This is **not** in scope for
  the basic Windows port — handle separately if/when discovery lands.

## Drop this file when done

Once Windows builds green and the changes are committed, delete
`INDIGO_WINDOWS_PORT.md` — durable docs go in `README.md` or
`CHANGELOG.md`, not here.
