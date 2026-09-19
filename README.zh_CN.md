<p align="right"><strong>简体中文</strong> | <a href="README.md">English</a></p>

# 单词学习本

![单词学习本封面](docs/development/projects/word-learning-notebook-cover.png)

单词学习本把 FoloToy AI Passport 变成一台离线英语学习设备。固件内置 850 个
Ogden Basic English 单词、中文释义、英文说明、例句、近义词、离线发音、
每日学习卡片、三选一自测、生词本和基础学习统计。

开机后直接进入单词学习本，无需配网，也不依赖手机或电脑。

## 功能

- 浏览全部 850 个单词，或按六类词库筛选。
- 查看中文释义、英文释义、核心意象、例句、例句翻译和近义词。
- 播放每个单词的离线发音。
- 每天学习 10 个单词，按 `OK` 翻面查看释义。
- 完成 10 题三选一自测，即时查看结果并听正确答案。
- 收藏难词，学习状态和收藏进度保存在设备中。
- 查看已掌握、学习中、生词本、本次学习和自测正确率统计。

## 按键

| 页面 | UP / DOWN | OK | 长按 OK |
| --- | --- | --- | --- |
| 首页 | 移动选择 | 进入 | - |
| 列表 | 移动选择 | 打开详情 | 返回 |
| 单词详情 | 上一个 / 下一个单词 | 播放发音 | 返回 |
| 每日学习 | 上一个 / 下一个单词 | 翻面 / 重新播放 | 返回 |
| 单词自测 | 选择答案 | 确认 / 下一题 | 返回 |
| 生词本 | 移动选择 | 打开详情 | 返回 |

在单词详情页双击 `OK`，可以收藏或取消收藏当前单词。

## 构建

环境要求：

- ESP-IDF 5.5.3
- Python 3.10+
- 重新生成字体时需要 Node.js

运行仓库检查并构建合并固件：

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
```

合并固件输出路径：

```text
build/FoloToy-AI-Passport-full.bin
```

从 `0x0` 写入该合并镜像。

## 项目结构

- `main/`：单词本界面、学习状态、自测逻辑、NVS 保存、音频索引和离线播放。
- `components/bsp/`：板级支持层。
- `assets/data/ogden-850.json`：单词数据。
- `assets/audio/ogden_words.adpcm`：离线发音数据。
- `assets/fonts/`：生成的 16 px 和 20 px LVGL 中文字体。
- `docs/development/projects/word-learning-notebook.zh_CN.md`：项目行为与实现说明。

## 重新生成素材

```bash
python tools/build_word_data.py
python tools/build_word_audio.py
python tools/build_fonts.py
```

修改单词数据、音频或字体后，必须重新运行主机测试。

## 许可证

见 [LICENSE](LICENSE)。
