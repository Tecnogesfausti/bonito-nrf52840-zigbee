# Zigbee `light_bulb` para Bonito Nano V2.0 / nRF52840

Este repo prepara un build local simplificado del sample Zigbee `light_bulb` para una placa estilo `nice!nano` / `Pro Micro nRF52840`, con salida `UF2` para flasheo por USB.

## Requisitos

- SDK local en `/home/lego/ncs/v2.9.0`
- `west`, `cmake`, `arm-none-eabi-gcc`
- Una placa `Bonito Nano V2.0` con bootloader `UF2`
- Un coordinador Zigbee USB ya funcionando en Home Assistant (`ZHA` o `Zigbee2MQTT`)

## Compilar

```bash
cd /home/lego/sensores/zigbee
chmod +x build-light-bulb.sh
./build-light-bulb.sh
```

El fichero importante queda en:

```text
build/light_bulb/zephyr/zephyr.uf2
```

Los fuentes locales simplificados están en:

```text
app/light_bulb_local/
```

## Compilar con Docker

El entorno Docker usa la imagen oficial fijada al tag concreto `400c6cb4ec` y crea un workspace NCS persistente dentro del repo en:

```text
.ncs-docker/v2.9.2
```

La caché y los temporales del contenedor también quedan dentro del repo para no depender de `/tmp` del host:

```text
.cache/docker-ncs/
```

Como este workspace `v2.9.2` no trae el add-on Zigbee dentro del repo local, el script Docker clona además el add-on oficial:

```text
.ncs-docker/v2.9.2/addons/ncs-zigbee
```

```bash
cd /home/lego/sensores/zigbee
chmod +x build-light-bulb-docker.sh docker-ncs-shell.sh
./build-light-bulb-docker.sh
```

Para abrir una shell dentro del contenedor con el workspace listo:

```bash
./docker-ncs-shell.sh
```

## Flashear y depurar con Black Magic Probe

La board local ya tiene activado el runner `blackmagicprobe`, así que Zephyr puede usar BMP directamente para `flash`, `debug` y `attach`.

Cableado mínimo:

- `SWDIO` -> `SWDIO`
- `SWCLK` -> `SWCLK`
- `GND` -> `GND`
- `3V3` o `VTref` -> referencia de tensión de la placa
- `RST` -> opcional, solo si quieres usar `connect-rst`

Uso rápido:

```bash
cd /home/lego/sensores/zigbee
chmod +x blackmagicprobe.sh
./blackmagicprobe.sh flash
./blackmagicprobe.sh debug
./blackmagicprobe.sh attach
```

El script busca primero un build nativo en:

```text
build/light_bulb/light_bulb_local
```

Si no existe, usa el build Docker en:

```text
build-docker/light_bulb/light_bulb_local
```

Variables útiles:

```bash
export BMP_GDB_SERIAL=/dev/ttyACM0
BUILD_DIR=/home/lego/sensores/zigbee/build-docker/light_bulb/light_bulb_local ./blackmagicprobe.sh flash
BMP_CONNECT_RST=1 ./blackmagicprobe.sh debug
```

Notas:

- `flash` carga el firmware al objetivo usando el `zephyr.elf` del build.
- `debug` abre `gdb` ya conectado al BMP y recarga el firmware.
- `attach` abre `gdb` sin reflashear.
- El script usa `--skip-rebuild`, así que primero debes compilar con `./build-light-bulb.sh` o `./build-light-bulb-docker.sh`.

## Flashear por USB

1. Conecta la placa por USB.
2. Pulsa `reset` dos veces rápido para entrar en modo bootloader.
3. Debe aparecer un disco USB tipo `NICENANO`, `FTHR840BOOT` o similar.
4. Copia `build/light_bulb/zephyr/zephyr.uf2` a ese disco.
5. La placa se reiniciará sola.

## Emparejar con Home Assistant

### ZHA

1. Ve a `Ajustes -> Dispositivos y servicios -> Zigbee Home Automation`.
2. Pulsa `Añadir dispositivo`.
3. Enciende o reinicia la placa ya flasheada.
4. Espera a que aparezca como una luz Zigbee regulable.

### Zigbee2MQTT

1. Activa `Permit join` en Zigbee2MQTT.
2. Reinicia la placa ya flasheada.
3. Espera a que anuncie un dispositivo de tipo luz.

## Notas

- Este sample funciona como `router Zigbee`, no como coordinador.
- La dongle USB de Home Assistant sigue siendo el coordinador de la red.
- El overlay define GPIOs mínimos para que el sample no dependa de una Nordic DK clásica.
- Si tu bootloader UF2 usa un mapa distinto, el build puede arrancar mal y habría que ajustar `boards/others/promicro_nrf52840/promicro_nrf52840_flash_uf2.dtsi`.
