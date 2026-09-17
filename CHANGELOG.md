# Changes

## Unreleased — visual study

- Replaced the six visual families with a new set drawn as light on a dark ground: current, interference, strata, pendulum, growth and curtains.
- Added a shared drawing material for them: subpixel additive splats, hash-noise dithering that moves every frame, and a decay that returns the frame to ground without leaving residue.
- Archived the original six families in `src/LightClassic.h`, unused by the firmware; the retained-behavior test now holds that archive to the historical fixture.
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
