#!/usr/bin/env bash
# gen_audio.sh — generates a test audio file and its C header
#
# Usage: gen_audio.sh <wav|mp3|ogg> <out_header_path> [out_data_path]
#
# Pipeline:
#   1. Python synthesizes a 1.5s 22050Hz mono 16-bit PCM sine wave
#      (musical A4 = 440Hz) with a 20ms fade-in/out to avoid clicks.
#   2. ffmpeg encodes to the requested container (or for wav, just wraps PCM).
#   3. xxd -i emits a `static const unsigned char <name>[]` array.
#
# The output header is included by the ROM main.c. The audio bytes live
# in the WASM data section, so the decoder reads them straight from
# ROM memory at startup — no external file, no I/O.

set -euo pipefail

FMT="${1:-wav}"
OUT_HEADER="${2:?usage: gen_audio.sh <wav|mp3|ogg> <out_header> [out_data]}"
OUT_DATA="${3:-${OUT_HEADER%.h}.bin}"
NAME_DEFAULT="$(basename "${OUT_HEADER%.h}")"

case "$FMT" in
  wav) ENCODER_ARGS=(-c:a pcm_s16le -f wav)  ;;
  mp3) ENCODER_ARGS=(-c:a libmp3lame -b:a 96k -f mp3) ;;
  ogg) ENCODER_ARGS=(-c:a libvorbis -q:a 4 -f ogg) ;;
  *)   echo "unknown format: $FMT (use wav, mp3, or ogg)" >&2; exit 1 ;;
esac

# 1. synthesize WAV via Python (avoids pulling in sox/lame at this stage)
TMP_WAV="$(mktemp --suffix=.wav)"
python3 - <<PY
import math, struct, wave
sr, dur, freq = 22050, 1.5, 440.0
n = int(sr * dur)
fade_in  = int(sr * 0.020)   # 20ms attack
fade_out = int(sr * 0.200)   # 200ms release — needed because the ROM
                              # loops the audio forever; without a long
                              # release, every loop boundary pops
with wave.open("${TMP_WAV}", "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
    frames = bytearray()
    for i in range(n):
        env = 1.0
        if i < fade_in:            env = i / fade_in
        elif i > n - fade_out:      env = (n - i) / fade_out
        s = int(0.30 * env * 32767 * math.sin(2 * math.pi * freq * i / sr))
        frames += struct.pack("<h", max(-32768, min(32767, s)))
    w.writeframes(bytes(frames))
PY

# 2. encode to target format
ffmpeg -y -loglevel error -i "$TMP_WAV" "${ENCODER_ARGS[@]}" "$OUT_DATA"
rm -f "$TMP_WAV"

# 3. emit C array with a fixed, predictable name
xxd -i "$OUT_DATA" \
  | sed -e '1s/.*/static const unsigned char audio_bin[] = {/' \
        -e 's/^unsigned int [a-zA-Z0-9_]*_len = \([0-9]*\);/static const unsigned int audio_bin_len = \1;/' \
  > "$OUT_HEADER"

# 4. report
SIZE=$(stat -c %s "$OUT_DATA")
printf "  %-6s  %8d bytes  →  %s\n" "$FMT" "$SIZE" "$OUT_HEADER"
