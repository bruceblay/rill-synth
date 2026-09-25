<img src="docs/images/visuals-study.png" alt="Current Rill Synth visuals: Contour, Pendulum, Growth, Eclipse, Tiles and Reef" width="800">

# Rill Synth

**rill** /rɪl/ *noun* — a small stream or a tiny, shallow channel cut into soil by running water.

A generative synthesizer for the **M5Stack StickS3**. Seven voices compose delicate, evolving melodies with generative visuals. Tap for a new piece. Shake for a new visual. Sound and visuals run entirely on the device, without Wi-Fi or an account.

[Play Rill Synth](https://rillsound.com/synth) · [Build and install](#build-and-install)

**Rill family:** [Synth](https://github.com/bruceblay/rill-synth) · [Mallet](https://github.com/bruceblay/rill-mallet) · [World](https://github.com/bruceblay/rill-world) · [Drums](https://github.com/bruceblay/rill-drums) · [Rill Sound](https://rillsound.com)

## Visuals

These are captures from the current renderer, driven by the Synth engine, not photographs of the device. Firmware releases published before these visuals show the earlier set.

| Contour | Pendulum |
| --- | --- |
| ![Contour](docs/images/current-contour.png) | ![Pendulum](docs/images/current-pendulum.png) |
| **Growth** | **Eclipse** |
| ![Growth](docs/images/current-growth.png) | ![Eclipse](docs/images/current-eclipse.png) |
| **Tiles** | **Reef** |
| ![Tiles](docs/images/current-tiles.png) | ![Reef](docs/images/current-reef.png) |

- **Contour** — two travelling wave sources and a drifting plane wave sum into a field, and the field is quantised into flat areas of ink and tint, the way a printed contour map is.
- **Pendulum** — a harmonograph traces a figure slowly enough to follow, in long passes of flat colour, then the page is changed and a different figure begins.
- **Growth** — branches cross the page from one side, splitting as they go and stopping where they meet ground already taken.
- **Eclipse** — flat discs and punched rings drift past each other, and the composition is whatever their overlaps happen to make.
- **Tiles** — a grid of flat squares where each note lights its own column, fading back so the grid settles when the music stops.
- **Reef** — a Gray-Scott reaction, the chemistry behind both brain coral and fingerprints, run at half resolution and read at full so its edges stay organic.

Everything is opaque shapes with hard edges: filled spans, discs, rings punched back to the ground, single-pixel lines. There is no additive blending, dithering or soft falloff anywhere in the renderer. The palettes are daylight ones, a coloured ground with three inks that sit on it, in the register of a faded photograph rather than of emitted light.

Except Contour, the families answer individual notes as well as output level, since a level
meter cannot tell one note from two and says nothing about pitch. What a note
does differs by family, because a drawing and a field cannot take an event the
same way. The pendulum leaves a bead on its line where the pen was when the
note sounded. A note splits a living tip in growth, lights a column in tiles,
pulses the body its pitch points at in eclipse, and seeds a colony in the
reef. Contour keeps its slow evolution and gentle level response, without
per-note shifts or pulses.

## Play

| Gesture | Action |
| --- | --- |
| Front button: tap | Generate a new musical piece, change the visual, and play |
| Front button: hold for about 0.65 seconds | Fade sound out or in; the composition continues while quiet |
| Side button: tap | Cycle volume and show the data view for four seconds |
| Shake | Immediately switch to a different visual family and composition |

The data view shows the voice, key, mode, generation number, tempo, delay rhythm, volume and battery estimate. New musical generations also select a new visual. Shake changes only the visual. The gesture uses two acceleration peaks and a short cooldown; a single tilt is not a shake.

## Sound

- **Seven timbres:** Bongo, Bars, Wood, Bells, Wire, Halo and Synth. These combine resonant modes, plucked tones, FM and filtered oscillators; they are interpretations, not exact hardware or acoustic-instrument emulations.
- **Generated phrases:** six contour tendencies guide newly composed melodies, variable phrase spans, interval preferences and rhythms. Ideas develop through changed endings, rhythmic rephrasing, recalled fragments and new descendants. Sparse answering parts follow their own timing. Twelve tonics, three modes, four harmonic behaviors and gradual changes in activity give each piece its own phrasing.
- **Evolving echoes:** two tempo-related taps, smooth or stepped feedback, occasional stronger repeat passages and intermittent smearing.
- **Six visual families:** a contour field, a drawing pendulum, branching growth, drifting discs, a grid lit note by note, and a reaction-diffusion reef. Each shake selects a different family. All six are drawn flat, in opaque shapes with hard edges on a coloured ground, and drift through slow parameter changes, so a family returned to minutes later is not the arrangement it was. The pendulum and growth run a life of their own: they arrive, fill the page, and the page is changed.

New music fades between generations. Generations are not saved across restarts. Near other Rill devices (Synth, Mallet, Drums or World) it joins an ensemble over ESP-NOW with no setup: the lowest device id keeps the clock, every device plays on the shared tempo and bar line, and a new piece on a Synth or Mallet proposes its key to the others. See [how it works](SYNC-DESIGN.md).

## Hardware

Supported and tested: **M5Stack StickS3**, with ESP32-S3, 8 MB flash, display, IMU and built-in speaker. Other ESP32 boards and earlier M5Stick models are not supported by this configuration.

The PlatformIO board name is `esp32-s3-devkitc-1`; the project supplies the StickS3 memory settings and uses M5Unified for board peripherals.

## Build and install

Install Python 3.11 or later, then run these commands from the repository root:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
pio run
```

On Windows, activate with `.venv\Scripts\activate` instead. PlatformIO downloads the pinned platform and library dependencies on the first build.

Connect the StickS3 with a USB data cable, locate its port with `pio device list`, then install:

```sh
python tools/flash.py --port YOUR_DEVICE_PORT
```

Flashing replaces the firmware currently on the device. The script builds and uploads, then applies the watchdog reset used successfully during development; a normal RTS reset can leave this board in download mode.

To observe diagnostics:

```sh
pio device monitor --port YOUR_DEVICE_PORT --baud 115200
```

Close the monitor before another upload. If the device is not detected, check the cable and port permissions and consult the [StickS3 documentation](https://docs.m5stack.com/en/core/StickS3).

## Develop without hardware

Host tools use the same C++ synthesis and visual code as the firmware. A C++17 compiler is required.

```sh
python tools/test.py
mkdir -p build
c++ -std=c++17 -O2 tools/render.cpp -o build/render
build/render build/rill.wav 60 42 2
c++ -std=c++17 -O2 tools/visual_preview.cpp -o build/visual_preview
build/visual_preview build/preview.ppm 17 2
```

The audio arguments are output path, seconds, seed, and optional first voice (0–6). The visual arguments are output path, seed, and optional family (0–5). Audio output is mono 32 kHz / 16-bit WAV; visual output is PPM. For host address/undefined-behavior checks, run `python tools/test.py --sanitize` with a compatible compiler. Set `CXX` to choose a compiler.

Tests cover thirty simulated minutes of music, bounded output, key/register constraints, live transitions, reproducibility, all musical and visual families, shake detection, and retained historical behaviors. They do not replace listening or checking the physical screen.

## Project layout

- `src/Garden.h` — synthesis, score and effects
- `src/Light.h` — procedural visual families
- `src/LightClassic.h` — the archived original six families, unused by the firmware and held to their historical baseline by the tests
- `src/main.cpp` — audio, display, buttons and motion tasks
- `src/ShakeDetector.h` — gesture recognition
- `tools/` — portable tests, auditions, previews and flashing
- `tests/` — host verification, with required historical baselines in `fixtures/`

See [publishing notes](docs/PUBLISHING.md), [changes](CHANGELOG.md), and [contributing](CONTRIBUTING.md).

## Credits and license

Created by Bruce Blay. Developed through iterative on-device listening and viewing, with Codex assisting implementation.

Brian Eno's generative work, Cyma Forma's RND synth, and Zach Lieberman's daily sketches helped inform the direction. Rill is an independent project, with no affiliation or endorsement implied. Device photos by Bruce Blay show Rill running on the hardware; no artwork by those artists is bundled.

Rill follows its parent project Pocket Radio's **GPL-3.0-or-later** license. See [LICENSE](LICENSE). Third-party components retain their own licenses; see [dependency notices](docs/DEPENDENCIES.md).
