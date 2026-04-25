# herald

> Fully local, LLM-driven voice command dispatch across embedded nodes via MCP-inspired tool calling

## Overview

**herald** is a distributed voice control system where a Raspberry Pi 5 serves as the central
processing node — handling speech capture, transcription, LLM inference, and structured task
dispatch entirely on-device. Commands are routed over MQTT to a network of ESP32 actuator nodes,
which respond to natural language instructions with real hardware actuation.

Unlike prior voice-controlled systems that rely on cloud pipelines or hardcoded command mappings,
herald uses an MCP-inspired tool-calling layer between the LLM and the embedded nodes. Each node
exposes its hardware capabilities as named callable tools (e.g., `set_relay`, `move_servo`), and
the LLM dynamically reasons over which tools to invoke and on which node — without any hardcoded
routing logic.

## Key Features

- **Push-to-talk input** — a physical button on the RPi 5 GPIO triggers recording; release sends
  the audio through the pipeline
- **Natural language dispatch** — the LLM selects tools and target nodes from the spoken command;
  no hardcoded command mappings
- **MCP-inspired tool calling** — nodes expose hardware capabilities as named tools with typed
  parameters; the LLM returns structured JSON tool calls
- **Multi-node fan-out** — a single voice command can dispatch to multiple nodes simultaneously
  via parallel MQTT publish
- **Plug-and-play nodes** — adding a new node requires only registering its tool schemas; the LLM
  and dispatcher require no changes
- **LLM-driven stop** — "stop" is a high-priority tool call that bypasses the command queue and
  halts the current task immediately
- **Sensor polling** — the RPi queries node-side sensors on demand; results are announced via TTS
- **Fully local inference** — Whisper, the LLM, and Kokoro TTS all run on the RPi 5; no cloud
  APIs used at any point
- **Self-contained networking** — RPi 5 acts as a WiFi hotspot; no dependency on external
  infrastructure

## Architecture

```
        ┌──────────────────────────────────────────┐
        │               Raspberry Pi 5              │
        │                                           │
        │  [Button] → record → Whisper (ASR)        │
        │                    → LLM + Tool Dispatch  │
        │                    → Kokoro (TTS)         │
        │                    → [USB Speaker]        │
        │                                           │
        │  Mosquitto (MQTT Broker)                  │
        │  WiFi Hotspot                             │
        └──────────────┬────────────────────────────┘
                       │  WiFi / MQTT
           ┌───────────┴───────────┐
           ▼                       ▼
    ┌──────────────┐       ┌──────────────┐
    │ ESP32 Node 1 │       │ ESP32 Node 2 │
    │ ELEGOO kit   │       │ SunFounder   │
    │              │       │              │
    │ SSD1306 OLED │       │ LCD 1602     │
    │ SG90 Servo   │       │ SG90 Servo   │
    │ Relay Module │       │ WS2812B LEDs │
    │ Active Buzzer│       │ Active Buzzer│
    │ DHT11        │       │ HC-SR04      │
    └──────────────┘       └──────────────┘
```

The RPi 5 is the sole intelligence layer. ESP32 nodes maintain no reasoning state — they parse a
JSON payload, route to a driver function, and actuate. The RPi acts as both the MQTT broker and
WiFi hotspot, making the system fully self-contained.

## Hardware

| Device | Role | Key Peripherals |
|---|---|---|
| Raspberry Pi 5 (4GB) | Central node | SunFounder USB Mic, HONKYOB USB Speaker, push-to-talk button (GPIO) |
| ESP32 Node 1 (ELEGOO) | Actuator node | SSD1306 OLED (I2C), SG90 Servo (PWM), Relay (GPIO), Active Buzzer (GPIO), DHT11 |
| ESP32 Node 2 (SunFounder) | Actuator node | LCD 1602 (I2C), SG90 Servo (PWM), WS2812B Strip (one-wire), Active Buzzer (GPIO), HC-SR04 |

> Hardware configuration may shift depending on availability. The architecture is node-agnostic at
> the dispatch layer — adding or removing nodes requires no changes to the LLM or dispatcher.

## Software Stack

| Layer | Technology | Notes |
|---|---|---|
| ASR | OpenAI Whisper (`base.en`) | Local; model size flexible based on latency constraints |
| LLM | TBD via llama.cpp | Quantized GGUF, instruction-tuned; must fit ~3GB alongside Whisper + Kokoro |
| TTS | Kokoro | Local synthesis, USB speaker playback |
| Tool Dispatch | Python (MCP-inspired) | LLM output parsed into JSON tool calls, dispatched via MQTT |
| MQTT Broker | Mosquitto | Runs on RPi 5 |
| Networking | RPi 5 as WiFi hotspot | ESP32s connect directly; no external network dependency |
| Firmware | ESP-IDF (C) | Both ESP32 nodes |
| Boot | systemd service | herald auto-starts on RPi 5 power-on; headless operation |

## Tool Definitions

All tools are defined as JSON schemas in `server/tools.py` and provided to the LLM at inference
time. The LLM returns a flat JSON object; the dispatcher validates and publishes it to the
appropriate MQTT topic.

| Tool | Node | Parameters | Hardware |
|---|---|---|---|
| `set_display` | Any | `text: str` | OLED (Node 1) / LCD (Node 2) |
| `move_servo` | Any | `angle: int` (0–180) | SG90 |
| `buzz` | Any | `duration_ms: int` | Active Buzzer |
| `stop` | Any | _(none)_ | Bypasses queue, halts current task immediately |
| `set_relay` | Node 1 only | `state: bool` | SRD-05VDC Relay |
| `get_temp_humidity` | Node 1 only | _(none)_ | DHT11 |
| `set_rgb_strip` | Node 2 only | `pattern: str`, `color: str` | WS2812B |
| `get_distance` | Node 2 only | _(none)_ | HC-SR04 |

### Dispatch payload format

```json
{"tool": "set_relay", "arguments": {"state": true}}
```

## MQTT Topic Structure

```
herald/
├── cmd/node1                    RPi → Node 1    JSON tool call
├── cmd/node2                    RPi → Node 2    JSON tool call
├── ack/node1                    Node 1 → RPi    Execution ack
├── ack/node2                    Node 2 → RPi    Execution ack
├── poll/node1/temp_humidity     RPi → Node 1    Sensor poll request
├── poll/node2/distance          RPi → Node 2    Sensor poll request
├── data/node1/temp_humidity     Node 1 → RPi    Sensor reading
└── data/node2/distance          Node 2 → RPi    Sensor reading
```

## Repo Structure

```
herald/
├── docs/
│   └── herald_architecture.pdf   Full system design and data flow
├── firmware/
│   ├── node1/                    ELEGOO ESP32 firmware (ESP-IDF)
│   └── node2/                    SunFounder ESP32 firmware (ESP-IDF)
├── server/
│   ├── main.py                   Pipeline entrypoint
│   ├── asr.py                    Whisper capture + transcription
│   ├── llm.py                    LLM inference + tool call parsing
│   ├── tts.py                    Kokoro synthesis + playback
│   ├── dispatcher.py             Tool call → MQTT publish
│   └── tools.py                  Tool schemas + handler registry
├── tests/
│   └── server/
└── README.md
```

## Demo Scenarios

| Voice Command | Expected Behavior |
|---|---|
| "Turn on the relay" | Node 1 relay activates; OLED updates; TTS confirms |
| "Set the LED strip to red" | Node 2 WS2812B turns red; LCD updates; TTS confirms |
| "Move the servo on both nodes to 90 degrees" | Fan-out: both nodes actuate simultaneously |
| "What's the temperature?" | Node 1 DHT11 polled; reading announced via TTS |
| "How far is the nearest object?" | Node 2 HC-SR04 polled; distance announced via TTS |
| "Stop" | High-priority stop dispatched; current task halts immediately |

## Team

Anthony Yalong & Vaidehi Gohil