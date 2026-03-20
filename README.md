# Bonito nRF52840 Zigbee

Firmware Zigbee para una placa `Bonito` / `nice!nano`-compatible basada en `nRF52840`, con bootloader `UF2` y builds locales para:

- una luz Zigbee simple `On/Off`
- un sensor Zigbee de temperatura y humedad por I2C

La base actual usa una board local `nice_nano_v2/nrf52840/uf2`, con flasheo por `UF2` y soporte opcional para `Black Magic Probe`.

## Hardware objetivo

- `nRF52840` tipo `Bonito` / `nice!nano` clone
- bootloader `UF2`
- coordinador Zigbee USB en Home Assistant (`ZHA` o `Zigbee2MQTT`)

Pinout relevante de la board actual:

- LED onboard: `P0.15`
- I2C0 SDA: `P0.17` (`D2`)
- I2C0 SCL: `P0.20` (`D3`)
- `EXT_POWER`: `P0.13` (`HIGH` enciende `VCC`, `LOW` lo corta)

Board local:

- [nice_nano_v2_nrf52840_uf2.dts](/home/lego/sensores/zigbee/boards/others/nice_nano_v2/nice_nano_v2_nrf52840_uf2.dts)

## Apps incluidas

### Luz Zigbee

Código:

- [app/light_bulb_local](/home/lego/sensores/zigbee/app/light_bulb_local)

Build:

```bash
cd /home/lego/sensores/zigbee
bash ./build-light-bulb.sh
```

Artefacto principal:

- [build/light_bulb/zephyr/zephyr.uf2](/home/lego/sensores/zigbee/build/light_bulb/zephyr/zephyr.uf2)

### Sensor Zigbee de temperatura y humedad

Código:

- [app/temp_sensor_local](/home/lego/sensores/zigbee/app/temp_sensor_local)

El sensor probado actualmente es `HDC1080` por I2C en `0x40`, con temperatura y humedad expuestas por Zigbee.

Overlay:

- [temp_sensor_nice_nano.overlay](/home/lego/sensores/zigbee/overlays/temp_sensor_nice_nano.overlay)

Build normal:

```bash
cd /home/lego/sensores/zigbee
bash ./build-temp-sensor.sh
```

Build de prueba a 30 s:

```bash
cd /home/lego/sensores/zigbee
bash ./build-temp-sensor-debug30.sh
```

Build sleepy de pruebas:

```bash
cd /home/lego/sensores/zigbee
bash ./build-temp-sensor-sleepy.sh
```

Artefacto principal de la build `debug30`:

- [build/temp_sensor_debug30/temp_sensor_local/zephyr/zephyr.uf2](/home/lego/sensores/zigbee/build/temp_sensor_debug30/temp_sensor_local/zephyr/zephyr.uf2)

## Flasheo por UF2

1. Conecta la placa por USB.
2. Pulsa `reset` dos veces rápido.
3. Debe aparecer una unidad como `NICENANO`.
4. Copia el `.uf2` correspondiente a esa unidad.
5. La placa se reiniciará sola.

Ejemplo:

```bash
cp /home/lego/sensores/zigbee/build/temp_sensor_debug30/temp_sensor_local/zephyr/zephyr.uf2 /media/lego/NICENANO/
sync
```

## Flasheo y debug por Black Magic Probe

Script:

- [blackmagicprobe.sh](/home/lego/sensores/zigbee/blackmagicprobe.sh)

Cableado mínimo:

- `SWDIO` -> `SWDIO`
- `SWCLK` -> `SWCLK`
- `GND` -> `GND`
- `VTref` -> `3V3`

Uso:

```bash
cd /home/lego/sensores/zigbee
./blackmagicprobe.sh flash
./blackmagicprobe.sh debug
./blackmagicprobe.sh attach
```

## Consola serie

Las builds de depuración exponen `USB CDC ACM` y salen como `/dev/ttyACM*`.

Ejemplo:

```bash
screen /dev/ttyACM0 115200
```

En la app de sensor se imprimen:

- fecha/hora de compilación
- configuración de muestreo
- resultado del escaneo I2C
- lecturas de temperatura y humedad
- estado de unión Zigbee

## Home Assistant

La placa no es el coordinador. La dongle USB Zigbee de Home Assistant sigue siendo el coordinador.

ZHA:

1. `Ajustes -> Dispositivos y servicios -> Zigbee Home Automation`
2. `Añadir dispositivo`
3. Reinicia la placa o arráncala durante `permit join`

Zigbee2MQTT:

1. Activa `Permit join`
2. Reinicia la placa

## Docker

Hay scripts para compilar dentro de contenedor:

- [build-light-bulb-docker.sh](/home/lego/sensores/zigbee/build-light-bulb-docker.sh)
- [docker-ncs-shell.sh](/home/lego/sensores/zigbee/docker-ncs-shell.sh)

El workspace Docker persistente se guarda en:

- `.ncs-docker/`

## Estado actual

- luz Zigbee funcional
- sensor Zigbee `temp + humidity` funcional con `HDC1080`
- bootloader `UF2` instalado y operativo
- board local migrada a `nice_nano_v2/nrf52840/uf2`

Pendiente si se quiere versión a batería:

- quitar `USB CDC`
- bajar o eliminar logs
- sleepy end device real
- opcionalmente cortar `VCC` de sensores usando `P0.13`
