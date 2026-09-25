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
// again and the trims never stop.
int main() {
  for (uint32_t seed : {1u, 7u, 0x6c696665u}) {
    garden::Engine engine(seed);
    const int64_t span = engine.barSamples(), check = garden::rate * 120 / 1000;
    const int64_t origin = span / 3;  // where the shared beat falls
    int64_t trimmed = 0, worst = 0;
    for (int64_t i = 1; i <= int64_t(garden::rate) * 60; ++i) {
      engine.sample();
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
    std::cout << "seed " << seed << ": settles on the shared beat, worst " << worst << " samples after 15 s\n";
  }
}
