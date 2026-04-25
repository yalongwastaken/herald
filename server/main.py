# @file    main.py
# @author  Vaidehi Gohil
# @brief   Herald pipeline entrypoint. ASR -> LLM -> Dispatcher -> TTS.

import signal
import sys
from asr import ASR
from llm import LLM
from tts import TTS
from dispatcher import Dispatcher


def main():
    print("[Herald] Starting up...")

    asr        = ASR()
    llm        = LLM()
    tts        = TTS()

    # wire TTS callback into dispatcher for async sensor responses
    dispatcher = Dispatcher(tts_callback=tts.speak)

    llm.set_tool_schemas(dispatcher.get_tool_schemas())

    tts.speak("Herald is online and ready.")
    print("[Herald] Ready. Press button to speak.")

    def shutdown(sig=None, frame=None):
        print("\n[Herald] Shutting down...")
        tts.speak("Herald shutting down.")
        asr.cleanup()
        dispatcher.cleanup()
        tts.cleanup()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    while True:
        try:
            transcript = asr.listen()
            if not transcript:
                print("[Herald] No speech detected.")
                continue

            result = llm.infer(transcript)

            if result.is_tool_call:
                confirmation = dispatcher.dispatch(result.tool_calls)
                if confirmation:
                    tts.speak(confirmation)
            else:
                tts.speak(result.text)

        except Exception as e:
            print(f"[Herald] Error: {e}")
            tts.speak("Something went wrong. Please try again.")
            continue


if __name__ == "__main__":
    main()