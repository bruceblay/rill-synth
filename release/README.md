# Rill Synth 0.4.0

Current StickS3 firmware: **0.4.0**, submitted to the canonical [rill-synth M5Burner listing](https://burner.m5stack.com/firmware/2102134892988723201) on September 29, 2026. It is awaiting review; 0.3.0 remains the public store version.

This release adds ESP-NOW ensemble timing and key sharing with the other Rill instruments. New pieces enter on a shared bar line. Holding the side button slows the ensemble by 4 BPM, wrapping from below 52 to 100 BPM. Seven voices accompany the current six visuals: Contour, Pendulum, Growth, Eclipse, Tiles and Reef.

The factory image is `rill-synth-0.4.0-factory.bin`, 8 MB, flashed at **0x0**. Installation replaces existing firmware and settings. The M5Burner cover is the web app's `social/synth-v4.png`.

See the [current family release record](https://github.com/bruceblay/rill-sound/blob/main/release/m5burner/README.md), [source manifest](https://github.com/bruceblay/rill-sound/blob/main/release/m5burner/rill-synth-manifest.json), and [build instructions](../README.md#build-and-install). The previous 0.2.0 release notes and source archive details are preserved in [README-0.2.0.md](README-0.2.0.md).

Original code is GPL-3.0-or-later. Dependencies retain their notices; see [dependencies](../docs/DEPENDENCIES.md).
