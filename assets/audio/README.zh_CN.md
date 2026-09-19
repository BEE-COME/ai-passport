<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# 离线单词发音

- `ogden_words.adpcm` 由 `tools/build_word_audio.py` 生成，默认使用
  Piper `en_US-lessac-medium`（MIT 风格许可的神经语音），回退引擎为
  eSpeak NG 1.52.0（GPL-3.0），统一为 16 kHz 单声道后做 IMA ADPCM 编码。
- 资产通过 `target_add_binary_data(... RENAME_TO ogden_words_blob)` 嵌入，
  索引偏移位于 `main/audio_index.c`。
- 设备播放完全不依赖网络。重新生成时保留 Piper 语音/模型许可证与脚本参数。
