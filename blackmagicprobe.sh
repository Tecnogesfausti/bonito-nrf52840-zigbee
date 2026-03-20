#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NCS_DIR="${NCS_DIR:-/home/lego/ncs/v2.9.0}"
NCS_TOOLCHAIN_DIR="${NCS_TOOLCHAIN_DIR:-/home/lego/ncs/toolchains/b77d8c1312}"
DEFAULT_NATIVE_BUILD="$ROOT_DIR/build/light_bulb/light_bulb_local"
DEFAULT_DOCKER_BUILD="$ROOT_DIR/build-docker/light_bulb/light_bulb_local"
GDB_BIN="${GDB_BIN:-$NCS_TOOLCHAIN_DIR/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb}"
BMP_CONNECT_RST="${BMP_CONNECT_RST:-0}"

usage() {
  cat <<'EOF'
Uso:
  ./blackmagicprobe.sh flash
  ./blackmagicprobe.sh debug
  ./blackmagicprobe.sh attach

Variables opcionales:
  BUILD_DIR=/ruta/al/build                Build a usar.
  NCS_DIR=/ruta/al/workspace/ncs          Workspace desde el que lanzar west.
  GDB_BIN=/ruta/al/arm-zephyr-eabi-gdb    GDB del host.
  BMP_GDB_SERIAL=/dev/ttyACM0             Puerto GDB del Black Magic Probe.
  BMP_CONNECT_RST=1                       Usa connect-rst/connect-srst.
EOF
}

if [[ $# -ne 1 ]]; then
  usage >&2
  exit 1
fi

COMMAND="$1"
case "$COMMAND" in
  flash|debug|attach)
    ;;
  *)
    usage >&2
    exit 1
    ;;
esac

BUILD_DIR="${BUILD_DIR:-}"
if [[ -z "$BUILD_DIR" ]]; then
  if [[ -f "$DEFAULT_NATIVE_BUILD/zephyr/runners.yaml" ]]; then
    BUILD_DIR="$DEFAULT_NATIVE_BUILD"
  elif [[ -f "$DEFAULT_DOCKER_BUILD/zephyr/runners.yaml" ]]; then
    BUILD_DIR="$DEFAULT_DOCKER_BUILD"
  else
    BUILD_DIR="$DEFAULT_NATIVE_BUILD"
  fi
fi

if [[ ! -d "$NCS_DIR/.west" ]]; then
  echo "No encuentro un workspace NCS en: $NCS_DIR" >&2
  exit 1
fi

if [[ ! -f "$BUILD_DIR/zephyr/runners.yaml" ]]; then
  echo "No encuentro un build Zephyr válido en: $BUILD_DIR" >&2
  echo "Compila antes con ./build-light-bulb.sh o ./build-light-bulb-docker.sh" >&2
  exit 1
fi

if [[ ! -x "$GDB_BIN" ]]; then
  echo "No encuentro GDB ejecutable en: $GDB_BIN" >&2
  exit 1
fi

RUNNER_ARGS=()
if [[ "$BMP_CONNECT_RST" == "1" ]]; then
  RUNNER_ARGS+=(--connect-rst)
fi

echo "Black Magic Probe"
echo "  comando: $COMMAND"
echo "  build:   $BUILD_DIR"
echo "  gdb:     $GDB_BIN"
if [[ -n "${BMP_GDB_SERIAL:-}" ]]; then
  echo "  puerto:  $BMP_GDB_SERIAL"
else
  echo "  puerto:  autodetect"
fi
echo

cd "$NCS_DIR"
cmd=(west "$COMMAND" --skip-rebuild -d "$BUILD_DIR" -r blackmagicprobe --gdb "$GDB_BIN")
if (( ${#RUNNER_ARGS[@]} > 0 )); then
  cmd+=(-- "${RUNNER_ARGS[@]}")
fi
"${cmd[@]}"
