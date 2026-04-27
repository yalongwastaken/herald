# @file    dispatcher.py
# @author  Vaidehi Gohil
# @brief   MQTT dispatcher for herald. Routes LLM tool calls to ESP32 nodes.
#          Actuator tools speak confirmation immediately on dispatch.
#          Sensor tools speak the actual value when data arrives via MQTT callback.

import json
import time
from typing import Callable
import paho.mqtt.client as mqtt
from tools import get_tool, build_payload, get_schemas_for_llm


BROKER_HOST     = "localhost"
BROKER_PORT     = 1883
QOS_CMD         = 0    # actuator commands — lowest latency
QOS_POLL        = 1    # sensor polls — reliable delivery
SENSOR_TIMEOUT  = 5.0  # seconds to wait for sensor response


# sensor topics published by ESP32 nodes
SENSOR_TOOLS = {"get_temp_humidity", "get_distance"}


class Dispatcher:
    def __init__(
        self,
        broker_host: str = BROKER_HOST,
        broker_port: int = BROKER_PORT,
        tts_callback: Callable[[str], None] = None,
    ):
        self.broker_host  = broker_host
        self.broker_port  = broker_port
        self._tts         = tts_callback  # called with a string to speak
        self._sensor_data = {}            # topic -> payload, written by _on_message

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        print("[Dispatcher] Connecting to MQTT broker...")
        self.client.connect(broker_host, broker_port, keepalive=60)
        self.client.loop_start()
        print("[Dispatcher] Connected.")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        client.subscribe("herald/data/+/+", qos=QOS_POLL)
        print("[Dispatcher] Subscribed to sensor data topics.")

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        try:
            payload = json.loads(msg.payload.decode())
        except json.JSONDecodeError:
            payload = msg.payload.decode()

        if topic.startswith("herald/data/"):
            self._sensor_data[topic] = payload
            print(f"[Dispatcher] Sensor data on {topic}: {payload}")

            # speak sensor value immediately via TTS callback
            text = self._format_sensor_response(topic, payload)
            if text and self._tts:
                self._tts(text)

    def _format_sensor_response(self, topic: str, payload: dict) -> str:
        """Format sensor MQTT payload into a natural language string for TTS."""
        # herald/data/node1/temp_humidity -> {"temperature": 24.5, "humidity": 60.0}
        if "temp_humidity" in topic:
            temp = payload.get("temperature")
            hum  = payload.get("humidity")
            if temp is not None and hum is not None:
                return f"Temperature is {temp:.1f} degrees Celsius. Humidity is {hum:.1f} percent."

        # herald/data/node2/distance -> {"distance": 42.3}
        elif "distance" in topic:
            dist = payload.get("distance")
            if dist is not None:
                return f"Distance is {dist:.2f} centimeters."

        return "Sensor data received."

    def get_tool_schemas(self) -> str:
        """Return tool schemas string for injection into LLM system prompt."""
        return get_schemas_for_llm()

    def dispatch(self, tool_calls: list) -> str | None:
        """Dispatch a list of tool calls to the appropriate MQTT topics.

        Actuator tools: publish and return confirmation string for immediate TTS.
        Sensor tools: publish poll and return None — TTS fires via _on_message callback.
        """
        if not tool_calls:
            return "No tool calls to dispatch."

        confirmations = []

        for tool_call in tool_calls:
            tool_name = tool_call.get("tool")
            node_id   = tool_call.get("node")
            arguments = tool_call.get("arguments", {})

            tool = get_tool(tool_name, node_id)
            if not tool:
                print(f"[Dispatcher] Unknown tool: {tool_name} on {node_id}")
                confirmations.append(f"I don't recognize the tool {tool_name}.")
                continue

            try:
                topic, payload = build_payload(tool_call)
                qos = QOS_POLL if topic.startswith("herald/poll") else QOS_CMD
                self.client.publish(topic, payload, qos=qos)
                print(f"[Dispatcher] Published to {topic}: {payload}")

                # sensor tools — no immediate TTS, callback handles it
                if tool_name in SENSOR_TOOLS:
                    print(f"[Dispatcher] Waiting for sensor response on {node_id}...")
                else:
                    confirmations.append(self._confirmation_string(tool_name, node_id, arguments))

            except Exception as e:
                print(f"[Dispatcher] Error dispatching {tool_name}: {e}")
                confirmations.append(f"Something went wrong with {tool_name}.")

        return " ".join(confirmations) if confirmations else None

    def _confirmation_string(self, tool_name: str, node_id: str, arguments: dict) -> str:
        """Natural language confirmation for actuator tools."""
        confirmations = {
            "set_relay":    lambda a: f"Relay on {node_id} turned {'on' if a.get('state') else 'off'}.",
            "move_servo":   lambda a: f"Servo on {node_id} moved to {a.get('angle')} degrees.",
            "buzz":         lambda a: f"Buzzer on {node_id} activated.",
            "set_display":  lambda a: f"Display on {node_id} updated.",
            "set_rgb_strip":lambda a: f"LED strip on {node_id} set to R{a.get('r')} G{a.get('g')} B{a.get('b')}.",
            "stop":         lambda a: f"All operations on {node_id} stopped.",
        }
        fn = confirmations.get(tool_name)
        return fn(arguments) if fn else f"{tool_name} executed on {node_id}."

    def cleanup(self):
        """Disconnect MQTT client."""
        self.client.loop_stop()
        self.client.disconnect()
        print("[Dispatcher] Disconnected.")


if __name__ == "__main__":
    def mock_tts(text: str):
        print(f"[TTS] {text}")

    dispatcher = Dispatcher(tts_callback=mock_tts)

    print("\n--- Test: set_relay ---")
    result = dispatcher.dispatch([
        {"tool": "set_relay", "node": "node1", "arguments": {"state": True}}
    ])
    print(f"Confirmation: {result}")

    print("\n--- Test: fan-out move_servo ---")
    result = dispatcher.dispatch([
        {"tool": "move_servo", "node": "node1", "arguments": {"angle": 90}},
        {"tool": "move_servo", "node": "node2", "arguments": {"angle": 90}},
    ])
    print(f"Confirmation: {result}")

    print("\n--- Test: get_temp_humidity (async, no immediate return) ---")
    result = dispatcher.dispatch([
        {"tool": "get_temp_humidity", "node": "node1", "arguments": {}}
    ])
    print(f"Confirmation: {result}")  # None expected

    print("\n--- Test: unknown tool ---")
    result = dispatcher.dispatch([
        {"tool": "fly_drone", "node": "node1", "arguments": {}}
    ])
    print(f"Confirmation: {result}")

    time.sleep(2)
    dispatcher.cleanup()