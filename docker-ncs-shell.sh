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

mkdir -p "$NCS_WS_DIR" "$CACHE_DIR/home" "$CACHE_DIR/tmp"

docker run --rm -it \
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
      echo "[docker-ncs] west init '"$NCS_REF"'"
      west init -m https://github.com/nrfconnect/sdk-nrf --mr '"$NCS_REF"' /ncs
    fi
    cd /ncs
    echo "[docker-ncs] west update"
    west update --narrow -o=--depth=1
    if [ ! -d '"$ZIGBEE_ADDON_DIR"'/.git ]; then
      echo "[docker-ncs] clone ncs-zigbee '"$ZIGBEE_ADDON_REF"'"
      mkdir -p /ncs/addons
      git clone --branch '"$ZIGBEE_ADDON_REF"' --depth 1 \
        https://github.com/nrfconnect/ncs-zigbee.git '"$ZIGBEE_ADDON_DIR"'
    fi
    echo "[docker-ncs] west zephyr-export"
    west zephyr-export
    exec bash
  '
