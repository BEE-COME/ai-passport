<p align="right"><a href="word-learning-notebook.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Word Learning Notebook: Requirements (Draft)

> Status: V1 implemented and build-verified; device testing pending.
> Baseline: FoloToy AI Passport repository `main`.
> Branch: `feature/word-learning-notebook`.
> Data source: <https://ogden.munch.love/> (Ogden Basic English 850).

## 1. Product summary

Word Learning Notebook is an offline vocabulary-learning application for the
FoloToy AI Passport. The firmware embeds all 850 words from Ogden Basic English
850, boots directly into the Chinese application UI with offline word
pronunciation, and does not use the baseline demo menu or pages as the product
shell.

V1 paths:

- Browse by six built-in categories with the full word card.
- Daily study with a 10-word session and flip-to-reveal cards that speak the
  word on reveal.
- Quiz with a Chinese prompt, three English choices, top/middle/bottom buttons
  mapped to the choices, and spoken feedback for the correct answer.
- Starred-words list and cumulative learning statistics persisted in NVS.

## 2. Hardware and BSP boundary

- 240×320 portrait RGB565 display and backlight owned by BSP.
- UP/DOWN/OK ADC buttons; V1 consumes click and long-press events only.
- ES8311 and I2S remain owned by BSP; V1 streams offline word pronunciation.
- CW2017 battery is optional; home screen shows SOC with a graceful fallback.
- NVS 24 KB and the default partition table are preserved.
- Wi-Fi/BLE and network time are out of scope for V1.

Implementation lives in `main` and reuses `components/bsp` unchanged unless a
BSP defect is found. The device boots directly into the app home screen.

## 3. V1 scope

Home: app title, battery indicator, six entries, and a cumulative progress
summary (for example, 120 / 850 mastered).

Browse: All 850, Operations 100, General Things 400, Picturable 200, Qualities
100, Opposites 50. A selected category opens a scrollable word list; the detail
card. Up/down first scroll the detail content and switch words only at the
scroll boundary. OK short plays the word; a quick double press toggles the star;
OK long returns to the list.

Daily study: 10 words sampled in source order, one card at a time. OK flips the
card and plays the word; pressing OK again replays it. UP/DOWN move between
cards. A revealed card is marked as learning.

Quiz: each question shows a Chinese prompt and three English choices laid out
top/middle/bottom, mapped to UP/OK/DOWN. Answer feedback is immediate, the
correct choice is shown and spoken, OK advances, and UP/DOWN replay the word.
A session is 10 questions and updates mastered/learning progress.

Stars and statistics: starred words are listed; stats show mastered, learning,
starred, and session counts. Calendar streaks are not in V1 because the device
has no reliable clock source.

## 4. Interaction and persistence

- Home: UP/DOWN move selection, OK enters, OK long does nothing.
- Subpages: OK long returns to the parent page.
- Button callbacks only enqueue events; UI operations run in the input task
  with the LVGL lock held.
- Tasks and timers touching UI stop before a screen is deleted.
- Per-word state uses 2 bits (new, learning, mastered) and 1 star bit, stored
  as one NVS blob; total state is well under 1 KB.
- NVS writes happen at session end or meaningful state changes, not on every
  key press.

## 5. Offline pronunciation

- Audio is generated at build time and played from firmware; the device never
  accesses the network during use.
- The build machine synthesizes 16 kHz mono PCM for all 850 words with the
  open source Piper neural TTS (eSpeak-NG remains the fallback), then compresses
  it with IMA ADPCM for embedding.
- Playback decodes in small blocks in an audio worker task and streams through
  `bsp_audio_set_format(16000,16,1)` and `bsp_audio_write`; button callbacks and
  the LVGL task never block on audio.
- The 850 clips are expected to use roughly 2-5 MB of Flash before the final
  `idf.py size-components` measurement.
- TTS/encoder versions, commands, and licenses are recorded in
  `assets/audio/README.md`; generated C tables are produced by the same scripts
  used by the build.

## 6. Data and assets

- Extract the 850 records from the embedded
  `<script id="data" type="application/json">` on the data site.
- Fields: `w`, `c`, `zh`, `en`, `ex`, `exz`, `s`, `core`.
- Save source data under `assets/data/ogden-850.json`, record the fetch and
  conversion commands in `assets/data/README.md`, and generate static C tables
  under `main/words_generated.c/h`.
- The extended synonym map on the site is not used in V1.
- Current scale: 850 records, about 123 KB UTF-8 JSON, 1623 distinct Han
  characters in the page data. Flash budget is sufficient.

## 7. Chinese UI and fonts

- Design a new notebook-style theme: light paper background, dark ink text,
  and category color accents. Do not reuse the demo sky/grass/mascot theme.
- Generate LVGL subset fonts from an open-licensed CJK font at 16 px and 20 px,
  covering fixed UI text, all visible data text, full-width punctuation,
  Latin, and digits; fall back to Montserrat for icons.
- Keep LVGL placeholders enabled during development.
- Add a host-side glyph-coverage test over every codepoint actually used.

## 8. Memory and performance

- No PSRAM; keep the 24 KB LVGL pool and the single 240×20 draw buffer.
- Generated word and font data live in Flash as read-only data; no runtime JSON
  parsing.
- Render only visible list rows rather than creating 850 labels.
- Check `idf.py size-components` and record runtime free heap/largest block on
  the device.

## 9. Validation and delivery

Host tests cover data integrity, the study/quiz state machine, generated C
table consistency, audio data integrity, and font glyph coverage. The
repository gate is:

```text
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The verified merged image is `build/FoloToy-AI-Passport-full.bin`, written at
`0x0` only after explicit approval. Device acceptance includes Chinese
rendering, navigation, battery fallback, persistence after reboot, and repeated
page-entry stability. Results are reported separately as Build, Host tests,
Device tests, and Unverified.

## 10. Confirmed defaults

- Offline pronunciation is included: build-time TTS, IMA ADPCM, streamed
  playback on the device.
- Each study session contains 10 words sampled in source order.
- The device boots directly into the word notebook, replacing the demo entry.
- No Wi-Fi/BLE, network time, or extra data partition.
- All 850 words are supported; statistics are cumulative and session-based.

Scope confirmed; implementation starts.
