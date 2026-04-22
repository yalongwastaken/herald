import json
import time
import paho.mqtt.client as mqtt
from tools import get_tool, build_payload, get_schemas_for_llm


BROKER_HOST = "localhost"
BROKER_PORT = 1883
ACK_TIMEOUT = 5.0       # seconds to wait for ack from node
QOS_CMD = 0             # commands use QoS 0 (lowest latency)
QOS_POLL = 1            # sensor polls use QoS 1 (reliable)


class Dispatcher:
    def __init__(self, broker_host: str = BROKER_HOST, broker_port: int = BROKER_PORT):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self._acks = {}         # node_id -> ack payload received
        self._sensor_data = {}  # topic -> sensor payload received

        # Set up MQTT client
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

        print("[Dispatcher] Connecting to MQTT broker...")
        self.client.connect(broker_host, broker_port, keepalive=60)
        self.client.loop_start()
        print("[Dispatcher] Connected.")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        # Subscribe to ack and sensor data topics for all nodes
        client.subscribe("herald/ack/+", qos=QOS_POLL)
        client.subscribe("herald/data/+/+", qos=QOS_POLL)
        print("[Dispatcher] Subscribed to ack and data topics.")

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        try:
            payload = json.loads(msg.payload.decode())
        except json.JSONDecodeError:
            payload = msg.payload.decode()

        # Handle acks: herald/ack/<node_id>
        if topic.startswith("herald/ack/"):
            node_id = topic.split("/")[2]
            self._acks[node_id] = payload
            print(f"[Dispatcher] Ack from {node_id}: {payload}")

        # Handle sensor data: herald/data/<node_id>/<sensor>
        elif topic.startswith("herald/data/"):
            self._sensor_data[topic] = payload
            print(f"[Dispatcher] Sensor data on {topic}: {payload}")

    def get_tool_schemas(self) -> str:
        """Return tool schemas string for injection into LLM system prompt."""
        return get_schemas_for_llm()

    def dispatch(self, tool_calls: list) -> str:
        """Dispatch a list of tool calls to the appropriate MQTT topics.
        Returns a confirmation string for TTS."""
        if not tool_calls:
            return "No tool calls to dispatch."

        confirmations = []

        for tool_call in tool_calls:
            tool_name = tool_call.get("tool")
            node_id = tool_call.get("node")
            arguments = tool_call.get("arguments", {})

            # Validate tool exists
            tool = get_tool(tool_name, node_id)
            if not tool:
                print(f"[Dispatcher] Unknown tool: {tool_name} on {node_id}")
                confirmations.append(f"I don't recognize the tool {tool_name}.")
                continue

            # Build and publish MQTT payload
            try:
                topic, payload = build_payload(tool_call)
                qos = QOS_POLL if topic.startswith("herald/poll") else QOS_CMD
                self.client.publish(topic, payload, qos=qos)
                print(f"[Dispatcher] Published to {topic}: {payload}")
                confirmations.append(self._confirmation_string(tool_name, node_id, arguments))

            except Exception as e:
                print(f"[Dispatcher] Error dispatching {tool_name}: {e}")
                confirmations.append(f"Something went wrong with {tool_name}.")

        return " ".join(confirmations)

    def _confirmation_string(self, tool_name: str, node_id: str, arguments: dict) -> str:
        """Generate a natural language confirmation string for TTS."""
        confirmations = {
            "set_relay": lambda a: f"Relay on {node_id} turned {'on' if a.get('state') else 'off'}.",
            "move_servo": lambda a: f"Servo on {node_id} moved to {a.get('angle')} degrees.",
            "buzz": lambda a: f"Buzzer on {node_id} activated.",
            "set_display": lambda a: f"Display on {node_id} updated.",
            "set_rgb_strip": lambda a: f"LED strip on {node_id} color set.",
            "get_temp_humidity": lambda a: f"Requesting temperature and humidity from {node_id}.",
            "get_distance": lambda a: f"Requesting distance reading from {node_id}.",
            "stop": lambda a: f"Stopping all operations on {node_id}.",
        }

        fn = confirmations.get(tool_name)
        if fn:
            return fn(arguments)
        return f"{tool_name} executed on {node_id}."

    def wait_for_ack(self, node_id: str, timeout: float = ACK_TIMEOUT) -> dict | None:
        """Block until ack received from node or timeout. Returns ack payload or None."""
        start = time.time()
        while time.time() - start < timeout:
            if node_id in self._acks:
                ack = self._acks.pop(node_id)
                return ack
            time.sleep(0.05)
        print(f"[Dispatcher] No ack from {node_id} within {timeout}s.")
        return None

    def cleanup(self):
        """Disconnect MQTT client."""
        self.client.loop_stop()
        self.client.disconnect()
        print("[Dispatcher] Disconnected.")


if __name__ == "__main__":
    # Test dispatcher — requires Mosquitto running on localhost
    dispatcher = Dispatcher()

    # Test single dispatch
    print("\n--- Test: set_relay ---")
    result = dispatcher.dispatch([
        {"tool": "set_relay", "node": "node1", "arguments": {"state": True}}
    ])
    print(f"Confirmation: {result}")

    # Test fan-out
    print("\n--- Test: fan-out move_servo ---")
    result = dispatcher.dispatch([
        {"tool": "move_servo", "node": "node1", "arguments": {"angle": 90}},
        {"tool": "move_servo", "node": "node2", "arguments": {"angle": 90}},
    ])
    print(f"Confirmation: {result}")

    # Test unknown tool
    print("\n--- Test: unknown tool ---")
    result = dispatcher.dispatch([
        {"tool": "fly_drone", "node": "node1", "arguments": {}}
    ])
    print(f"Confirmation: {result}")

    time.sleep(1)
    dispatcher.cleanup()
