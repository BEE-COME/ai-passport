#!/usr/bin/env python3
"""Generate main/word_data.{h,c} from assets/data/ogden-850.json."""

import argparse
import json
from pathlib import Path

CATEGORY_ORDER = ("op", "gt", "pt", "qg", "qo")
CATEGORY_ZH = {
    "op": "操作词",
    "gt": "通用词",
    "pt": "图示词",
    "qg": "性质词",
    "qo": "反义对",
}


def c_string(value):
    if value is None:
        return '""'
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
    )
    return f'"{escaped}"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--words", default="assets/data/ogden-850.json")
    parser.add_argument("--header", default="main/word_data.h")
    parser.add_argument("--source", default="main/word_data.c")
    args = parser.parse_args()

    words = json.loads(Path(args.words).read_text(encoding="utf-8"))
    if len(words) != 850:
        raise ValueError("expected 850 words")

    counts = {category: 0 for category in CATEGORY_ORDER}
    for word in words:
        category = word["c"]
        if category not in counts:
            raise ValueError(f"unexpected category {category!r}")
        counts[category] += 1

    header = """#pragma once

#include <stdint.h>

#define WORD_COUNT 850
#define WORD_CATEGORY_COUNT 5

typedef struct {
    const char *w;
    const char *zh;
    const char *en;
    const char *ex;
    const char *exz;
    const char *core;
    const char *syn[3];
    uint8_t cat;
} word_entry_t;

extern const word_entry_t WORD_LIST[WORD_COUNT];
extern const char *const WORD_CATEGORY_ZH[WORD_CATEGORY_COUNT];
extern const uint32_t WORD_CATEGORY_COUNTS[WORD_CATEGORY_COUNT];
"""
    Path(args.header).write_text(header, encoding="utf-8")

    lines = [
        '#include "word_data.h"',
        "",
        "const word_entry_t WORD_LIST[WORD_COUNT] = {",
    ]
    for word in words:
        synonyms = word.get("s") or []
        syn_text = ", ".join(c_string(item) for item in synonyms[:3])
        while synonyms and len(synonyms) < 3:
            syn_text += ', ""'
            synonyms = synonyms + [""]
        lines.append(
            "    {"
            f" .w = {c_string(word['w'])},"
            f" .zh = {c_string(word['zh'])},"
            f" .en = {c_string(word['en'])},"
            f" .ex = {c_string(word['ex'])},"
            f" .exz = {c_string(word['exz'])},"
            f" .core = {c_string(word['core'])},"
            f" .syn = {{ {syn_text} }},"
            f" .cat = {CATEGORY_ORDER.index(word['c'])}"
            " },"
        )
    lines.append("};")
    lines.append("")
    lines.append(
        "const char *const WORD_CATEGORY_ZH[WORD_CATEGORY_COUNT] = {"
        + ", ".join(c_string(CATEGORY_ZH[category]) for category in CATEGORY_ORDER)
        + "};"
    )
    lines.append("")
    lines.append(
        "const uint32_t WORD_CATEGORY_COUNTS[WORD_CATEGORY_COUNT] = {"
        + ", ".join(str(counts[category]) for category in CATEGORY_ORDER)
        + "};"
    )
    lines.append("")

    Path(args.source).write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.header} and {args.source}")


if __name__ == "__main__":
    main()
