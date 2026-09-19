#!/usr/bin/env python3
"""Build LVGL subset fonts covering the exact app + data character set."""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_CONVERTER = (
    r"C:\Users\xia\esp\tools\npm-global\node_modules\lv_font_conv\lv_font_conv.js"
)

UI_TEXT = """
单词学习本 分类浏览 每日学习 单词自测 生词本 学习统计 全部单词
操作词 通用词 图示词 性质词 反义对 中文释义 英文释义 核心意象 例句
例句翻译 近义词 单词详情 收藏 发音 取消 完成 正确 错误 重听 下一题
退出 返回 开始 已掌握 学习中 未学 生词 词 会话 累计
三个任六击务双并库度有次没率答还进选面页首
★☆▶，。：；！？·—（）“”‘’
"""


def collect_symbols(words, fixed_only=False):
    symbols = set(UI_TEXT)
    if fixed_only:
        return "".join(sorted(symbols, key=lambda char: ord(char)))
    for word in words:
        symbols.update(word["w"])
        for field in ("zh", "en", "ex", "exz", "core"):
            symbols.update(word[field])
        for synonym in word.get("s") or []:
            symbols.update(synonym)
    symbols.discard("\n")
    symbols.discard("\r")
    return "".join(sorted(symbols, key=lambda char: ord(char)))


def run_converter(converter, font_path, symbols, size, output):
    node = shutil.which("node")
    if not node:
        raise RuntimeError("node is required to run lv_font_conv")
    command = [
        node,
        converter,
        "--font",
        str(font_path),
        "--symbols",
        symbols,
        "--size",
        str(size),
        "--bpp",
        "4",
        "--format",
        "lvgl",
        "--no-compress",
        "--no-kerning",
        "--lv-include",
        "lvgl.h",
        "--lv-font-name",
        f"app_font_{size}",
        "--output",
        str(output),
    ]
    print("running lv_font_conv for", size)
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--words", default="assets/data/ogden-850.json")
    parser.add_argument("--font", default=r"C:\Users\xia\AppData\Local\Temp\NotoSansCJKsc-Regular.otf")
    parser.add_argument("--converter", default=DEFAULT_CONVERTER)
    parser.add_argument("--output-dir", default="assets/fonts")
    parser.add_argument("--charset", default="assets/fonts/font_charset.txt")
    args = parser.parse_args()

    words = json.loads(Path(args.words).read_text(encoding="utf-8"))
    all_symbols = collect_symbols(words)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    UI_SYMBOLS = collect_symbols(words, fixed_only=True)
    UI_SYMBOLS = UI_SYMBOLS.replace("\n", "").replace("\r", "")
    Path(args.charset).write_text(all_symbols, encoding="utf-8")
    print(f"charset: {len(all_symbols)} unique chars -> {args.charset}")

    for size in (16, 20):
        symbols = all_symbols if size == 16 else UI_SYMBOLS
        run_converter(args.converter, args.font, symbols, size, output_dir / f"app_font_{size}.c")
    print("fonts written to", output_dir)


if __name__ == "__main__":
    main()
