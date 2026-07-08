# generate_audio.py — run once on your PC, outputs directly into data/audio/
from gtts import gTTS
import subprocess, os

prompts = {
    "starting": "Starting your task now.",
    "halfway":  "You are halfway through.",
    "onemin":   "One minute remaining.",
    "done":     "Task complete. Well done.",
}

os.makedirs("data/audio", exist_ok=True)

for name, text in prompts.items():
    mp3_path = f"/tmp/{name}.mp3"
    wav_path = f"data/audio/{name}.wav"
    gTTS(text=text, lang='en').save(mp3_path)
    subprocess.run([
        "ffmpeg", "-y", "-i", mp3_path,
        "-ar", "8000", "-ac", "1", "-acodec", "pcm_s16le",
        wav_path
    ])
    print(f"Generated: {wav_path}")
    os.remove(mp3_path)
