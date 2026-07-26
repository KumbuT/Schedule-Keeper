#!/usr/bin/env python3
"""
generate_audio.py — Generate the schedule-tracker voice prompts.

Produces the four WAV clips AudioManager plays from LittleFS:

    starting.wav   "Starting your task now."
    halfway.wav    "You are halfway through."
    onemin.wav     "One minute remaining."
    done.wav       "Task complete. Well done."

Output format matches AudioManager::_consumeWavHeader() expectations:
    mono, 16-bit PCM, 8 kHz (RIFF/WAVE, 44-byte header).

------------------------------------------------------------------------------
ENGINES
------------------------------------------------------------------------------
This script picks a TTS engine automatically, preferring the best available:

  1. --engine say     macOS built-in `say` (neural voices, best quality).
                      Only on macOS. Pick a voice with --voice "Samantha".
                      List voices:  say -v '?'

  2. --engine espeak  espeak-ng via the pip package `espeakng-loader`
                      (bundles the library + data — no apt/root needed, works
                      on Linux/macOS/Windows, fully offline). Robotic but clear.
                      This is the default fallback and needs no model download.
                      Install:  pip install espeakng-loader

  3. --engine piper   Piper neural TTS, if you have a voice model locally.
                      Point at it with --piper-model /path/to/voice.onnx
                      (the matching voice.onnx.json must sit beside it).
                      Install:  pip install piper-tts

All engines pipe through ffmpeg for the final 8 kHz / mono / 16-bit downsample,
so ffmpeg must be on PATH.

------------------------------------------------------------------------------
EXAMPLES
------------------------------------------------------------------------------
  # Default (espeak-ng, zero setup beyond pip + ffmpeg):
  python3 generate_audio.py

  # Best quality on a Mac:
  python3 generate_audio.py --engine say --voice "Samantha"

  # Neural, with a downloaded Piper model:
  python3 generate_audio.py --engine piper \
      --piper-model voices/en_US-lessac-medium.onnx

  # Change the wording or the output rate:
  python3 generate_audio.py --rate 16000
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import wave

# ---------------------------------------------------------------------------
# The clips. Key -> (filename, spoken text).
# Keep filenames in sync with AudioManager / main.cpp defaults.
# ---------------------------------------------------------------------------
CLIPS = {
    "starting": ("starting.wav", "Let's get started! You can do it."),
    "halfway":  ("halfway.wav",  "You're halfway there. Keep it up!"),
    "onemin":   ("onemin.wav",   "Almost done! Just one more minute."),
    "done":     ("done.wav",     "All done! Great job!"),
}

DEFAULT_OUT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", "data", "audio")
)


# ---------------------------------------------------------------------------
# Engine: espeak-ng via bundled library (default, offline, no root)
# ---------------------------------------------------------------------------
def synth_espeak(text, wav_path, speed=160, pitch=60, prange=60, voice="en-us",
                 volume=100):
    import ctypes
    import espeakng_loader

    lib = espeakng_loader.get_library_path()
    data = espeakng_loader.get_data_path()          # .../espeak-ng-data
    parent = os.path.dirname(data)                  # dir CONTAINING espeak-ng-data

    e = ctypes.CDLL(lib)
    e.espeak_Initialize.restype = ctypes.c_int
    e.espeak_Initialize.argtypes = [ctypes.c_int, ctypes.c_int,
                                    ctypes.c_char_p, ctypes.c_int]
    AUDIO_OUTPUT_RETRIEVAL = 1
    rate = e.espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 0, parent.encode(), 0)
    if rate < 0:
        raise RuntimeError("espeak_Initialize failed")

    pcm = bytearray()
    CBTYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                              ctypes.POINTER(ctypes.c_short),
                              ctypes.c_int, ctypes.c_void_p)

    def _cb(wav, n, events):
        if n > 0:
            pcm.extend(ctypes.string_at(wav, n * 2))
        return 0

    cb = CBTYPE(_cb)
    e.espeak_SetSynthCallback(cb)
    e.espeak_SetVoiceByName(voice.encode())
    e.espeak_SetParameter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    e.espeak_SetParameter(1, speed, 0)   # 1 = rate (wpm)
    e.espeak_SetParameter(2, volume, 0)  # 2 = volume (0-200)
    e.espeak_SetParameter(3, pitch, 0)   # 3 = pitch
    e.espeak_SetParameter(4, prange, 0)  # 4 = pitch range (melodic warmth)

    b = text.encode("utf-8")
    espeakCHARS_UTF8 = 1
    e.espeak_Synth.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint,
                               ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                               ctypes.POINTER(ctypes.c_uint), ctypes.c_void_p]
    e.espeak_Synth(b, len(b) + 1, 0, 0, 0, espeakCHARS_UTF8, None, None)
    e.espeak_Synchronize()

    with wave.open(wav_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(pcm))
    return rate


# ---------------------------------------------------------------------------
# Engine: macOS `say` (best quality on a Mac)
# ---------------------------------------------------------------------------
def synth_say(text, wav_path, voice=None):
    if not shutil.which("say"):
        raise RuntimeError("`say` not found (macOS only)")
    aiff = wav_path + ".aiff"
    cmd = ["say"]
    if voice:
        cmd += ["-v", voice]
    cmd += ["-o", aiff, text]
    subprocess.run(cmd, check=True)
    # afconvert (also built into macOS) -> wav; ffmpeg step below normalises it.
    if shutil.which("afconvert"):
        subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16",
                        aiff, wav_path], check=True)
        os.remove(aiff)
    else:  # fall back to ffmpeg for the aiff->wav step
        subprocess.run(["ffmpeg", "-y", "-i", aiff, wav_path],
                       check=True, capture_output=True)
        os.remove(aiff)


# ---------------------------------------------------------------------------
# Engine: Piper neural TTS (requires a local .onnx voice model)
# ---------------------------------------------------------------------------
def synth_piper(text, wav_path, model):
    if not model or not os.path.exists(model):
        raise RuntimeError("--piper-model must point at an existing .onnx voice")
    # piper reads text on stdin, writes a wav to --output_file
    subprocess.run([sys.executable, "-m", "piper",
                    "--model", model, "--output_file", wav_path],
                   input=text.encode("utf-8"), check=True)


# ---------------------------------------------------------------------------
# Engine: Microsoft Edge neural TTS (free, no API key -- needs internet)
# ---------------------------------------------------------------------------
def synth_edge(text, wav_path, voice="en-US-AnaNeural"):
    # Genuinely natural neural voices, free and keyless, via the `edge-tts`
    # package (pip install edge-tts). Requires an internet connection at
    # generate time. Good kid-friendly choices:
    #   en-US-AnaNeural   -- young / child voice (warm, gentle)
    #   en-US-JennyNeural -- warm friendly adult female
    #   en-US-AriaNeural  -- clear friendly adult female
    # List all: `edge-tts --list-voices`.
    edge = shutil.which("edge-tts")
    mp3 = wav_path + ".mp3"
    cmd = ([edge] if edge else [sys.executable, "-m", "edge_tts"]) + \
          ["--voice", voice, "--text", text, "--write-media", mp3]
    subprocess.run(cmd, check=True)
    subprocess.run(["ffmpeg", "-y", "-i", mp3, wav_path],
                   check=True, capture_output=True)
    os.remove(mp3)


# ---------------------------------------------------------------------------
# Final normalise to the AudioManager spec via ffmpeg
# ---------------------------------------------------------------------------
def to_device_wav(src, dst, rate):
    # AudioManager::_consumeWavHeader() reads a FIXED 44-byte header and treats
    # everything after as raw PCM. By default ffmpeg writes an ISFT/LIST metadata
    # chunk that pushes the 'data' chunk past byte 44, which the firmware would
    # then play as a burst of noise. -map_metadata -1 + -bitexact suppress that
    # chunk so the layout is exactly: RIFF(12) + fmt(24) + data-header(8) = 44,
    # with PCM samples starting at byte 44.
    subprocess.run(
        ["ffmpeg", "-y", "-i", src,
         "-ar", str(rate), "-ac", "1", "-acodec", "pcm_s16le",
         "-map_metadata", "-1", "-flags", "+bitexact", "-fflags", "+bitexact",
         dst],
        check=True, capture_output=True,
    )


def pick_engine(requested):
    if requested != "auto":
        return requested
    if shutil.which("say"):
        return "say"
    try:
        import espeakng_loader  # noqa: F401
        return "espeak"
    except ImportError:
        pass
    return "espeak"  # will raise a helpful error if truly unavailable


def main():
    ap = argparse.ArgumentParser(description="Generate schedule-tracker voice prompts.")
    ap.add_argument("--engine", choices=["auto", "say", "espeak", "piper", "edge"],
                    default="auto", help="TTS engine (default: auto)")
    ap.add_argument("--voice", default=None,
                    help="Voice name (say: e.g. 'Samantha'; espeak: e.g. 'en-us')")
    ap.add_argument("--piper-model", default=None,
                    help="Path to a Piper .onnx voice model (engine=piper)")
    ap.add_argument("--rate", type=int, default=16000,
                    help="Output sample rate in Hz (default 16000; 8000 is "
                         "telephone-quality and sounds crackly)")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="Output directory (default: ../data/audio)")
    ap.add_argument("--soft", action="store_true",
                    help="espeak: soothing, kid-friendly preset (gentle female "
                         "voice en-us+f3, slower pace, warm pitch). Override any "
                         "piece with --voice/--speed/--pitch/--range/--volume.")
    ap.add_argument("--speed", type=int, default=None, help="espeak words/min")
    ap.add_argument("--pitch", type=int, default=None, help="espeak pitch 0-99")
    ap.add_argument("--range", type=int, default=None, dest="prange",
                    help="espeak pitch range 0-99 (higher = more melodic)")
    ap.add_argument("--volume", type=int, default=None, help="espeak volume 0-200")
    args = ap.parse_args()

    # espeak tuning: start from the --soft preset if requested, then let any
    # explicit flag override.
    esp = dict(voice="en-us", speed=160, pitch=60, prange=60, volume=100)
    if args.soft:
        esp.update(voice="en-us+f3", speed=138, pitch=62, prange=72, volume=95)
    if args.voice:  esp["voice"]  = args.voice
    if args.speed  is not None: esp["speed"]  = args.speed
    if args.pitch  is not None: esp["pitch"]  = args.pitch
    if args.prange is not None: esp["prange"] = args.prange
    if args.volume is not None: esp["volume"] = args.volume

    if not shutil.which("ffmpeg"):
        sys.exit("ERROR: ffmpeg not found on PATH — required for the 8kHz downsample.")

    engine = pick_engine(args.engine)
    os.makedirs(args.out, exist_ok=True)
    print(f"Engine: {engine}   Rate: {args.rate} Hz   Out: {args.out}")

    with tempfile.TemporaryDirectory() as tmp:
        for key, (fname, text) in CLIPS.items():
            raw = os.path.join(tmp, key + "_raw.wav")
            dst = os.path.join(args.out, fname)

            if engine == "espeak":
                synth_espeak(text, raw, **esp)
            elif engine == "say":
                synth_say(text, raw, voice=args.voice)
            elif engine == "edge":
                synth_edge(text, raw, voice=(args.voice or "en-US-AnaNeural"))
            elif engine == "piper":
                synth_piper(text, raw, args.piper_model)
            else:
                sys.exit(f"Unknown engine: {engine}")

            to_device_wav(raw, dst, args.rate)

            with wave.open(dst) as w:
                dur = w.getnframes() / float(w.getframerate())
            print(f"  {fname:14s} {dur:4.1f}s  \"{text}\"")

    print("Done.")


if __name__ == "__main__":
    main()
