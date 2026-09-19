#!/usr/bin/env python3
"""Check the generated offline pronunciation blob and its C index agree."""

import re
from pathlib import Path


def main():
    repo = Path(__file__).resolve().parents[1]
    blob = (repo / "assets" / "audio" / "ogden_words.adpcm").read_bytes()
    source = (repo / "main" / "audio_index.c").read_text(encoding="utf-8")

    offsets_block = re.search(r"WORD_AUDIO_OFFSET\[WORD_COUNT\] = \{(.*?)\};", source, re.S).group(1)
    lengths_block = re.search(r"WORD_AUDIO_LEN\[WORD_COUNT\] = \{(.*?)\};", source, re.S).group(1)
    offsets = [int(value) for value in re.findall(r"\d+", offsets_block)]
    lengths = [int(value) for value in re.findall(r"\d+", lengths_block)]

    assert len(offsets) == 850, f"expected 850 offsets, got {len(offsets)}"
    assert len(lengths) == 850, f"expected 850 lengths, got {len(lengths)}"
    assert offsets[0] == 0
    for index in range(1, 850):
        assert offsets[index] == offsets[index - 1] + lengths[index - 1]
    assert offsets[-1] + lengths[-1] == len(blob)
    assert blob[:4] != b"RIFF"
    print("audio assets: PASS")


if __name__ == "__main__":
    main()
