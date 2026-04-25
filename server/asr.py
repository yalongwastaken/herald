# @file    asr.py
# @author  Vaidehi Gohil
# @brief   Push-to-talk ASR using faster-whisper and INMP441 USB mic on RPi 5.

import numpy as np
import sounddevice as sd
from scipy.signal import resample_poly
from gpiozero import Button
from faster_whisper import WhisperModel

BUTTON_PIN = 17


class ASR:
    def __init__(
        self,
        model_size: str = "base.en",
        device: str = "cpu",
        compute_type: str = "int8",
        input_device: int = None,
        sample_rate: int = 48000,
        whisper_rate: int = 16000,
        channels: int = 1,
        button_pin: int = BUTTON_PIN,
    ):
        self.input_device = input_device
        self.sample_rate = sample_rate
        self.whisper_rate = whisper_rate
        self.channels = channels

        if input_device is None:
            print("[ASR] Available audio devices:")
            print(sd.query_devices())
            print("[ASR] Warning: no input_device specified, using system default.")

        self.button = Button(button_pin, pull_up=True)

        print("[ASR] Loading Whisper model...")
        self.model = WhisperModel(model_size, device=device, compute_type=compute_type)
        print("[ASR] Model loaded.")

    def wait_for_button_press(self):
        """Block until the push-to-talk button is pressed."""
        print("[ASR] Waiting for button press...")
        self.button.wait_for_press()
        print("[ASR] Button pressed — recording started.")

    def record_until_release(self) -> np.ndarray:
        """Record audio from the mic until the button is released.
        Returns raw audio as a numpy array (sample_rate Hz, int32)."""
        chunks = []
        chunk_duration = 0.1
        chunk_size = int(self.sample_rate * chunk_duration)

        with sd.InputStream(
            samplerate=self.sample_rate,
            channels=self.channels,
            dtype="int32",
            device=self.input_device,
        ) as stream:
            while self.button.is_pressed:
                chunk, _ = stream.read(chunk_size)
                chunks.append(chunk)

        print("[ASR] Button released — recording stopped.")

        if not chunks:
            return np.array([], dtype=np.int32)
        return np.concatenate(chunks, axis=0)

    def _preprocess(self, audio: np.ndarray) -> np.ndarray:
        """Convert raw mic audio to Whisper-compatible format.
        Steps:
          1. int32 -> float32 normalisation
          2. Mono or stereo -> mono
          3. Resample to 16000Hz using polyphase filter (no aliasing)
        """
        if audio.size == 0:
            return np.array([], dtype=np.float32)

        # normalise int32 to [-1.0, 1.0]
        audio_float = audio.astype(np.float32) / np.iinfo(np.int32).max

        # stereo -> mono if needed
        if audio_float.ndim == 2:
            audio_float = audio_float.mean(axis=1)

        # resample using polyphase filter — avoids aliasing from naive decimation
        from math import gcd
        g = gcd(self.whisper_rate, self.sample_rate)
        up = self.whisper_rate // g
        down = self.sample_rate // g
        resampled = resample_poly(audio_float, up, down).astype(np.float32)

        return resampled

    def transcribe(self, audio: np.ndarray) -> str:
        """Preprocess and transcribe audio. Returns transcript string."""
        processed = self._preprocess(audio)

        if processed.size == 0:
            print("[ASR] No audio captured.")
            return ""

        segments, _ = self.model.transcribe(processed, language="en")
        transcript = " ".join(segment.text.strip() for segment in segments)
        print(f"[ASR] Transcript: {transcript}")
        return transcript

    def listen(self) -> str:
        """Full push-to-talk listen cycle.
        Blocks until button press, records until release, returns transcript.
        This is what main.py calls in its loop."""
        self.wait_for_button_press()
        audio = self.record_until_release()
        return self.transcribe(audio)

    def cleanup(self):
        """Release GPIO resources. Call on shutdown."""
        self.button.close()


if __name__ == "__main__":
    asr = ASR()
    try:
        while True:
            result = asr.listen()
            if result:
                print(f"You said: {result}")
    except KeyboardInterrupt:
        print("\n[ASR] Shutting down.")
        asr.cleanup()