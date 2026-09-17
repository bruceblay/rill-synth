// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#define light previous_light
#include "fixtures/visuals-before.h"
#undef light
#include "../src/LightClassic.h"
#include <cassert>
#include <memory>
// The original six families are archived rather than deleted. This holds the
// archive to the historical fixture the way the firmware header used to be
// held to it: the two oldest families pixel for pixel, the family sequence
// throughout. The later four were deliberately revised after the fixture was
// taken, so only their ordering is checked.
int main() {
  auto before = std::unique_ptr<previous_light::Painting>(new previous_light::Painting(17));
  auto after = std::unique_ptr<light_classic::Painting>(new light_classic::Painting(17));
  for (unsigned generation = 0; generation < 30; ++generation) {
    assert(before->visualFamily() == after->visualFamily());
    for (unsigned frame = 0; frame < 120; ++frame) {
      before->render(.083f, .4f); after->render(.083f, .4f);
      if (after->visualFamily() < 2)
        for (unsigned p = 0; p < 240 * 135; ++p) assert(before->pixels()[p] == after->pixels()[p]);
    }
    before->regenerate(); after->regenerate();
  }
}
