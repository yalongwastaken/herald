import json


# Tool registry — list of all tool definitions across all nodes.
# Each entry has:
#   name        — tool name the LLM will call
#   node        — which node this tool belongs to
#   description — plain English description for the LLM
#   parameters  — dict of param_name -> {type, description}
#   topic       — MQTT topic to publish to
#
# To add a new node or tool, just add entries here.
# Nothing else in the codebase needs to change.

TOOLS = [
    # ── Node 1 (ELEGOO) ──────────────────────────────────────────────
    {
        "name": "set_relay",
        "node": "node1",
        "description": "Turn the relay on Node 1 on or off.",
        "parameters": {
            "state": {"type": "boolean", "description": "true = on, false = off"}
        },
        "topic": "herald/cmd/node1",
    },
    {
        "name": "move_servo",
        "node": "node1",
        "description": "Move the servo motor on Node 1 to a specified angle.",
        "parameters": {
            "angle": {"type": "integer", "description": "Angle in degrees (0-180)"}
        },
        "topic": "herald/cmd/node1",
    },
    {
        "name": "buzz",
        "node": "node1",
        "description": "Activate the buzzer on Node 1 for a specified duration.",
        "parameters": {
            "duration_ms": {"type": "integer", "description": "Duration in milliseconds"}
        },
        "topic": "herald/cmd/node1",
    },
    {
        "name": "set_display",
        "node": "node1",
        "description": "Show a message on the OLED display on Node 1.",
        "parameters": {
            "message": {"type": "string", "description": "Text to display (max 32 chars)"}
        },
        "topic": "herald/cmd/node1",
    },
    {
        "name": "get_temp_humidity",
        "node": "node1",
        "description": "Read temperature and humidity from the DHT11 sensor on Node 1.",
        "parameters": {},
        "topic": "herald/poll/node1/temp_humidity",
    },
    {
        "name": "stop",
        "node": "node1",
        "description": "Immediately stop all current operations on Node 1.",
        "parameters": {},
        "topic": "herald/cmd/node1",
    },

    # ── Node 2 (SunFounder) ──────────────────────────────────────────
    {
        "name": "set_rgb_strip",
        "node": "node2",
        "description": "Set the RGB LED strip color on Node 2.",
        "parameters": {
            "r": {"type": "integer", "description": "Red value (0-255)"},
            "g": {"type": "integer", "description": "Green value (0-255)"},
            "b": {"type": "integer", "description": "Blue value (0-255)"},
        },
        "topic": "herald/cmd/node2",
    },
    {
        "name": "move_servo",
        "node": "node2",
        "description": "Move the servo motor on Node 2 to a specified angle.",
        "parameters": {
            "angle": {"type": "integer", "description": "Angle in degrees (0-180)"}
        },
        "topic": "herald/cmd/node2",
    },
    {
        "name": "buzz",
        "node": "node2",
        "description": "Activate the buzzer on Node 2 for a specified duration.",
        "parameters": {
            "duration_ms": {"type": "integer", "description": "Duration in milliseconds"}
        },
        "topic": "herald/cmd/node2",
    },
    {
        "name": "set_display",
        "node": "node2",
        "description": "Show a message on the LCD 1602 display on Node 2.",
        "parameters": {
            "message": {"type": "string", "description": "Text to display (max 32 chars)"}
        },
        "topic": "herald/cmd/node2",
    },
    {
        "name": "get_distance",
        "node": "node2",
        "description": "Read distance from the HC-SR04 ultrasonic sensor on Node 2.",
        "parameters": {},
        "topic": "herald/poll/node2/distance",
    },
    {
        "name": "stop",
        "node": "node2",
        "description": "Immediately stop all current operations on Node 2.",
        "parameters": {},
        "topic": "herald/cmd/node2",
    },
]


def get_schemas_for_llm() -> str:
    """Format tool definitions as plain text for injection into the LLM system prompt."""
    lines = []
    current_node = None

    for tool in TOOLS:
        if tool["node"] != current_node:
            current_node = tool["node"]
            lines.append(f"\n{current_node} tools:")

        params = tool["parameters"]
        if params:
            param_str = ", ".join(
                f"{k}: {v['type']} ({v['description']})"
                for k, v in params.items()
            )
        else:
            param_str = "no parameters"

        lines.append(f"  - {tool['name']}({param_str}) — {tool['description']}")

    return "\n".join(lines)


def get_tool(name: str, node: str) -> dict | None:
    """Look up a tool by name and node. Returns the tool dict or None."""
    for tool in TOOLS:
        if tool["name"] == name and tool["node"] == node:
            return tool
    return None


def build_payload(tool_call: dict) -> tuple[str, str]:
    """Build MQTT topic and JSON payload from a tool call dict.
    Returns (topic, payload_json)."""
    tool = get_tool(tool_call["tool"], tool_call["node"])
    if not tool:
        raise ValueError(f"Unknown tool: {tool_call['tool']} on {tool_call['node']}")

    payload = {
        "tool": tool_call["tool"],
        "arguments": tool_call.get("arguments", {}),
    }

    return tool["topic"], json.dumps(payload)


if __name__ == "__main__":
    # Print the tool schemas as the LLM will see them
    print("=== Tool schemas for LLM ===")
    print(get_schemas_for_llm())

    # Test build_payload
    print("\n=== Payload test ===")
    test_call = {"tool": "set_relay", "node": "node1", "arguments": {"state": True}}
    topic, payload = build_payload(test_call)
    print(f"Topic:   {topic}")
    print(f"Payload: {payload}")
