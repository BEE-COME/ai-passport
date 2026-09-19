#!/usr/bin/env python3
"""Verify the generated charset covers every visible UI/data character."""

import json
from pathlib import Path

UI_TEXT = "单词学习本分类浏览每日学习单词自测生词本学习统计全部单词操作词通用词图示词性质词反义对中文释义英文释义核心意象例句例句翻译近义词单词详情收藏发音取消完成正确错误重听下一题退出返回开始已掌握学习中未学生词词会话累计三个任六击务双并库度有次没率答还进选面页首★☆▶，。：；！？·—（）“”‘’"


def main():
    repo = Path(__file__).resolve().parents[1]
    words = json.loads((repo / "assets" / "data" / "ogden-850.json").read_text(encoding="utf-8"))
    charset = set((repo / "assets" / "fonts" / "font_charset.txt").read_text(encoding="utf-8"))
    required = set(UI_TEXT)
    for word in words:
        required.update(word["w"])
        for field in ("zh", "en", "ex", "exz", "core"):
            required.update(word[field])
        for synonym in word.get("s") or []:
            required.update(synonym)
    missing = sorted(required - charset)
    assert not missing, f"missing glyphs: {missing}"
    print("font coverage: PASS")


if __name__ == "__main__":
    main()
