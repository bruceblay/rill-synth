// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Garden.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

// The ensemble keeps each device on the shared beat the way main.cpp does:
// every 120 ms it compares where the shared grid is with barPhase() and trims
// a quarter of the difference. Run that loop against a grid well off this
// engine's own, and the engine has to settle onto it and stay there. If
// barPhase() does not move with the trims, the same error is paid again and
// again and the trims never stop. Taps are part of it: a new piece has to
// begin on the grid it was on, not start one of its own.
// A tempo change, as the ensemble makes one: the shared grid takes the new
// tempo on a line, and the engine hears of it at its next 120 ms service.
// It has to stay on the grid through the change, not lose its place.
static void checkTempoChange() {
  for (uint32_t seed : {2u, 3u}) {
    garden::Engine engine(seed);
    engine.followTempo(engine.bpm());
    const unsigned slower = engine.bpm() - 8;
    garden::Engine sizer(seed);
    sizer.followTempo(slower);
    const int64_t check = garden::rate * 120 / 1000;
    int64_t span = engine.barSamples(), origin = span / 3, worst = 0, during = 0, switchAt = -1;
    bool told = false;
    for (int64_t i = 1; i <= int64_t(garden::rate) * 50; ++i) {
      engine.sample();
      // The shared grid changes tempo on its first line after 20 s.
      if (switchAt < 0 && i > int64_t(garden::rate) * 20 && ((i - origin) % span) == 0) {
        switchAt = i; origin = i; span = sizer.barSamples();
      }
      if (i % check) continue;
      if (switchAt >= 0 && !told) { engine.followTempo(slower); told = true; }
      int64_t want = ((int64_t(engine.frames()) - origin) % span + span) % span;
      int64_t error = ((want - int64_t(engine.barPhase())) % span + span) % span;
      if (error > span / 2) error -= span;
      engine.trimGrid(int32_t(error / 4));
      if (switchAt >= 0) {
        if (i < switchAt + int64_t(garden::rate) * 8) during = std::max(during, std::llabs(error));
        else worst = std::max(worst, std::llabs(error));
      }
    }
    assert(engine.bpm() == slower);
    assert(worst < garden::rate * 2 / 1000);
    std::cout << "seed " << seed << ": " << (slower + 8) << " to " << slower << " BPM, worst " << during
              << " samples in the change, " << worst << " after\n";
  }
}

int main() {
  checkTempoChange();
  for (uint32_t seed : {1u, 7u, 0x6c696665u}) {
    garden::Engine engine(seed);
    engine.followTempo(engine.bpm());  // as the ensemble does, every 120 ms
    const unsigned first = engine.variation();
    const int64_t span = engine.barSamples(), check = garden::rate * 120 / 1000;
    const int64_t origin = span / 3;  // where the shared beat falls
    int64_t trimmed = 0, worst = 0;
    for (int64_t i = 1; i <= int64_t(garden::rate) * 60; ++i) {
      engine.sample();
      if (i > int64_t(garden::rate) * 15 && i % (garden::rate * 3) == 0) engine.newVariation();
      if (i % check) continue;
      int64_t want = ((int64_t(engine.frames()) - origin) % span + span) % span;
      int64_t error = ((want - int64_t(engine.barPhase())) % span + span) % span;
      if (error > span / 2) error -= span;
      engine.trimGrid(int32_t(error / 4));
      if (i > int64_t(garden::rate) * 15) { trimmed += std::llabs(error / 4); worst = std::max(worst, std::llabs(error)); }
    }
    // Settled: within a couple of milliseconds, and the trims have stopped.
    assert(worst < garden::rate * 2 / 1000);
    assert(trimmed < span / 4);
    assert(engine.variation() > first + 5);  // the taps did land
    std::cout << "seed " << seed << ": settles on the shared beat, worst " << worst << " samples after 15 s, through taps\n";
  }
}
