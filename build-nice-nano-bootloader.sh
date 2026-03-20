#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTLOADER_DIR="$ROOT_DIR/tools/adafruit-nrf52-bootloader"
BOARD="${BOARD:-nice_nano}"
BUILD_DIR="$BOOTLOADER_DIR/_build/build-$BOARD"
ARTIFACT_DIR="$ROOT_DIR/bootloader-artifacts/$BOARD"

latest_file() {
  local pattern="$1"
  ls -1t $pattern 2>/dev/null | head -n 1
}

if [[ ! -d "$BOOTLOADER_DIR" ]]; then
  echo "No encuentro el repo del bootloader en: $BOOTLOADER_DIR" >&2
  exit 1
fi

make -C "$BOOTLOADER_DIR" BOARD="$BOARD" PYTHON=python3 NRFUTIL=true all

mkdir -p "$ARTIFACT_DIR"
cp "$(latest_file "$BUILD_DIR"/${BOARD}_bootloader-*.out)" "$ARTIFACT_DIR/bootloader.out"
cp "$(latest_file "$BUILD_DIR"/${BOARD}_bootloader-*_nosd.hex)" "$ARTIFACT_DIR/bootloader_nosd.hex"
cp "$(latest_file "$BUILD_DIR"/update-${BOARD}_bootloader-*_nosd.uf2)" "$ARTIFACT_DIR/bootloader_update.uf2"
cp "$(latest_file "$BUILD_DIR"/${BOARD}_bootloader-*_s140_6.1.1.hex)" "$ARTIFACT_DIR/bootloader_s140_6.1.1.hex"

echo
echo "Build terminado."
echo "OUT:   $ARTIFACT_DIR/bootloader.out"
echo "HEX:   $ARTIFACT_DIR/bootloader_s140_6.1.1.hex"
echo "NOSD:  $ARTIFACT_DIR/bootloader_nosd.hex"
echo "UF2:   $ARTIFACT_DIR/bootloader_update.uf2"
