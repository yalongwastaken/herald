# @file    tts.py
# @author  Vaidehi Gohil
# @brief   TTS using kokoro-onnx and sounddevice on RPi 5.

import os
import numpy as np
import sounddevice as sd
from scipy.signal import resample_poly
from math import gcd
from kokoro_onnx import Kokoro

_BASE       = os.path.dirname(os.path.abspath(__file__))
_MODELS_DIR = os.path.join(_BASE, "models")

MODEL_PATH  = os.path.join(_MODELS_DIR, "kokoro-v1.0.onnx")
VOICES_PATH = os.path.join(_MODELS_DIR, "voices-v1.0.bin")

KOKORO_SR = 24000


class TTS:
    def __init__(
        self,
        model_path: str = MODEL_PATH,
        voices_path: str = VOICES_PATH,
        voice: str = "af_heart",
        speed: float = 1.0,
        output_device: int = None,
        sample_rate: int = 48000,
    ):
        self.voice         = voice
        self.speed         = speed
        self.output_device = output_device
        self.sample_rate   = sample_rate

        if output_device is None:
            print("[TTS] Available audio devices:")
            print(sd.query_devices())
            print("[TTS] Warning: no output_device specified, using system default.")

        print("[TTS] Loading Kokoro model...")
        self.kokoro = Kokoro(model_path, voices_path)
        print("[TTS] Model loaded.")

    def speak(self, text: str):
        """Synthesize text and play through the speaker."""
        if not text or not text.strip():
            return

        print(f"[TTS] Speaking: {text}")

        samples, sr = self.kokoro.create(
            text,
            voice=self.voice,
            speed=self.speed,
            lang="en-us",
        )

        # resample using polyphase filter if needed
        if sr != self.sample_rate:
            g    = gcd(self.sample_rate, sr)
            up   = self.sample_rate // g
            down = sr // g
            samples = resample_poly(samples, up, down).astype(np.float32)

        sd.play(samples, samplerate=self.sample_rate, device=self.output_device)
        sd.wait()
        print("[TTS] Done.")

    def cleanup(self):
        """Stop any playing audio."""
        sd.stop()


if __name__ == "__main__":
    tts = TTS()
    test_phrases = [
        "Herald is online and ready.",
        "Relay on node 1 turned on.",
        "Servo on node 1 moved to 90 degrees.",
        "I don't recognize that command.",
    ]
    for phrase in test_phrases:
        tts.speak(phrase)