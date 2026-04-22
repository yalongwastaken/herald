import json
from dataclasses import dataclass
from llama_cpp import Llama


MODEL_PATH = "/home/herald/herald/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf"

SYSTEM_PROMPT = """You are a voice command dispatcher for a network of embedded hardware nodes.
When the user's command maps to a hardware action, respond ONLY with valid JSON:
{{"tool": "<tool_name>", "node": "<node_id>", "arguments": {{<args>}}}}
For fan-out commands that target multiple nodes, respond with a JSON array:
[{{"tool": "<tool_name>", "node": "<node_id>", "arguments": {{<args>}}}}, ...]
When no tool applies, respond with a short plain-text sentence (max 20 words).
Never explain your reasoning. Never return anything other than JSON or a sentence.
Available nodes and tools:
{tool_schemas}"""


@dataclass
class LLMResult:
    is_tool_call: bool
    tool_calls: list        # list of dicts — always a list, even for single calls
    text: str               # plain text response if not a tool call


class LLM:
    def __init__(
        self,
        model_path: str = MODEL_PATH,
        n_ctx: int = 2048,
        n_threads: int = 4,
        verbose: bool = False,
    ):
        print("[LLM] Loading model...")
        self.model = Llama(
            model_path=model_path,
            n_ctx=n_ctx,
            n_threads=n_threads,
            verbose=verbose,
        )
        print("[LLM] Model loaded.")
        self._tool_schemas = ""

    def set_tool_schemas(self, schemas: str):
        """Inject tool schemas into the system prompt at runtime.
        Called by dispatcher after tools.py is loaded."""
        self._tool_schemas = schemas

    def _build_prompt(self, transcript: str) -> list:
        """Build the messages list for the LLM."""
        system = SYSTEM_PROMPT.format(tool_schemas=self._tool_schemas)
        return [
            {"role": "system", "content": system},
            {"role": "user", "content": transcript},
        ]

    def _parse_response(self, raw: str) -> LLMResult:
        """Parse LLM response into a LLMResult.
        Tries to parse as JSON tool call(s), falls back to plain text."""
        raw = raw.strip()

        try:
            parsed = json.loads(raw)

            # Single tool call — wrap in list for uniform handling
            if isinstance(parsed, dict) and "tool" in parsed:
                return LLMResult(is_tool_call=True, tool_calls=[parsed], text="")

            # Fan-out — list of tool calls
            if isinstance(parsed, list) and all("tool" in tc for tc in parsed):
                return LLMResult(is_tool_call=True, tool_calls=parsed, text="")

        except json.JSONDecodeError:
            pass

        # Plain text response
        return LLMResult(is_tool_call=False, tool_calls=[], text=raw)

    def infer(self, transcript: str) -> LLMResult:
        """Run inference on a transcript. Returns an LLMResult."""
        print(f"[LLM] Inferring: {transcript}")

        messages = self._build_prompt(transcript)

        response = self.model.create_chat_completion(
            messages=messages,
            max_tokens=256,
            temperature=0.1,    # low temperature for deterministic tool calls
            stop=["</s>"],
        )

        raw = response["choices"][0]["message"]["content"]
        print(f"[LLM] Raw response: {raw}")

        result = self._parse_response(raw)
        return result


if __name__ == "__main__":
    # Quick test with dummy tool schemas
    dummy_schemas = """
node1 tools:
- set_relay(state: boolean) — turn the relay on or off
- move_servo(angle: int) — move servo to angle in degrees (0-180)
- buzz(duration_ms: int) — activate buzzer for duration in milliseconds

node2 tools:
- set_rgb_strip(r: int, g: int, b: int) — set RGB LED strip color
- move_servo(angle: int) — move servo to angle in degrees (0-180)
- buzz(duration_ms: int) — activate buzzer for duration in milliseconds
"""

    llm = LLM()
    llm.set_tool_schemas(dummy_schemas)

    test_commands = [
        "turn on the relay",
        "move the servo to 90 degrees",
        "move both servos to 45 degrees",
        "set the LED strip to red",
        "what time is it",
    ]

    for cmd in test_commands:
        print(f"\n--- Testing: '{cmd}' ---")
        result = llm.infer(cmd)
        if result.is_tool_call:
            print(f"Tool calls: {result.tool_calls}")
        else:
            print(f"Plain text: {result.text}")
