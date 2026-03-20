#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${IMAGE:-ghcr.io/nrfconnect/sdk-nrf-toolchain:400c6cb4ec}"
NCS_REF="${NCS_REF:-v2.9.2}"
NCS_WS_DIR="${NCS_WS_DIR:-$ROOT_DIR/.ncs-docker/${NCS_REF}}"
CACHE_DIR="${CACHE_DIR:-$ROOT_DIR/.cache/docker-ncs}"
ZIGBEE_ADDON_REF="${ZIGBEE_ADDON_REF:-v1.3.0}"
ZIGBEE_ADDON_DIR="/ncs/addons/ncs-zigbee"
CONTAINER_HOME="/work/.cache/docker-ncs/home"
CONTAINER_TMP="/work/.cache/docker-ncs/tmp"
BOARD="${BOARD:-promicro_nrf52840/nrf52840/uf2}"
APP_DIR="/work/app/light_bulb_local"
BUILD_DIR="/work/build-docker/light_bulb"
OVERLAY_FILE="/work/overlays/light_bulb_promicro.overlay"
PM_STATIC_FILE="/work/app/light_bulb_local/pm_static.yml"

mkdir -p "$NCS_WS_DIR" "$CACHE_DIR/home" "$CACHE_DIR/tmp"

docker pull "$IMAGE"

docker run --rm -t \
  --user "$(id -u):$(id -g)" \
  --entrypoint /bin/bash \
  -e HOME="$CONTAINER_HOME" \
  -e TMPDIR="$CONTAINER_TMP" \
  -e XDG_CACHE_HOME=/work/.cache/docker-ncs \
  -v "$ROOT_DIR:/work" \
  -v "$NCS_WS_DIR:/ncs" \
  -w /ncs \
  "$IMAGE" \
  -lc '
    set -euo pipefail
    mkdir -p "$HOME" "$TMPDIR"
    if [ -d /ncs/.west ] && [ ! -f /ncs/.west/config ]; then
      rm -rf /ncs/.west
    fi
    if [ ! -d /ncs/.west ]; then
      echo "[docker-build] west init '"$NCS_REF"'"
      west init -m https://github.com/nrfconnect/sdk-nrf --mr '"$NCS_REF"' /ncs
    fi
    cd /ncs
    echo "[docker-build] west update"
    west update --narrow -o=--depth=1
    if [ ! -d '"$ZIGBEE_ADDON_DIR"'/.git ]; then
      echo "[docker-build] clone ncs-zigbee '"$ZIGBEE_ADDON_REF"'"
      mkdir -p /ncs/addons
      git clone --branch '"$ZIGBEE_ADDON_REF"' --depth 1 \
        https://github.com/nrfconnect/ncs-zigbee.git '"$ZIGBEE_ADDON_DIR"'
    fi
    echo "[docker-build] west zephyr-export"
    west zephyr-export
    echo "[docker-build] west build"
    west build -p always -b '"$BOARD"' '"$APP_DIR"' -d '"$BUILD_DIR"' -- \
      -DDTC_OVERLAY_FILE='"$OVERLAY_FILE"' \
      -DZEPHYR_EXTRA_MODULES='"$ZIGBEE_ADDON_DIR"' \
      -DPM_STATIC_YML_FILE='"$PM_STATIC_FILE"' \
      -DKCONFIG_ERROR_ON_WARNINGS=OFF
  '

echo
echo "Build Docker terminado."
echo "UF2:  $ROOT_DIR/build-docker/light_bulb/light_bulb_local/zephyr/zephyr.uf2"
echo "HEX:  $ROOT_DIR/build-docker/light_bulb/merged.hex"
echo "NCS:  $NCS_WS_DIR"
echo "Zigbee add-on: $ZIGBEE_ADDON_REF"
