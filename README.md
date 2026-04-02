# herald
> Distributed voice-command system with LLM-based task dispatch to embedded nodes

## Overview

**herald** is a distributed voice control system where a central LLM server receives spoken commands, processes them, and dispatches structured actions to a network of embedded nodes via MCP (Model Context Protocol). MCP is used here as a structured tool-calling layer — the LLM outputs commands that map to hardware capabilities exposed by each node, rather than issuing raw instructions directly. This allows the LLM to route commands dynamically without hardcoded logic.

The system supports natural language task management including task interruption and deletion mid-execution. Node count and hardware configuration may vary depending on availability.

## Architecture

```
[Mic Node (ESP32)] ──audio──▶ [LLM Server]
                                    │
                              ASR → LLM → TTS
                              MCP Dispatcher
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
              [ESP32 Mic Node] [ESP32 Node 2]   [RPi 5 Node]
              LCD + Speaker    LCD + Speaker    LCD + Speaker
              + Actuators      + Actuators      + Actuators
```

The mic node functions as both the primary voice input/output interface and a standard MCP node — it can receive and execute dispatched commands like any other node.

## Features

- **Voice-driven task dispatch** — spoken commands are transcribed, reasoned over, and routed to the appropriate node(s)
- **MCP tool-calling layer** — each node exposes its hardware capabilities as MCP tools (`set_led`, `run_motor`, etc.), which the LLM calls by name
- **Multi-node fan-out** — a single command can target multiple nodes simultaneously
- **LCD feedback** — LLM responses are written to each relevant node's display
- **TTS announcement** — spoken responses are output via the mic node's speaker
- **Task management** — supports task interruption and deletion via voice mid-execution

## Hardware

> Node configuration may shift slightly depending on hardware availability. The core architecture remains consistent regardless.

| Node | Hardware | Role |
|---|---|---|
| Mic Node | ESP32 | Voice input, speaker output, MCP node |
| Node 2 | ESP32 | MCP node, LCD, actuators |
| Node 3 | Raspberry Pi 5 | MCP node, LCD, actuators |

**Actuators (node-dependent):** LEDs, motors, additional peripherals from ELEGOO/SunFounder ESP32 starter kits

## Repo Structure

```
herald/
├── server/          # LLM server: ASR, LLM, TTS, MCP dispatcher
├── firmware/
│   ├── mic_node/    # ESP32 mic node firmware
│   └── node/        # Generic node firmware (ESP32 / RPi)
├── mcp/             # MCP tool definitions per capability
├── docs/            # Architecture diagrams, notes
└── README.md
```

## Contributers

- Anthony Yalong
- Vaidehi Gohil