# herald
> Fully local, LLM-driven voice command dispatch across embedded nodes via MCP

## Overview

**herald** is a distributed voice control system where a Raspberry Pi 5 serves as the central processing node — handling speech capture, transcription, LLM inference, and MCP-based task dispatch entirely on-device. Commands are routed over MQTT to a network of ESP32 actuator nodes, which emulate robotic agents by responding to natural language instructions with real hardware actuation.

Unlike prior voice-controlled systems that rely on cloud pipelines or hardcoded command mappings, herald uses a structured tool-calling layer between the LLM and the embedded nodes, inspired by the MCP (Model Context Protocol) pattern. Each node exposes its hardware capabilities as named callable tools (e.g., `set_led`, `run_motor`), and the LLM dynamically reasons over which tools to invoke based on the spoken command — without any hardcoded routing logic. Depending on implementation progress, this may be realized as explicit MCP via the `mcp` Python library or as an equivalent custom tool-calling layer.

## Key Differentiators

- **Plug-and-play nodes** — new nodes and actuators can be added without modifying the LLM or dispatcher logic
- **Multi-node fan-out** — a single natural language command can dispatch to multiple heterogeneous nodes simultaneously
- **LLM-driven interrupts** — task interruption and deletion are handled through the same LLM reasoning pipeline, not as separate hardcoded interrupt handlers
- **Fully local inference** — Whisper (ASR) and a small LLM run entirely on the Raspberry Pi 5; nothing touches the cloud

## Architecture

```
        ┌─────────────────────────────┐
        │        Raspberry Pi 5        │
        │  Mic → Whisper → LLM → TTS  │
        │       MCP Dispatcher         │
        │       MQTT Broker            │
        └─────────────┬───────────────┘
                      │
          ┌───────────┴───────────┐
          ▼                       ▼
   [ESP32 Node 1]          [ESP32 Node 2]
   LCD + Actuators         LCD + Actuators
   (LEDs, motors, etc.)    (LEDs, motors, etc.)
```

## Hardware

| Device | Role |
|---|---|
| Raspberry Pi 5 | Central node — ASR, LLM inference, MCP dispatch, TTS, MQTT broker |
| ESP32 (Node 1) | Actuator node — receives and executes MCP commands |
| ESP32 (Node 2) | Actuator node — receives and executes MCP commands |

**Actuators (node-dependent):** LEDs, motors, LCD displays, additional peripherals from ELEGOO/SunFounder ESP32 starter kits

> Hardware configuration may shift depending on availability. The core architecture remains consistent regardless of node count.

## Repo Structure

```
herald/
├── server/          # ASR, LLM, TTS, MCP dispatcher (runs on RPi 5)
├── firmware/
│   └── node/        # ESP32 actuator node firmware
├── mcp/             # MCP tool definitions per node capability
├── docs/            # Architecture diagrams, notes
└── README.md
```

## Team

- Anthony Yalong
- Vaidehi Gohil