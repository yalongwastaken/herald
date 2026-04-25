# @file    benchmark.py
# @author  Vaidehi Gohil
# @brief   Benchmark for ASR, LLM, and TTS components on RPi 5.
# @usage   python3 benchmark.py --component [asr|llm|tts|pipeline|all]

import os
import sys
import time
import json
import argparse
import numpy as np

N_RUNS = 10

VALID_TOOLS = {
    "node1": {"set_relay", "move_servo", "buzz", "set_display", "get_temp_humidity", "stop"},
    "node2": {"set_rgb_strip", "move_servo", "buzz", "set_display", "get_distance", "stop"},
}
VALID_NODES = set(VALID_TOOLS.keys())

# ── Test data ─────────────────────────────────────────────────────────────────

ASR_SAMPLES = [
    ("Hello.", 1.0),
    ("The quick brown fox jumps over the lazy dog.", 3.0),
    ("Debouncing is a technique used to filter out rapid repeated signals from a button.", 5.0),
    ("Embedded systems require careful management of hardware resources and real-time constraints.", 6.0),
    ("An interrupt service routine is a callback function executed by the processor in response "
     "to a hardware or software interrupt signal.", 8.0),
]

LLM_PROMPTS = [
    ("turn on the relay",                                                              "short",  True),
    ("move the servo to 90 degrees",                                                   "short",  True),
    ("set the LED strip to red",                                                       "medium", True),
    ("move both servos to 45 degrees",                                                 "medium", True),
    ("get the temperature and humidity from node 1 and the distance from node 2",      "long",   True),
    ("what are you?",                                                                  "conv",   False),
    ("what can you do?",                                                               "conv",   False),
]

TTS_TEXTS = [
    ("Hello.", "very_short"),
    ("How can I assist you today?", "short"),
    ("Relay on node 1 turned on.", "medium"),
    ("Servo on node 1 moved to 90 degrees. Servo on node 2 moved to 90 degrees.", "long"),
    ("Herald is online and ready to receive voice commands for the embedded hardware nodes.", "very_long"),
]

TTS_VOICES = ["af_heart", "af_bella"]

PIPELINE_PROMPTS = [
    ("turn on the relay",             "single tool call"),
    ("move the servo to 45 degrees",  "single tool call"),
    ("set the LED strip to blue",     "single tool call"),
    ("move both servos to 90 degrees","fan-out"),
    ("what are you?",                 "conversation"),
]

# ── Model paths ───────────────────────────────────────────────────────────────

_BASE       = os.path.dirname(os.path.abspath(__file__))
_MODELS_DIR = os.path.abspath(os.path.join(_BASE, "..", "models"))

WHISPER_MODEL  = "base.en"
LLM_MODEL_PATH = os.path.join(_MODELS_DIR, "Llama-3.2-3B-Instruct-Q4_K_M.gguf")
KOKORO_MODEL   = os.path.join(_MODELS_DIR, "kokoro-v1.0.onnx")
KOKORO_VOICES  = os.path.join(_MODELS_DIR, "voices-v1.0.bin")

KOKORO_SR       = 24000
WHISPER_SR      = 16000

# ── LLM system prompt ─────────────────────────────────────────────────────────

TOOL_SCHEMAS = """
node1 tools:
  - set_relay(state: boolean) — Turn the relay on Node 1 on or off.
  - move_servo(angle: integer) — Move servo to angle in degrees (0-180).
  - buzz(duration_ms: integer) — Activate buzzer for duration in milliseconds.
  - set_display(message: string) — Show a message on the OLED display.
  - get_temp_humidity() — Read temperature and humidity from DHT11.
  - stop() — Immediately stop all operations on Node 1.

node2 tools:
  - set_rgb_strip(r: integer, g: integer, b: integer) — Set RGB LED strip color.
  - move_servo(angle: integer) — Move servo to angle in degrees (0-180).
  - buzz(duration_ms: integer) — Activate buzzer for duration in milliseconds.
  - set_display(message: string) — Show a message on the LCD display.
  - get_distance() — Read distance from HC-SR04 ultrasonic sensor.
  - stop() — Immediately stop all operations on Node 2.
"""

SYSTEM_PROMPT = f"""You are Herald, a voice-controlled assistant for a distributed embedded hardware system.
You run locally on a Raspberry Pi 5 and can control two ESP32 hardware nodes via voice commands.
 
When the user's command maps to a hardware action, respond ONLY with valid JSON:
{{"tool": "<tool_name>", "node": "<node_id>", "arguments": {{<args>}}}}
For commands targeting multiple nodes, respond with a JSON array of objects: [{{"tool":..,"node":..,"arguments":..}}, {{"tool":..,"node":..,"arguments":..}}].
Each object in the array must have exactly the keys "tool", "node", and "arguments". Never use semicolons between objects.
When no tool applies or the user is just talking, respond naturally in plain text (max 40 words).
Never explain your reasoning. Never return anything other than JSON or plain text.
Always use JSON boolean values true or false, never strings.
Valid node IDs are: node1, node2.
 
Example:
User: turn on the relay
Response: {{"tool": "set_relay", "node": "node1", "arguments": {{"state": true}}}}
 
User: get temperature from node1 and distance from node2
Response: [{{"tool": "get_temp_humidity", "node": "node1", "arguments": {{}}}}, {{"tool": "get_distance", "node": "node2", "arguments": {{}}}}]
 
User: move both servos to 45 degrees
Response: [{{"tool": "move_servo", "node": "node1", "arguments": {{"angle": 45}}}}, {{"tool": "move_servo", "node": "node2", "arguments": {{"angle": 45}}}}]
 
User: what are you?
Response: I'm Herald, a voice assistant running locally on a Raspberry Pi 5. I can control two ESP32 hardware nodes via voice commands.
 
Available nodes and tools:
{TOOL_SCHEMAS}"""


# ── Tee logger ────────────────────────────────────────────────────────────────

class Tee:
    """Mirrors all stdout writes to a log file.

    Use printlog(term_line, log_line) when the terminal and log file should
    show different strings (e.g. truncated vs full transcript/response).
    """

    def __init__(self, log_path: str):
        self._terminal = sys.stdout
        self._log = open(log_path, "w", buffering=1)  # line-buffered

    def write(self, data: str) -> None:
        self._terminal.write(data)
        self._log.write(data)

    def flush(self) -> None:
        self._terminal.flush()
        self._log.flush()

    def printlog(self, term_line: str, log_line: str) -> None:
        """Print term_line to terminal and log_line to the log file."""
        self._terminal.write(term_line + "\n")
        self._terminal.flush()
        self._log.write(log_line + "\n")
        self._log.flush()

    def close(self) -> None:
        sys.stdout = self._terminal
        self._log.close()

    # Needed so sys.stdout.fileno() calls (e.g. from C extensions) still work.
    def fileno(self):
        return self._terminal.fileno()

# ── Helpers ───────────────────────────────────────────────────────────────────

def print_stats(label: str, values: list) -> None:
    arr = np.array(values)
    print(f"  {label:<20} mean={arr.mean():.3f}  std={arr.std():.3f}  "
          f"min={arr.min():.3f}  max={arr.max():.3f}")

def tts_to_whisper_audio(kokoro, text: str) -> np.ndarray:
    samples, sr = kokoro.create(text, voice="af_heart", speed=1.0, lang="en-us")
    n = int(len(samples) * WHISPER_SR / sr)
    resampled = np.interp(
        np.linspace(0, len(samples), n),
        np.arange(len(samples)),
        samples
    ).astype(np.float32)
    return resampled

def compute_wer(reference: str, hypothesis: str) -> float:
    ref = reference.lower().split()
    hyp = hypothesis.lower().split()
    edits = sum(1 for r, h in zip(ref, hyp) if r != h) + abs(len(ref) - len(hyp))
    return edits / max(len(ref), 1)

def validate_tool_call(raw: str, expect_tool: bool) -> tuple[bool, str]:
    """
    Returns (is_correct, reason).
    If expect_tool is True, validates JSON structure, node ID, tool name, and arguments exist.
    If expect_tool is False, validates the response is plain text (not JSON).
    """
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError:
        if not expect_tool:
            return True, "ok"
        return False, "invalid JSON"

    if not expect_tool:
        return False, "returned JSON for conversational prompt"

    entries = parsed if isinstance(parsed, list) else [parsed]
    for entry in entries:
        if "tool" not in entry:
            return False, "missing 'tool' key"
        if "node" not in entry:
            return False, "missing 'node' key"
        if "arguments" not in entry:
            return False, "missing 'arguments' key"
        node = entry["node"]
        tool = entry["tool"]
        if node not in VALID_NODES:
            return False, f"invalid node '{node}'"
        if tool not in VALID_TOOLS[node]:
            return False, f"invalid tool '{tool}' for {node}"
    return True, "ok"

# ── ASR Benchmark ─────────────────────────────────────────────────────────────

def benchmark_asr(tee: "Tee") -> dict:
    from faster_whisper import WhisperModel
    from kokoro_onnx import Kokoro

    print("\n" + "="*60)
    print("ASR BENCHMARK — faster-whisper")
    print("="*60)
    print(f"  model: {WHISPER_MODEL}  |  runs per sample: {N_RUNS}")

    t0 = time.time()
    model = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
    whisper_load = time.time() - t0
    print(f"  Whisper load time: {whisper_load:.2f}s")

    t0 = time.time()
    kokoro = Kokoro(KOKORO_MODEL, KOKORO_VOICES)
    kokoro_load = time.time() - t0
    print(f"  Kokoro load time (for audio gen): {kokoro_load:.2f}s")

    print("\n  generating test audio via Kokoro TTS...")
    all_times, all_rtfs, all_wers = [], [], []

    print(f"\n  {'#':<4} {'dur':>6} {'time':>7} {'RTF':>7} {'WER':>6}  transcript")
    print(f"  {'-'*75}")

    for i, (text, _) in enumerate(ASR_SAMPLES):
        audio = tts_to_whisper_audio(kokoro, text)
        duration = len(audio) / WHISPER_SR

        run_times = []
        transcript = ""
        for _ in range(N_RUNS):
            t0 = time.time()
            segments, _ = model.transcribe(audio, language="en")
            transcript = " ".join(s.text.strip() for s in segments)
            run_times.append(time.time() - t0)

        elapsed = np.mean(run_times)
        rtf = elapsed / duration
        wer = compute_wer(text, transcript)

        all_times.append(elapsed)
        all_rtfs.append(rtf)
        all_wers.append(wer)

        # terminal: truncated transcript; log file: full transcript
        tee.printlog(
            f"  [{i+1}] {duration:>5.1f}s  {elapsed:>6.2f}s  {rtf:>6.3f}  {wer:>5.2f}  '{transcript[:45]}'",
            f"  [{i+1}] {duration:>5.1f}s  {elapsed:>6.2f}s  {rtf:>6.3f}  {wer:>5.2f}  '{transcript}'",
        )
    print(f"\n  --- summary ---")
    print_stats("latency (s)", all_times)
    print_stats("RTF", all_rtfs)
    print_stats("WER", all_wers)
    print(f"  throughput: {sum(len(tts_to_whisper_audio(kokoro, t)) / WHISPER_SR for t, _ in ASR_SAMPLES) / sum(all_times):.2f}x real-time")

    return {
        "load_time_s": whisper_load,
        "mean_latency_s": float(np.mean(all_times)),
        "mean_rtf": float(np.mean(all_rtfs)),
        "mean_wer": float(np.mean(all_wers)),
    }

# ── LLM Benchmark ─────────────────────────────────────────────────────────────

def benchmark_llm(tee: "Tee") -> dict:
    from llama_cpp import Llama

    print("\n" + "="*60)
    print("LLM BENCHMARK — llama-cpp-python (Llama 3.2 3B Q4_K_M)")
    print("="*60)
    print(f"  model: {os.path.basename(LLM_MODEL_PATH)}  |  runs per prompt: {N_RUNS}")

    t0 = time.time()
    llm = Llama(model_path=LLM_MODEL_PATH, n_ctx=2048, n_threads=4, verbose=False)
    load_time = time.time() - t0
    print(f"  model load time: {load_time:.2f}s")

    all_times, all_gen_tps, all_gen_tokens = [], [], []
    correct = 0
    total = 0

    print(f"\n  {'#':<4} {'type':<8} {'time':>7} {'g_tps':>8} {'tok':>5} {'ok':<6}  response")
    print(f"  {'-'*80}")

    for i, (prompt, length, expect_tool) in enumerate(LLM_PROMPTS):
        messages = [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": prompt},
        ]

        run_times = []
        all_raw = []
        response_data = None
        for _ in range(N_RUNS):
            t0 = time.time()
            response_data = llm.create_chat_completion(
                messages=messages,
                max_tokens=256,
                temperature=0.1,
                stop=["</s>"],
            )
            run_times.append(time.time() - t0)
            all_raw.append(response_data["choices"][0]["message"]["content"].strip())

        elapsed = np.mean(run_times)
        usage = response_data.get("usage", {})
        g_tokens = usage.get("completion_tokens", len(all_raw[-1].split()))
        g_tps = g_tokens / elapsed if elapsed > 0 else 0

        # correctness across all runs
        run_correct = sum(1 for r in all_raw if validate_tool_call(r, expect_tool)[0])
        is_correct = run_correct == N_RUNS
        _, reason = validate_tool_call(all_raw[-1], expect_tool)

        if is_correct:
            correct += 1
        total += 1

        all_times.append(elapsed)
        all_gen_tps.append(g_tps)
        all_gen_tokens.append(g_tokens)

        status = "PASS" if is_correct else f"FAIL({run_correct}/{N_RUNS})"
        # terminal: truncated response; log file: full response
        tee.printlog(
            f"  [{i+1}] {length:<8} {elapsed:>6.2f}s  {g_tps:>7.1f}  {g_tokens:>4} {status:<10}  '{all_raw[-1][:35]}'",
            f"  [{i+1}] {length:<8} {elapsed:>6.2f}s  {g_tps:>7.1f}  {g_tokens:>4} {status:<10}  '{all_raw[-1]}'",
        )        
        if not is_correct:
            print(f"       reason: {reason}")

    accuracy = correct / total * 100
    print(f"\n  --- summary ---")
    print_stats("latency (s)", all_times)
    print_stats("gen tokens/req", all_gen_tokens)
    print_stats("approx gen tps", all_gen_tps)
    print(f"  tool-call accuracy: {accuracy:.0f}% ({correct}/{total})")

    return {
        "load_time_s": load_time,
        "mean_latency_s": float(np.mean(all_times)),
        "mean_gen_tps": float(np.mean(all_gen_tps)),
        "tool_call_accuracy_pct": accuracy,
    }

# ── TTS Benchmark ─────────────────────────────────────────────────────────────

def benchmark_tts() -> dict:
    from kokoro_onnx import Kokoro

    print("\n" + "="*60)
    print("TTS BENCHMARK — kokoro-onnx")
    print("="*60)

    t0 = time.time()
    kokoro = Kokoro(KOKORO_MODEL, KOKORO_VOICES)
    load_time = time.time() - t0
    print(f"  model load time: {load_time:.2f}s")
    print(f"  voices tested: {TTS_VOICES}  |  runs per text: {N_RUNS}\n")

    results = {"load_time_s": load_time, "voices": {}}

    for voice in TTS_VOICES:
        print(f"  voice: {voice}")
        print(f"  {'#':<4} {'label':<12} {'dur':>6} {'time':>7} {'RTF':>7}")
        print(f"  {'-'*45}")

        times, rtfs = [], []
        for i, (text, label) in enumerate(TTS_TEXTS):
            run_times = []
            samples = None
            for _ in range(N_RUNS):
                t0 = time.time()
                samples, sr = kokoro.create(text, voice=voice, speed=1.0, lang="en-us")
                run_times.append(time.time() - t0)

            elapsed = np.mean(run_times)
            duration = len(samples) / KOKORO_SR
            rtf = elapsed / duration
            times.append(elapsed)
            rtfs.append(rtf)

            print(f"  [{i+1}] {label:<12} {duration:>5.2f}s  {elapsed:>6.2f}s  {rtf:>6.3f}")

        print(f"  avg RTF: {np.mean(rtfs):.3f}  avg latency: {np.mean(times):.3f}s\n")
        results["voices"][voice] = {
            "mean_latency_s": float(np.mean(times)),
            "mean_rtf": float(np.mean(rtfs)),
        }

    return results

# ── Pipeline Benchmark ────────────────────────────────────────────────────────

def benchmark_pipeline(tee: "Tee") -> dict:
    from faster_whisper import WhisperModel
    from llama_cpp import Llama
    from kokoro_onnx import Kokoro

    print("\n" + "="*60)
    print("PIPELINE BENCHMARK — ASR → LLM → TTS (end-to-end)")
    print("="*60)

    print("  loading models...")
    t0 = time.time()
    asr_model = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
    llm = Llama(model_path=LLM_MODEL_PATH, n_ctx=2048, n_threads=4, verbose=False)
    kokoro = Kokoro(KOKORO_MODEL, KOKORO_VOICES)
    load_time = time.time() - t0
    print(f"  all models loaded in {load_time:.2f}s\n")

    print(f"  {'#':<4} {'type':<20} {'asr':>7} {'llm':>7} {'tts':>7} {'total':>7} {'ok':<6}  tool/response")
    print(f"  {'-'*80}")

    results = []
    total_times = []

    for i, (text, label) in enumerate(PIPELINE_PROMPTS):
        expect_tool = label != "conversation"

        # stage 1: TTS → audio (simulating mic input)
        audio = tts_to_whisper_audio(kokoro, text)

        # stage 2: ASR
        t0 = time.time()
        segments, _ = asr_model.transcribe(audio, language="en")
        transcript = " ".join(s.text.strip() for s in segments)
        asr_time = time.time() - t0

        # stage 3: LLM
        messages = [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": transcript},
        ]
        t0 = time.time()
        response_data = llm.create_chat_completion(
            messages=messages,
            max_tokens=256,
            temperature=0.1,
            stop=["</s>"],
        )
        llm_time = time.time() - t0
        raw = response_data["choices"][0]["message"]["content"].strip()

        # stage 4: TTS response
        t0 = time.time()
        tts_text = raw if not expect_tool else f"Command executed: {raw[:60]}"
        kokoro.create(tts_text[:200], voice="af_heart", speed=1.0, lang="en-us")
        tts_time = time.time() - t0

        total = asr_time + llm_time + tts_time
        total_times.append(total)

        is_correct, reason = validate_tool_call(raw, expect_tool)
        status = "PASS" if is_correct else "FAIL"

        # terminal: truncated response; log file: full response
        tee.printlog(
            f"  [{i+1}] {label:<20} {asr_time:>6.2f}s {llm_time:>6.2f}s {tts_time:>6.2f}s {total:>6.2f}s {status:<6}  '{raw[:30]}'",
            f"  [{i+1}] {label:<20} {asr_time:>6.2f}s {llm_time:>6.2f}s {tts_time:>6.2f}s {total:>6.2f}s {status:<6}  '{raw}'",
        )        
        if not is_correct:
            print(f"       transcript: '{transcript}'  reason: {reason}")

        results.append({
            "prompt": text,
            "transcript": transcript,
            "response": raw,
            "asr_time_s": asr_time,
            "llm_time_s": llm_time,
            "tts_time_s": tts_time,
            "total_time_s": total,
            "correct": is_correct,
        })

    passed = sum(1 for r in results if r["correct"])
    print(f"\n  --- summary ---")
    print_stats("total latency (s)", total_times)
    print(f"  pass rate: {passed}/{len(results)}")
    print(f"  mean stage breakdown:")
    print(f"    ASR: {np.mean([r['asr_time_s'] for r in results]):.3f}s")
    print(f"    LLM: {np.mean([r['llm_time_s'] for r in results]):.3f}s")
    print(f"    TTS: {np.mean([r['tts_time_s'] for r in results]):.3f}s")

    return {
        "load_time_s": load_time,
        "mean_total_latency_s": float(np.mean(total_times)),
        "pass_rate": f"{passed}/{len(results)}",
        "runs": results,
    }

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--component", choices=["asr", "llm", "tts", "pipeline", "all"], default="all")
    args = parser.parse_args()

    log_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "benchmark_log.txt")
    tee = Tee(log_path)
    sys.stdout = tee

    try:
        results = {}

        if args.component in ("asr", "all"):
            results["asr"] = benchmark_asr(tee)
        if args.component in ("llm", "all"):
            results["llm"] = benchmark_llm(tee)
        if args.component in ("tts", "all"):
            results["tts"] = benchmark_tts()
        if args.component in ("pipeline", "all"):
            results["pipeline"] = benchmark_pipeline(tee)

        out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "benchmark_results.json")
        with open(out_path, "w") as f:
            json.dump(results, f, indent=2)

        print("\n" + "="*60)
        print("benchmark complete.")
        print(f"results saved to {out_path}")
        print(f"log saved to     {log_path}")
        print("="*60)

    finally:
        tee.close()

if __name__ == "__main__":
    main()