#!/usr/bin/env python3
"""Generate the offline pronunciation blob and index for all 850 words.

Each clip is synthesized as 16-bit mono PCM with eSpeak NG, resampled to
16 kHz, silence-trimmed, and compressed with IMA ADPCM. The clips are joined
into one binary blob that is embedded through target_add_binary_data().
"""

import argparse
import io
import json
import struct
import subprocess
from pathlib import Path

SAMPLE_RATE = 16000
ESPEAK_SAMPLE_RATE = 22050
IMA_STEP = [
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
]


def parse_wav(data):
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE stream")
    offset = 12
    sample_rate = None
    channels = None
    bits = None
    pcm = None
    while offset + 8 <= len(data):
        chunk = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        payload = offset + 8
        if chunk == b"fmt " and size >= 16:
            _, channels, sample_rate, _, _, bits = struct.unpack_from(
                "<HHIIHH", data, payload
            )
        elif chunk == b"data":
            available = len(data) - payload
            pcm = data[payload : payload + min(size, available)]
            break
        offset = payload + size + (size & 1)
    if sample_rate is None or channels is None or bits is None or pcm is None:
        raise ValueError("missing fmt/data chunks")
    if channels != 1 or bits != 16:
        raise ValueError(f"expected mono 16-bit PCM, got ch={channels} bits={bits}")
    return sample_rate, pcm


def resample_pcm(pcm, source_rate):
    source_samples = len(pcm) // 2
    if source_samples == 0:
        return bytearray()
    target_samples = max(1, int(source_samples * SAMPLE_RATE / source_rate))
    out = bytearray(target_samples * 2)
    for index in range(target_samples):
        pos = index * source_rate / SAMPLE_RATE
        left = int(pos)
        right = min(left + 1, source_samples - 1)
        fraction = pos - left
        a = struct.unpack_from("<h", pcm, left * 2)[0]
        b = struct.unpack_from("<h", pcm, right * 2)[0]
        value = int(a + (b - a) * fraction)
        struct.pack_into("<h", out, index * 2, value)
    return out


def trim_silence(pcm, threshold=700, pad_ms=60):
    samples = len(pcm) // 2
    if samples == 0:
        return pcm
    start = 0
    end = samples - 1
    while start < samples and abs(struct.unpack_from("<h", pcm, start * 2)[0]) < threshold:
        start += 1
    while end > start and abs(struct.unpack_from("<h", pcm, end * 2)[0]) < threshold:
        end -= 1
    pad = SAMPLE_RATE * pad_ms // 1000
    start = max(0, start - pad)
    end = min(samples - 1, end + pad)
    return pcm[start * 2 : (end + 1) * 2]


def ima_encode(pcm):
    if not pcm:
        return b""
    first = struct.unpack_from("<h", pcm, 0)[0]
    predictor = first
    step_index = 0
    out = bytearray()
    out += struct.pack("<IhBB", len(pcm) // 2, predictor, step_index, 0)
    for index in range(1, len(pcm) // 2):
        sample = struct.unpack_from("<h", pcm, index * 2)[0]
        difference = sample - predictor
        if difference < 0:
            sign = 8
            difference = -difference
        else:
            sign = 0
        step = IMA_STEP[step_index]
        code = 0
        if difference >= step:
            code = 4
            difference -= step
        if difference >= step // 2:
            code |= 2
            difference -= step // 2
        if difference >= step // 4:
            code |= 1
        code |= sign
        delta = step >> 3
        if code & 4:
            delta += step
        if code & 2:
            delta += step >> 1
        if code & 1:
            delta += step >> 2
        if code & 8:
            predictor -= delta
        else:
            predictor += delta
        if predictor > 32767:
            predictor = 32767
        elif predictor < -32768:
            predictor = -32768
        step_index += IMA_INDEX[code & 7]
        step_index = max(0, min(88, step_index))
        if index & 1:
            out[-1] |= code & 0x0F
        else:
            out.append((code & 0x0F) << 4)
    return bytes(out)


IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def synthesize(espeak, espeak_path, word):
    command = [
        espeak,
        f"--path={espeak_path}",
        "--stdout",
        "-v",
        "en-us",
        "-s",
        "135",
        "-p",
        "50",
        "-a",
        "120",
        word,
    ]
    result = subprocess.run(command, check=True, capture_output=True)
    sample_rate, pcm = parse_wav(result.stdout)
    pcm = resample_pcm(pcm, sample_rate)
    pcm = trim_silence(pcm)
    return ima_encode(pcm)


def synthesize_piper(voice, word):
    frames = []
    for chunk in voice.synthesize(word):
        frames.append(chunk.audio_int16_bytes)
    return b"".join(frames)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--words", default="assets/data/ogden-850.json")
    parser.add_argument("--espeak", default=r"C:\Users\xia\esp\tools\espeak-ng\eSpeak NG\espeak-ng.exe")
    parser.add_argument("--tts", choices=("espeak", "piper"), default="piper")
    parser.add_argument("--piper-model",
                        default=r"C:\Users\xia\esp\tools\piper-voice\en_US-lessac-medium.onnx")
    parser.add_argument("--output", default="assets/audio/ogden_words.adpcm")
    parser.add_argument("--header", default="main/audio_index.h")
    parser.add_argument("--source", default="main/audio_index.c")
    parser.add_argument("--limit", type=int, default=0,
                        help="synthesize only the first N words (default: all)")
    args = parser.parse_args()

    words = json.loads(Path(args.words).read_text(encoding="utf-8"))
    if args.limit > 0:
        words = words[: args.limit]

    piper_voice = None
    piper_voice_rate = None
    if args.tts == "piper":
        from piper import PiperVoice, SynthesisConfig
        piper_voice = PiperVoice.load(args.piper_model)
        piper_voice_rate = piper_voice.config.sample_rate
        print(f"piper voice sample rate: {piper_voice_rate}")
    else:
        espeak_path = Path(args.espeak).parent

    clips = []
    for index, word in enumerate(words, 1):
        if args.tts == "piper":
            sample_rate = piper_voice_rate
            pcm = synthesize_piper(piper_voice, word["w"])
            pcm = resample_pcm(pcm, sample_rate)
            pcm = trim_silence(pcm)
            clip = ima_encode(pcm)
        else:
            clip = synthesize(args.espeak, espeak_path, word["w"])
        if not clip:
            raise RuntimeError(f"empty audio for {word['w']!r}")
        clips.append(clip)
        if index % 100 == 0:
            print(f"synthesized {index}/{len(words)}")

    offsets = []
    blob = bytearray()
    for clip in clips:
        offsets.append(len(blob))
        blob.extend(clip)
    Path(args.output).write_bytes(blob)
    print(f"audio blob: {Path(args.output)} ({len(blob)} bytes)")

    interval = len(offsets) - 1
    lines = [
        f"// Generated by tools/build_word_audio.py; {len(offsets)} clips.",
        f"#include \"audio_index.h\"",
        "",
        f"const uint32_t WORD_AUDIO_OFFSET[WORD_COUNT] = {{",
    ]
    for i in range(0, len(offsets), interval):
        chunk = offsets[i : i + interval]
        lines.append("    " + ", ".join(str(value) for value in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"const uint32_t WORD_AUDIO_LEN[WORD_COUNT] = {{")
    for i in range(0, len(clips), interval):
        chunk = clips[i : i + interval]
        lines.append("    " + ", ".join(str(len(value)) for value in chunk) + ",")
    lines.append("};")
    lines.append("")

    header = """#pragma once

#include <stdint.h>

#include "word_data.h"

extern const uint32_t WORD_AUDIO_OFFSET[WORD_COUNT];
extern const uint32_t WORD_AUDIO_LEN[WORD_COUNT];
"""
    Path(args.header).write_text(header, encoding="utf-8")
    Path(args.source).write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.header} and {args.source}")


if __name__ == "__main__":
    main()
