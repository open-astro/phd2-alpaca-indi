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
#   PHD2_ALLOW_INDI_1_9=1   build against system INDI 1.9.x (default: 0, requires 2.0+)
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
      wx-common wx3.2-i18n libindi-dev libnova-dev gettext zlib1g-dev libx11-dev \\
      libcurl4-gnutls-dev libopencv-dev libeigen3-dev libgtest-dev

If your distro only ships INDI 1.9.x and you can't add the indilib PPA, set
PHD2_ALLOW_INDI_1_9=1 before running this script.
EOF
        exit 1
    fi
done

# Use all cores by default. Override with JOBS=N if you need to throttle.
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
export CMAKE_BUILD_PARALLEL_LEVEL=$JOBS

# Defaults match debian/rules so a local build matches the .deb package.
OPENSOURCE_ONLY=${OPENSOURCE_ONLY:-1}
PHD2_ALLOW_INDI_1_9=${PHD2_ALLOW_INDI_1_9:-0}

CMAKE_FLAGS=(
    -Wno-dev
    "-DUSE_SYSTEM_LIBINDI=1"
    "-DUSE_SYSTEM_GTEST=1"
    "-DUSE_SYSTEM_LIBUSB=1"
    "-DOPENSOURCE_ONLY=$OPENSOURCE_ONLY"
)
if [ "$PHD2_ALLOW_INDI_1_9" != "0" ]; then
    CMAKE_FLAGS+=("-DPHD2_ALLOW_INDI_1_9=1")
fi

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
