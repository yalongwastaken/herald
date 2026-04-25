# @file    download_models.py
# @author  Anthony Yalong
# @brief   Downloads herald model files into server/models/.

import os
import urllib.request

_BASE       = os.path.dirname(os.path.abspath(__file__))
_MODELS_DIR = os.path.join(_BASE, "models")

MODELS = [
    {
        "name": "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
        "url": "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
    },
    {
        "name": "kokoro-v1.0.onnx",
        "url": "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx",
    },
    {
        "name": "voices-v1.0.bin",
        "url": "https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/voices-v1.0.bin",
    },
]


def download(name: str, url: str, dest: str):
    if os.path.exists(dest):
        print(f"[skip] {name} already exists.")
        return

    print(f"[download] {name} ...")

    def progress(count, block_size, total_size):
        if total_size > 0:
            pct = count * block_size * 100 // total_size
            print(f"\r  {pct}%", end="", flush=True)

    urllib.request.urlretrieve(url, dest, reporthook=progress)
    print(f"\r  done.    ")


def main():
    os.makedirs(_MODELS_DIR, exist_ok=True)

    for model in MODELS:
        dest = os.path.join(_MODELS_DIR, model["name"])
        download(model["name"], model["url"], dest)

    print("\nAll models ready.")


if __name__ == "__main__":
    main()