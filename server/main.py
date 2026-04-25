import signal
import sys
from asr import ASR
from llm import LLM
from tts import TTS
from dispatcher import Dispatcher


def confirmation_for(tool_calls: list) -> str:
    """Generate a short spoken confirmation for a list of tool calls."""
    if len(tool_calls) == 1:
        tc = tool_calls[0]
        return f"Running {tc['tool']} on {tc['node']}."
    else:
        nodes = ", ".join(set(tc['node'] for tc in tool_calls))
        return f"Dispatching {len(tool_calls)} commands to {nodes}."


def main():
    print("[Herald] Starting up...")

    # Initialise all components
    asr = ASR()
    llm = LLM()
    tts = TTS()
    dispatcher = Dispatcher()

    # Inject tool schemas into LLM
    llm.set_tool_schemas(dispatcher.get_tool_schemas())

    # Speak startup confirmation
    tts.speak("Herald is online and ready.")
    print("[Herald] Ready. Press button to speak.")

    # Handle clean shutdown on Ctrl+C or SIGTERM
    def shutdown(sig=None, frame=None):
        print("\n[Herald] Shutting down...")
        tts.speak("Herald shutting down.")
        asr.cleanup()
        dispatcher.cleanup()
        tts.cleanup()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # Main loop
    while True:
        try:
            # Wait for button press, record until release, transcribe
            transcript = asr.listen()

            if not transcript:
                print("[Herald] No speech detected.")
                continue

            # Run LLM inference
            result = llm.infer(transcript)

            if result.is_tool_call:
                # Dispatch tool calls to MQTT
                confirmation = dispatcher.dispatch(result.tool_calls)
                tts.speak(confirmation)
            else:
                # Plain text response — speak directly
                tts.speak(result.text)

        except Exception as e:
            print(f"[Herald] Error: {e}")
            tts.speak("Something went wrong. Please try again.")
            continue


if __name__ == "__main__":
    main()
