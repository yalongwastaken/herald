import numpy as np
import sounddevice as sd
import soundfile as sf
import io
from kokoro_onnx import Kokoro


MODEL_PATH = "/home/herald/herald/models/kokoro-v1.0.onnx"
VOICES_PATH = "/home/herald/herald/models/voices-v1.0.bin"


class TTS:
    def __init__(
        self,
        model_path: str = MODEL_PATH,
        voices_path: str = VOICES_PATH,
        voice: str = "af_heart",
        speed: float = 1.0,
        output_device: int = 0,
        sample_rate: int = 48000,
    ):
        self.voice = voice
        self.speed = speed
        self.output_device = output_device
        self.sample_rate = sample_rate

        print("[TTS] Loading Kokoro model...")
        self.kokoro = Kokoro(model_path, voices_path)
        print("[TTS] Model loaded.")

    def speak(self, text: str):
        """Synthesize text to speech and play through the speaker."""
        if not text or not text.strip():
            return

        print(f"[TTS] Speaking: {text}")

        # Generate audio samples from text
        samples, sample_rate = self.kokoro.create(
            text,
            voice=self.voice,
            speed=self.speed,
            lang="en-us",
        )

        # Resample to 48000Hz if needed (kokoro outputs 24000Hz)
        if sample_rate != self.sample_rate:
            ratio = self.sample_rate / sample_rate
            target_length = int(len(samples) * ratio)
            samples = np.interp(
                np.linspace(0, len(samples) - 1, target_length),
                np.arange(len(samples)),
                samples,
            )

        # Play through the I2S speaker
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
