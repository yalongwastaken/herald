# herald — firmware

> All commands in this guide are run locally on your development machine unless stated otherwise.

The ESP32 nodes are flashed locally using ESP-IDF. Both nodes have the `herald` WiFi credentials
and MQTT broker address (`192.168.4.1:1883`) hardcoded — once flashed they connect automatically
whenever the Pi's hotspot is up. No configuration on the nodes is needed after flashing.

Before flashing, ensure the Pi's hotspot is already configured and broadcasting `herald`
(see [`server/README.md`](../server/README.md)).

## Prerequisites

- ESP-IDF v5.x installed on your local machine
- ESP32 connected to your machine via USB
- Pi hotspot up and broadcasting `herald` before powering nodes

---

## 1. Source ESP-IDF

**Run locally — required once per terminal session:**

```bash
. ~/esp/esp-idf/export.sh
```

This must be run before any `idf.py` commands.

---

## 2. Flash Node 1 (ELEGOO)

**Run locally with Node 1 connected via USB:**

```bash
cd firmware/node1
idf.py build flash monitor
```

Node 1 peripherals: SSD1306 OLED, SG90 Servo, Relay, Active Buzzer, DHT11.

---

## 3. Flash Node 2 (SunFounder)

**Run locally with Node 2 connected via USB:**

```bash
cd firmware/node2
idf.py build flash monitor
```

Node 2 peripherals: LCD 1602, SG90 Servo, WS2812B LED strip, Active Buzzer, HC-SR04.

---

## Monitoring

After flashing, `idf.py monitor` streams serial output directly from the node over USB. You should
see the node connect to the `herald` WiFi network and print its assigned IP:

```
I (4449) esp_netif_handlers: sta ip: 192.168.4.x ...
I (4559) node1: herald node1 ready
```

To monitor without reflashing:

```bash
idf.py monitor
```

To exit the monitor: `Ctrl+]`

You can monitor both nodes simultaneously by connecting both via USB and running
`idf.py monitor` in separate terminals, one per node.