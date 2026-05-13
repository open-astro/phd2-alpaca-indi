#!/usr/bin/env bash
#
# Linux counterpart to run_win.bat — wipes the tmp/ build dir, configures
# CMake with the same flags the Debian package uses, and sets parallel-build
# env vars. Run `make -j$(nproc)` afterward, or pass --build to do it here.
#
# Supported targets: Debian 13 Trixie / Raspberry Pi OS Trixie on amd64 or
# arm64. 32-bit ARM (armhf) and i386 are not supported.
#
# For producing an installable .deb, see build-deb.sh (the packaging script).
# macOS is not yet supported by this script; use build/build-mac for now.
#
# Optional env vars:
#   USE_SYSTEM_LIBINDI=0    fetch and build INDI 2.1.6 from source instead of
#                           using the distro's libindi-dev. Default: auto —
#                           uses the system package if pkg-config reports
#                           libindi >= 2.0.0 (true on Trixie), otherwise
#                           fetches from source.
#   OPENSOURCE_ONLY=0       include proprietary camera SDKs (default: 1, none)
#   JOBS=N                  parallelism (default: detected cores)
#
# Usage:
#   ./run_deb.sh             # configure only
#   ./run_deb.sh --build     # configure + parallel build
#

set -e

cd "$(dirname "$0")"

# Architecture check: amd64 and arm64 only. armhf / i386 are not supported.
HOST_ARCH=$(dpkg --print-architecture 2>/dev/null || echo unknown)
case "$HOST_ARCH" in
    amd64|arm64) ;;
    armhf|armel)
        echo "Unsupported architecture: ${HOST_ARCH}. This fork builds amd64 and arm64 only." >&2
        echo "32-bit ARM hardware (Pi Zero / Pi 1 / Pi 2) is not supported; on a 64-bit-capable" >&2
        echo "Pi (3/4/5) install the 64-bit Raspberry Pi OS instead." >&2
        exit 1
        ;;
    *)
        echo "Unsupported architecture: ${HOST_ARCH}. This fork builds amd64 and arm64 only." >&2
        exit 1
        ;;
esac

# Suggest deps once if a likely toolchain piece is missing. Don't try to install
# anything — distros vary too much.
for tool in cmake pkg-config make g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        cat >&2 <<EOF
Missing build tool: $tool

On Debian 13 Trixie / Raspberry Pi OS Trixie:
  sudo apt-get install build-essential git cmake pkg-config libwxgtk3.2-dev \\
      wx-common wx3.2-i18n libnova-dev gettext zlib1g-dev libx11-dev \\
      libcurl4-gnutls-dev libopencv-dev libeigen3-dev libgtest-dev

INDI 2.0+ is required. Trixie ships libindi-dev 2.x; if it's installed, the
build links against it. Otherwise the build fetches and compiles INDI 2.1.6
from source automatically (no manual setup needed).
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
