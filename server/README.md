# herald — Setup & Access Guide

## Connecting to the Pi

### Step 1 — Connect to herald WiFi
- **SSID:** `herald`
- **Password:** `herald1234`

> Make sure you're disconnected from any other network first.

---

### Step 2 — SSH into the Pi

**Option A — via herald WiFi (normal):**
```bash
ssh herald@192.168.4.1
```

**Option B — via ethernet cable (fallback):**
If herald WiFi is not working, plug an ethernet cable directly between your Mac and the Pi, then:
```bash
ssh herald@herald.local
```

Password: (whatever was set during OS setup)

---

## Checking if herald is running

### Check service status
```bash
sudo systemctl status herald
```

You should see `Active: active (running)`.

---

### Watch live logs
```bash
sudo journalctl -u herald -f
```

You'll see output like:
```
[Herald] Ready. Press button to speak.
[ASR] Waiting for button press...
[ASR] Button pressed — recording started.
[ASR] Transcript: turn on the relay
[LLM] Inferring: turn on the relay
[LLM] Raw response: {"tool": "set_relay", "node": "node1", "arguments": {"state": true}}
[Dispatcher] Published to herald/cmd/node1: {"tool": "set_relay", "arguments": {"state": true}}
[TTS] Speaking: Relay on node1 turned on.
```

Press `Ctrl+C` to stop watching logs.

---

### Start / stop / restart the service
```bash
sudo systemctl start herald
sudo systemctl stop herald
sudo systemctl restart herald
```

---

## Verifying MQTT is working

### Subscribe to all herald topics (open one terminal)
```bash
mosquitto_sub -h 192.168.4.1 -t "herald/#" -v
```

### Publish a test command (open another terminal)
```bash
mosquitto_pub -h 192.168.4.1 -t "herald/cmd/node1" -m '{"tool":"set_relay","arguments":{"state":true}}'
```

You should see the message appear in the subscriber terminal.

---

## MQTT Topic Schema

| Topic | Direction | Purpose |
|---|---|---|
| `herald/cmd/<node_id>` | RPi → ESP32 | Command payload |
| `herald/ack/<node_id>` | ESP32 → RPi | Execution acknowledgement |
| `herald/poll/<node_id>/<sensor>` | RPi → ESP32 | Sensor poll request |
| `herald/data/<node_id>/<sensor>` | ESP32 → RPi | Sensor reading response |

**Node IDs:** `node1`, `node2`

**Sensor suffixes:** `temp_humidity` (Node 1), `distance` (Node 2)

---

## Command Payload Format

```json
{"tool": "set_relay", "arguments": {"state": true}}
```

### Available tools

**Node 1 (ELEGOO)**
| Tool | Arguments |
|---|---|
| `set_relay` | `state: boolean` |
| `move_servo` | `angle: integer (0-180)` |
| `buzz` | `duration_ms: integer` |
| `set_display` | `message: string` |
| `get_temp_humidity` | none |
| `stop` | none |

**Node 2 (SunFounder)**
| Tool | Arguments |
|---|---|
| `set_rgb_strip` | `r, g, b: integer (0-255)` |
| `move_servo` | `angle: integer (0-180)` |
| `buzz` | `duration_ms: integer` |
| `set_display` | `message: string` |
| `get_distance` | none |
| `stop` | none |

---

## Acknowledgement Format

Your ESP32 should publish to `herald/ack/<node_id>` after executing a command:

```json
{"status": "ok", "tool": "set_relay"}
```

---

## Sensor Response Format

**temp_humidity** → publish to `herald/data/node1/temp_humidity`:
```json
{"temperature": 24.5, "humidity": 61.2}
```

**distance** → publish to `herald/data/node2/distance`:
```json
{"distance_cm": 34.7}
```

---

## Fan-out Commands

When the LLM detects a command targeting both nodes, it publishes to both topics in rapid succession:

```
herald/cmd/node1  {"tool": "move_servo", "arguments": {"angle": 90}}
herald/cmd/node2  {"tool": "move_servo", "arguments": {"angle": 90}}
```

Both ESP32s should subscribe to their own topic and act independently.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Can't see herald WiFi | Power cycle the Pi — wait 30 seconds |
| Can't SSH | Make sure you're on herald WiFi, use `ssh herald@192.168.4.1` |
| Service not running | `sudo systemctl start herald` |
| MQTT not reachable | `sudo systemctl status mosquitto` — restart if needed |
| herald service crashes | `sudo journalctl -u herald -n 50` to see error |
