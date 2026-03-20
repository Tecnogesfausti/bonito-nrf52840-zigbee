#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_DIR="${NCS_DIR:-/home/lego/ncs/v2.9.0}"
NCS_TOOLCHAIN_DIR="${NCS_TOOLCHAIN_DIR:-/home/lego/ncs/toolchains/b77d8c1312}"
SAMPLE_DIR="$ROOT_DIR/app/temp_sensor_local"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/temp_sensor}"
BOARD="${BOARD:-nice_nano_v2/nrf52840/uf2}"
OVERLAY_FILE="$ROOT_DIR/overlays/temp_sensor_nice_nano.overlay"
PYTHON_BIN="${PYTHON_BIN:-/usr/bin/python3}"
CACHE_DIR="${CACHE_DIR:-$ROOT_DIR/.cache}"

if [[ ! -d "$SAMPLE_DIR" ]]; then
  echo "No encuentro la app Zigbee en: $SAMPLE_DIR" >&2
  exit 1
fi

if [[ ! -d "$NCS_DIR/zephyr" ]]; then
  echo "No encuentro Zephyr dentro de: $NCS_DIR" >&2
  exit 1
fi

if [[ -d "$NCS_TOOLCHAIN_DIR/usr/local/lib" ]]; then
  export LD_LIBRARY_PATH="$NCS_TOOLCHAIN_DIR/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [[ -d "$NCS_TOOLCHAIN_DIR/usr/local/bin" ]]; then
  export PATH="$NCS_TOOLCHAIN_DIR/usr/local/bin:$NCS_TOOLCHAIN_DIR/bin:$NCS_TOOLCHAIN_DIR/opt/bin:$PATH"
fi
mkdir -p "$CACHE_DIR"
export XDG_CACHE_HOME="$CACHE_DIR"

cd "$NCS_DIR"
west build -p always -b "$BOARD" "$SAMPLE_DIR" -d "$BUILD_DIR" -- \
  -DBOARD_ROOT="$ROOT_DIR" \
  -DDTC_OVERLAY_FILE="$OVERLAY_FILE" \
  -DPython3_EXECUTABLE="$PYTHON_BIN"

echo
echo "Build terminado."
echo "UF2:  $BUILD_DIR/temp_sensor_local/zephyr/zephyr.uf2"
echo "HEX:  $BUILD_DIR/merged.hex"
