#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GDB_BIN="${GDB_BIN:-/home/lego/ncs/toolchains/b77d8c1312/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb}"
BMP_PORT="${BMP_PORT:-/dev/serial/by-id/usb-Black_Magic_Debug_Black_Magic_Probe__ST-Link_v2__v2.0.0_93BA59B8-if00}"
BOOT_HEX="${BOOT_HEX:-$ROOT_DIR/bootloader-artifacts/nice_nano/bootloader_s140_6.1.1.hex}"

if [[ ! -f "$BOOT_HEX" ]]; then
  echo "No encuentro el bootloader en: $BOOT_HEX" >&2
  exit 1
fi

exec "$GDB_BIN" -q --batch \
  -ex "target extended-remote $BMP_PORT" \
  -ex "monitor swd_scan" \
  -ex "attach 1" \
  -ex "load $BOOT_HEX" \
  -ex "kill" \
  -ex "quit"
