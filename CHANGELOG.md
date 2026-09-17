# Changes

## Unreleased — visual study

- Replaced the six visual families with a new set drawn as light on a dark ground: current, interference, strata, pendulum, growth and curtains.
- Added a shared drawing material for them: subpixel additive splats, hash-noise dithering that moves every frame, and a decay that returns the frame to ground without leaving residue.
- Archived the original six families in `src/LightClassic.h`, unused by the firmware; the retained-behavior test now holds that archive to the historical fixture.
- Slowed the interference field to about a third of its first speed.
- Reworked the pendulum: the pen now moves slowly enough to be followed, its envelope bottoms out instead of collapsing, and a new figure starts every twenty seconds or so, so it never settles into a filled shape.
- Took Rill Drums' four palettes and ground unchanged, so the two instruments share a colour. Strata blends its bands between two inks rather than filling a large area with one flat primary.
- Raised the display brightness from 55 to 100 and lifted the dimmer families, after finding that mid-tones which read on a monitor disappear on the panel.
- Fixed growth roots spawning outside the escape bound, where they died on their first step and left the screen blank; raised its crowding threshold to match the brighter strokes.
- Not yet seen on hardware. The frame times are host measurements.

## 0.2.0 — 2026-09-12

- Replaced fixed eight-step motifs and rhythm cells with generated phrases and cumulative development.
- Added independently timed sparse answers, changing activity, and sustained, pendulum, wandering or pedal harmony with nearby supporting chord tones.
- Kept the existing synthesis voices and controls.

## 0.1.0 — 2026-09-12

- Extended the volume cycle through 76%, 88% and 100%, keeping the 64% startup level.
- Simplified the data-view title to RILL.
- New musical generations also generate a new visual; shake remains visual-only.

## Initial repository — Study 16

First standalone source publication candidate, collecting the on-device prototype developed through sixteen studies.

- Seven curated synthesis families; six compositional characters; three interval palettes; twelve tonics and three modes.
- Phrase-shaped duration and dynamics, rests, octave answers and independent performance variation.
- Two-tap tempo-related delay with evolving feedback and optional smear.
- Six independent visual families, immediate shake regeneration, landscape ring sculpture, and evolving particle/tile/reflection arrangements.
- Large temporary data view with volume and battery estimate.
- Host DSP, visual, shake and historical regression tests.

Earlier snapshots in `studies/` are development references, not supported releases. Ensemble sync remains a proposal.
