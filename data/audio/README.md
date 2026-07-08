# Audio Files

Place converted WAV files here. Generate using tools/generate_audio.py

Required files:
  starting.wav   — "Starting your task now."
  halfway.wav    — "You are halfway through."
  onemin.wav     — "One minute remaining."
  done.wav       — "Task complete. Well done."

Format: 8kHz, mono, 16-bit PCM WAV
Convert: ffmpeg -i input.mp3 -ar 8000 -ac 1 -acodec pcm_s16le output.wav
