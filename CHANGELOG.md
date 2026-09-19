# Changes

## Unreleased — visual study

- Replaced the six visual families. Weave, contour, strata, pendulum, growth and eclipse, all drawn flat: opaque shapes with hard edges, no additive blending, no dithering, no soft falloff. This is Rill Drums' drawing language.
- Replaced the palettes with six daylight ones, each a coloured ground and three inks that sit on it. Levels and bands are an ink or that ink let down toward the ground, never stepped toward black.
- Archived the original six families in `src/LightClassic.h`, unused by the firmware; the retained-behavior test now holds that archive to the historical fixture.
- Raised the display brightness from 55 to 100.
- Not yet judged on hardware beyond a first look.

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
