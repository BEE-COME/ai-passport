#!/usr/bin/env python3
"""Fetch and validate the Ogden Basic English 850 data used by the app."""

import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path

EXPECTED_CATEGORIES = {
    "op": 100,
    "gt": 400,
    "pt": 200,
    "qg": 100,
    "qo": 50,
}

DEFAULT_URL = "https://ogden.munch.love/"


def fetch_html(url):
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "FoloToy-Word-Notebook/0.1 (+offline asset build)"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read().decode("utf-8")


def words_from_html(html):
    match = re.search(
        r'<script id="data" type="application/json">(.*?)</script>',
        html,
        re.S,
    )
    if not match:
        raise ValueError("embedded Ogden word JSON was not found in the page")
    words = json.loads(match.group(1))
    if not isinstance(words, list) or len(words) != 850:
        raise ValueError(f"expected 850 words, got {len(words) if isinstance(words, list) else 'invalid json'}")
    return words


def validate(words):
    seen = set()
    counts = {}
    for index, word in enumerate(words):
        try:
            token = word["w"]
            category = word["c"]
            for field in ("zh", "en", "ex", "exz", "core"):
                if not isinstance(word[field], str) or not word[field]:
                    raise ValueError(f"empty {field} at index {index}")
            if not isinstance(word.get("s"), list):
                raise ValueError(f"missing synonym list at index {index}")
        except KeyError as error:
            raise ValueError(f"missing field {error} at index {index}") from error
        if not token or token in seen:
            raise ValueError(f"missing or duplicate word at index {index}: {token!r}")
        seen.add(token)
        counts[category] = counts.get(category, 0) + 1
    if counts != EXPECTED_CATEGORIES:
        raise ValueError(f"unexpected category counts: {counts}")
    return words


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--output", default="assets/data/ogden-850.json")
    parser.add_argument("--html", help="optional local HTML file instead of network")
    args = parser.parse_args()

    if args.html:
        html = Path(args.html).read_text(encoding="utf-8")
    else:
        print(f"Fetching {args.url}", file=sys.stderr)
        html = fetch_html(args.url)

    words = validate(words_from_html(html))
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(words, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    print(f"Wrote {len(words)} words to {output}")


if __name__ == "__main__":
    main()
