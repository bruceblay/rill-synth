// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Garden.h"
#include "../src/Light.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>
// Preview a family as the device draws it: played by the engine, note by
// note. Rendering it against a synthetic level would show something the
// device never does.
int main(int argc, char** argv) {
  if (argc < 2) return 1;
  uint32_t seed = argc > 2 ? uint32_t(std::strtoul(argv[2], nullptr, 10)) : 17u;
  auto painting = std::unique_ptr<light::Painting>(new light::Painting(seed));
  if (argc > 3) {
    unsigned wanted = unsigned(std::strtoul(argv[3], nullptr, 10)) % light::Painting::familyCount;
    while (painting->visualFamily() != wanted) painting->regenerate();
  }
  unsigned frames = argc > 4 ? unsigned(std::strtoul(argv[4], nullptr, 10)) : 120;
  auto engine = std::unique_ptr<garden::Engine>(new garden::Engine(seed));
  std::array<int16_t, 512> block{};
  for (unsigned f = 0; f < frames; ++f) {
    uint8_t note = 0;
    float weight = 0.6f, energy = 0;
    // One display frame is a twelfth of a second of audio.
    for (unsigned b = 0; b < garden::rate / 12 / 512; ++b) {
      engine->render(block.data(), block.size());
      for (auto s : block) energy += float(std::abs(int(s)));
      if (uint8_t struck = engine->drainOnset()) { note = struck; weight = engine->onsetWeight(); }
    }
    painting->render(1.0f / 12, energy / (512 * 5 * 8000.0f), note, weight);
  }
  std::ofstream out(argv[1], std::ios::binary);
  out << "P6\n240 135\n255\n";
  for (unsigned i = 0; i < 240 * 135; ++i) {
    uint16_t c = painting->pixels()[i];
    out.put(char(((c >> 11) & 31) * 255 / 31));
    out.put(char(((c >> 5) & 63) * 255 / 63));
    out.put(char((c & 31) * 255 / 31));
  }
  return out ? 0 : 2;
}
