#!/usr/bin/env python3
"""Build a self-contained HTML simulator for the Word Learning Notebook UI."""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--words", default="assets/data/ogden-850.json")
    parser.add_argument("--template", default="tools/ui_simulator_template.html")
    parser.add_argument("--output", default="docs/development/projects/word-learning-notebook-simulator.html")
    args = parser.parse_args()

    words = json.loads(Path(args.words).read_text(encoding="utf-8"))
    if not isinstance(words, list) or len(words) != 850:
        raise ValueError("expected the 850-word Ogden dataset")

    template = Path(args.template).read_text(encoding="utf-8")
    marker = "__OGDEN_WORDS_JSON__"
    if marker not in template:
        raise ValueError(f"template is missing {marker} marker")

    rendered = template.replace(
        marker,
        json.dumps(words, ensure_ascii=False, separators=(",", ":")),
    )
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(rendered, encoding="utf-8")
    print(f"Wrote {output} ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
