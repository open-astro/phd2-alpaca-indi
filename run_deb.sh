#!/usr/bin/env bash
#
# Linux counterpart to run_win.bat — wipes the tmp/ build dir, configures
# CMake with the same flags the Debian package uses, and sets parallel-build
# env vars. Run `make -j$(nproc)` afterward, or pass --build to do it here.
#
# For producing an installable .deb, see build-deb.sh (the packaging script).
# macOS is not yet supported by this script; use build/build-mac for now.
#
# Optional env vars:
#   USE_SYSTEM_LIBINDI=0    fetch and build INDI 2.1.6 from source instead of
#                           using the distro's libindi-dev. Useful on Debian
#                           trixie / Pi OS where only INDI 1.9.x is packaged.
#                           Default: auto (uses system if pkg-config reports
#                           libindi >= 2.0.0, otherwise fetches from source).
#   OPENSOURCE_ONLY=0       include proprietary camera SDKs (default: 1, none)
#   JOBS=N                  parallelism (default: detected cores)
#
# Usage:
#   ./run_deb.sh             # configure only
#   ./run_deb.sh --build     # configure + parallel build
#

set -e

cd "$(dirname "$0")"

# Suggest deps once if a likely toolchain piece is missing. Don't try to install
# anything — distros vary too much.
for tool in cmake pkg-config make g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        cat >&2 <<EOF
Missing build tool: $tool

On Debian/Ubuntu:
  sudo apt-get install build-essential git cmake pkg-config libwxgtk3.2-dev \\
      wx-common wx3.2-i18n libnova-dev gettext zlib1g-dev libx11-dev \\
      libcurl4-gnutls-dev libopencv-dev libeigen3-dev libgtest-dev

INDI 2.0+ is required. The build will auto-detect your system libindi via
pkg-config; if it's missing or < 2.0.0, INDI 2.1.6 is fetched and built from
source automatically (no libindi-dev needed). To force the system package
instead, install libindi-dev >= 2.0.0:
  - Ubuntu: sudo add-apt-repository ppa:mutlaqja/ppa && sudo apt update
            && sudo apt install libindi-dev
  - Debian: the PPA is Ubuntu-only; let the build fetch INDI from source,
            or install libindi-dev from indilib's source build.
EOF
        exit 1
    fi
done

# Use all cores by default. Override with JOBS=N if you need to throttle.
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
export CMAKE_BUILD_PARALLEL_LEVEL=$JOBS

# Defaults match debian/rules so a local build matches the .deb package.
OPENSOURCE_ONLY=${OPENSOURCE_ONLY:-1}

# Auto-detect: prefer the system libindi if it's >= 2.0.0, otherwise fall back
# to building INDI 2.1.6 from source. Override explicitly with USE_SYSTEM_LIBINDI=0/1.
if [ -z "${USE_SYSTEM_LIBINDI:-}" ]; then
    if command -v pkg-config >/dev/null 2>&1 && \
       pkg-config --atleast-version=2.0.0 libindi 2>/dev/null; then
        USE_SYSTEM_LIBINDI=1
    else
        sys_ver=$(pkg-config --modversion libindi 2>/dev/null || echo "not found")
        echo "System libindi: $sys_ver (need >= 2.0.0); fetching INDI from source."
        USE_SYSTEM_LIBINDI=0
    fi
fi

CMAKE_FLAGS=(
    -Wno-dev
    "-DUSE_SYSTEM_LIBINDI=$USE_SYSTEM_LIBINDI"
    "-DUSE_SYSTEM_GTEST=1"
    "-DUSE_SYSTEM_LIBUSB=1"
    "-DOPENSOURCE_ONLY=$OPENSOURCE_ONLY"
)

rm -rf tmp
mkdir tmp
cd tmp

echo "Configuring with: cmake ${CMAKE_FLAGS[*]} .."
cmake "${CMAKE_FLAGS[@]}" ..

cd ..

case "${1:-}" in
    --build|-b)
        echo "Building with $JOBS parallel jobs..."
        cmake --build tmp --parallel "$JOBS"
        echo
        echo "Done. Binary: tmp/phd2.bin (or run via tmp/phd2 wrapper)"
        ;;
    "")
        echo
        echo "Configure complete. To build:"
        echo "  cd tmp && make -j$JOBS"
        echo "Or rerun with --build to build now."
        ;;
    *)
        echo "Unknown option: $1" >&2
        echo "Usage: $0 [--build]" >&2
        exit 1
        ;;
esac
