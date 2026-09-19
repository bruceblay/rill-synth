// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Six generative visual families, drawn flat.
//
// Everything here is an opaque shape with a hard edge: filled spans, discs,
// rings punched back to the ground, single-pixel lines, flat bands. There is
// no additive blending, no dithering and no soft falloff anywhere in the
// file, and a colour is either an ink or a fixed step down from it. An
// earlier version of these same six families was built the other way, out of
// dithered additive glow, and whatever the palette it read as neon: the glow
// is what made it read that way, not the colours. This is the drawing
// language Rill Drums uses, and the two instruments now share it.
//
// Shared by firmware and the host preview tools. No allocation, no locks;
// the only sizeable state is the frame itself.
namespace light {
class Painting {
 public:
  static constexpr unsigned width = 240, height = 135;
  static constexpr unsigned familyCount = 8;
  enum Family : unsigned { Contour = 0, Pendulum, Growth, Eclipse, Skyline, Truchet, Tiles, Parade };

 private:
  struct Color { float r, g, b; };
  // Weave: one pen, with the point it came from so a step can be drawn as a
  // segment rather than a dot.
  // Strata: a settled band, identified by the contour of its lower edge.
  // Growth: a branch tip. Dead tips stay in the array as terminals.
  struct Node { float x, y, dx, dy, weight; uint8_t depth; bool alive; };
  // Eclipse: a flat disc or a punched ring, drifting on its own two-rate path.
  struct Body { float phase, rateX, rateY, spanX, spanY, radius, thickness; unsigned tint; bool hollow; };

  std::array<uint16_t, width * height> frame{};
  std::array<float, 257> wave{};

  uint32_t rng = 1, evolutionRng = 1;
  unsigned kind = 0, palette = 0, count = 0;
  float phase = 0, breath = 0, pace = 1;
  std::array<float, 8> parameters{};
  std::array<float, 8> evolving{}, goals{};
  float evolutionAt = 0;
  Color ink[3]{};
  Color groundColor{232, 220, 192};
  uint16_t ground = 0;

  float figureT = 0, figureAt = 0;
  std::array<float, 4> figureFreq{}, figurePhase{}, figureSize{};
  std::array<Node, 168> nodes{};
  unsigned nodeCount = 0, liveNodes = 0;
  float growthHold = 0;
  std::array<Body, 9> bodies{};
  unsigned bodyCount = 0;
  std::array<uint8_t, 165> cells{};  // Truchet: 15 x 11
  float turnAt = 0;
  uint32_t scene = 1;                // fixed per generation; the hash-built families read it

  unsigned random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  float unit() { return float(random() >> 8) / 16777216.0f; }
  float range(float low, float high) { return low + unit() * (high - low); }
  // Deterministic and stateless, so a skyline or a parade can be as long as
  // it likes without storing itself.
  static uint32_t hash(uint32_t a, uint32_t b) {
    uint32_t h = a * 374761393u + b * 668265263u + 0x9e3779b9u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
  }
  static float hashUnit(uint32_t a, uint32_t b) { return float(hash(a, b) >> 8) * (1.0f / 16777216.0f); }
  float evolveUnit() {
    evolutionRng ^= evolutionRng << 13; evolutionRng ^= evolutionRng >> 17; evolutionRng ^= evolutionRng << 5;
    return float(evolutionRng >> 8) / 16777216.0f;
  }

  // Angles are carried in turns, not radians: the table is read by fraction,
  // which keeps every family's motion coefficients readable as cycles per
  // second or cycles per pixel.
  float fsin(float turns) const {
    float t = turns - std::floor(turns);
    float f = t * 256.0f;
    unsigned i = unsigned(f);
    if (i > 255) i = 255;
    return wave[i] + (wave[i + 1] - wave[i]) * (f - float(i));
  }
  float fcos(float turns) const { return fsin(turns + 0.25f); }

  static uint16_t color(Color c, float shade = 1) {
    unsigned r = unsigned(std::max(0.0f, std::min(255.0f, c.r * shade)));
    unsigned g = unsigned(std::max(0.0f, std::min(255.0f, c.g * shade)));
    unsigned b = unsigned(std::max(0.0f, std::min(255.0f, c.b * shade)));
    return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }
  // Four steps, not a ramp. A continuous shade is a gradient by another name.
  static float step(unsigned level) {
    static const float steps[4] = {1.0f, 0.74f, 0.52f, 0.34f};
    return steps[level & 3];
  }
  void span(int y, int left, int right, uint16_t c) {
    if (y < 0 || y >= int(height)) return;
    left = std::max(0, left); right = std::min(int(width) - 1, right);
    for (int x = left; x <= right; ++x) frame[unsigned(y) * width + unsigned(x)] = c;
  }
  void plot(int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= int(width) || y >= int(height)) return;
    frame[unsigned(y) * width + unsigned(x)] = c;
  }
  void disc(float cx, float cy, float r, uint16_t c) {
    if (r < 0.5f) return;
    int top = std::max(0, int(std::floor(cy - r))), bottom = std::min(int(height) - 1, int(std::ceil(cy + r)));
    for (int y = top; y <= bottom; ++y) {
      float v = (float(y) + 0.5f - cy) / r;
      if (std::abs(v) > 1) continue;
      float extent = r * std::sqrt(1 - v * v);
      span(y, int(std::ceil(cx - extent)), int(std::floor(cx + extent)), c);
    }
  }
  // The hole goes back to the ground colour rather than to a darker version
  // of the ink, so a ring is genuinely hollow.
  void ring(float cx, float cy, float r, float thickness, uint16_t c) {
    disc(cx, cy, r, c);
    if (r > thickness) disc(cx, cy, r - thickness, ground);
  }
  void line(float x0, float y0, float x1, float y1, uint16_t c) {
    int steps = int(std::max(std::abs(x1 - x0), std::abs(y1 - y0))) + 1;
    steps = std::min(steps, 320);
    float ix = (x1 - x0) / float(steps), iy = (y1 - y0) / float(steps);
    float x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) { plot(int(x), int(y), c); x += ix; y += iy; }
  }
  // Width is whole pixels, laid side by side across the direction of travel.
  void band(float x0, float y0, float x1, float y1, unsigned pixels, uint16_t c) {
    float dx = x1 - x0, dy = y1 - y0;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.0001f) { plot(int(x0), int(y0), c); return; }
    float ox = -dy / length, oy = dx / length;
    for (unsigned i = 0; i < pixels; ++i) {
      float offset = float(i) - float(pixels - 1) * 0.5f;
      line(x0 + ox * offset, y0 + oy * offset, x1 + ox * offset, y1 + oy * offset, c);
    }
  }
  void rect(float x0, float y0, float x1, float y1, uint16_t c) {
    int top = std::max(0, int(y0)), bottom = std::min(int(height) - 1, int(y1));
    for (int y = top; y <= bottom; ++y) span(y, int(x0), int(x1), c);
  }
  void triangle(float ax, float ay, float bx, float by, float cx, float cy, uint16_t c) {
    int top = std::max(0, int(std::floor(std::min(ay, std::min(by, cy)))));
    int bottom = std::min(int(height) - 1, int(std::ceil(std::max(ay, std::max(by, cy)))));
    for (int y = top; y <= bottom; ++y) {
      float scan = float(y) + 0.5f, crossings[3];
      unsigned n = 0;
      const float xs[3] = {ax, bx, cx}, ys[3] = {ay, by, cy};
      for (unsigned i = 0; i < 3; ++i) {
        unsigned j = (i + 1) % 3;
        if ((ys[i] <= scan && ys[j] > scan) || (ys[j] <= scan && ys[i] > scan))
          crossings[n++] = xs[i] + (scan - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
      if (n >= 2) span(y, int(std::ceil(std::min(crossings[0], crossings[1]))),
                          int(std::floor(std::max(crossings[0], crossings[1]))), c);
    }
  }
  // An ink let down toward the ground. Distance is carried this way rather
  // than by darkening: on a daylight palette the far things are paler, not
  // blacker, which is what haze actually does.
  uint16_t wash(const Color& c, float mix) const {
    return color({c.r + (groundColor.r - c.r) * mix,
                  c.g + (groundColor.g - c.g) * mix,
                  c.b + (groundColor.b - c.b) * mix});
  }
  void clear() { frame.fill(ground); }

  void evolve(float dt) {
    evolutionAt -= dt;
    if (evolutionAt <= 0) {
      evolutionAt = 5 + evolveUnit() * 8;
      for (auto& g : goals) g = evolveUnit();
    }
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] += (goals[i] - evolving[i]) * dt * 0.4f;
  }

  // Shared slow field. Weave reads it for drift, Growth for the way a branch
  // leans, so the two families feel like the same weather.
  float flowAngle(float x, float y, float t) const {
    return fsin(x * 0.0041f + t * 0.031f) * 0.30f
         + fsin(y * 0.0063f - t * 0.024f) * 0.26f
         + fsin((x + y * 0.7f) * 0.0027f + t * 0.017f) * 0.34f;
  }

  void renderContour() {
    // The field of an earlier version, quantised into flat areas instead of
    // lit as glowing lines: a printed contour map rather than a light show.
    float t = phase;
    float ax = 120 + fcos(t * 0.015f) * (34 + evolving[0] * 62);
    float ay = 67 + fsin(t * 0.012f) * (18 + evolving[1] * 34);
    float bx = 120 - fcos(t * 0.009f + 0.31f) * (28 + evolving[2] * 70);
    float by = 67 - fsin(t * 0.017f + 0.17f) * (16 + evolving[3] * 38);
    float ka = 0.0055f + evolving[4] * 0.0075f;
    float kb = 0.0042f + evolving[5] * 0.0085f;
    float tilt = evolving[6];
    float px = fcos(tilt) * 0.0065f, py = fsin(tilt) * 0.0065f;
    unsigned levels = 5 + unsigned(parameters[0] * 4.0f);
    float rise = breath * 0.06f;
    // Levels alternate between an ink and a tint of that ink toward the
    // ground. Stepping toward black instead, the way a dark-ground family
    // would, turns every one of these palettes to mud.
    uint16_t swatch[12];
    for (unsigned i = 0; i < levels && i < 12; ++i) {
      static const float tints[3] = {0.0f, 0.55f, 0.28f};
      float mix = tints[(i / 3) % 3];
      const Color& base = ink[i % 3];
      swatch[i] = color({base.r + (groundColor.r - base.r) * mix,
                         base.g + (groundColor.g - base.g) * mix,
                         base.b + (groundColor.b - base.b) * mix});
    }
    for (unsigned y = 0; y < height; ++y) {
      float fy = float(y) + 0.5f;
      float day = fy - ay, dby = fy - by;
      unsigned last = levels;
      int runStart = 0;
      for (unsigned x = 0; x < width; ++x) {
        float fx = float(x) + 0.5f;
        float dax = fx - ax, dbx = fx - bx;
        float da = std::sqrt(dax * dax + day * day), db = std::sqrt(dbx * dbx + dby * dby);
        float v = fsin(da * ka - t * 0.062f) * 0.4f
                + fsin(db * kb + t * 0.048f) * 0.36f
                + fsin(fx * px + fy * py + t * 0.028f) * 0.24f + rise;
        float u = std::max(0.0f, std::min(0.999f, v * 0.5f + 0.5f));
        unsigned level = unsigned(u * float(levels));
        if (level != last) {
          if (last < levels) span(int(y), runStart, int(x) - 1, swatch[last]);
          last = level; runStart = int(x);
        }
      }
      if (last < levels) span(int(y), runStart, int(width) - 1, swatch[last]);
    }
  }

  void newFigure() {
    // Near-integer frequency ratios draw closed figures; the small detune is
    // what makes the curve precess instead of retracing one line.
    float base = range(1.4f, 3.1f);
    static const float ratios[6] = {1.0f, 2.0f, 3.0f, 1.5f, 2.5f, 4.0f};
    for (unsigned i = 0; i < 4; ++i) {
      figureFreq[i] = base * ratios[unsigned(unit() * 6.0f) % 6] * (1 + (unit() - 0.5f) * 0.012f);
      figurePhase[i] = unit();
      figureSize[i] = range(0.25f, 1.0f);
    }
    float sum01 = figureSize[0] + figureSize[1], sum23 = figureSize[2] + figureSize[3];
    figureSize[0] /= sum01; figureSize[1] /= sum01; figureSize[2] /= sum23; figureSize[3] /= sum23;
    figureT = 0;
    figureAt = range(20.0f, 30.0f);
  }
  void renderPendulum(float dt) {
    // The pen is slow on purpose. Run it at the speed the figure is traversed
    // and the whole loop closes several times a second, which fills the page
    // in a moment and then has nothing left to show.
    figureAt -= dt;
    if (figureAt <= 0) { clear(); newFigure(); }
    float span_ = dt * (0.085f + parameters[1] * 0.06f) * (1.0f + breath * 0.35f);
    unsigned steps = 12 + unsigned(evolving[0] * 16);
    float stepT = span_ / float(steps);
    float rx = 76 + evolving[1] * 36, ry = 44 + evolving[2] * 18;
    float lx = 0, ly = 0;
    for (unsigned i = 0; i <= steps; ++i) {
      float t = figureT + stepT * float(i);
      float x = 120 + rx * (figureSize[0] * fsin(figureFreq[0] * t + figurePhase[0])
                          + figureSize[1] * fsin(figureFreq[1] * t + figurePhase[1]));
      float y = 67 + ry * (figureSize[2] * fsin(figureFreq[2] * t + figurePhase[2])
                         + figureSize[3] * fsin(figureFreq[3] * t + figurePhase[3]));
      if (i == 0) { lx = x; ly = y; continue; }
      // The ink changes by whole revolutions of the slowest pendulum, so the
      // drawing is built from a few long passes in flat colour.
      unsigned tint = unsigned(t * figureFreq[0]) % 3;
      line(lx, ly, x, y, color(ink[tint], step(unsigned(t * figureFreq[0] * 0.5f) % 2)));
      lx = x; ly = y;
    }
    figureT += span_;
  }

  void seedGrowth() {
    // Roots enter from one of the short sides and cross the long axis, the
    // only direction with room enough to develop before running out of frame.
    nodeCount = 0;
    bool fromLeft = unit() < 0.5f;
    unsigned roots = 2 + unsigned(unit() * 2.5f);
    for (unsigned i = 0; i < roots; ++i) {
      Node& n = nodes[nodeCount++];
      n.x = fromLeft ? range(4.0f, 13.0f) : range(227.0f, 236.0f);
      n.y = range(18.0f, 117.0f);
      float spread = (unit() - 0.5f) * 0.09f;
      n.dx = (fromLeft ? 1.0f : -1.0f) * fcos(spread);
      n.dy = fsin(spread);
      n.weight = range(2.2f, 3.2f);
      n.depth = uint8_t(i % 3);
      n.alive = true;
    }
    liveNodes = nodeCount;
    growthHold = 0;
  }
  void growStep(float reachScale) {
    unsigned live = 0;
    unsigned existing = nodeCount;
    for (unsigned i = 0; i < existing; ++i) {
      Node& n = nodes[i];
      if (!n.alive) continue;
      float reach = (1.5f + n.weight * 0.45f) * reachScale;
      // The lean is per step and accumulates, so it stays small: ten times
      // this and the branch spirals instead of reaching.
      float lean = flowAngle(n.x, n.y, phase) * 0.011f + (unit() - 0.5f) * 0.006f;
      float c = fcos(lean), s = fsin(lean);
      float dx = n.dx * c - n.dy * s, dy = n.dx * s + n.dy * c;
      float length = std::sqrt(dx * dx + dy * dy);
      n.dx = dx / length; n.dy = dy / length;
      float nx = n.x + n.dx * reach, ny = n.y + n.dy * reach;
      band(n.x, n.y, nx, ny, n.weight > 2.0f ? 3u : (n.weight > 1.1f ? 2u : 1u),
           color(ink[n.depth % 3], step(n.weight > 1.1f ? 0 : 1)));
      n.x = nx; n.y = ny;
      n.weight *= 0.9955f;
      bool escaped = nx < 2 || nx > float(width) - 2 || ny < 2 || ny > float(height) - 2;
      if (escaped || n.weight < 0.30f || n.depth > 30) { n.alive = false; continue; }
      // A tip that runs into ground already taken stops there. Growth then
      // competes for the empty parts of the frame instead of piling up, and
      // the structure ends up with its own negative space.
      int ax = int(nx + n.dx * reach * 2), ay = int(ny + n.dy * reach * 2);
      if (ax >= 0 && ax < int(width) && ay >= 0 && ay < int(height)
          && frame[unsigned(ay) * width + unsigned(ax)] != ground) { n.alive = false; continue; }
      // A split costs both children some weight, so the structure thins as it
      // spreads and finishes on its own rather than filling the screen.
      float branchChance = (0.055f + evolving[1] * 0.080f) * std::min(1.0f, n.weight * 0.7f);
      if (unit() < branchChance && nodeCount < nodes.size()) {
        Node& child = nodes[nodeCount++];
        float turn = range(0.045f, 0.125f) * (unit() < 0.5f ? -1.0f : 1.0f);
        float tc = fcos(turn), ts = fsin(turn);
        child = n;
        child.dx = n.dx * tc - n.dy * ts;
        child.dy = n.dx * ts + n.dy * tc;
        child.weight = n.weight * range(0.62f, 0.80f);
        child.depth = uint8_t(n.depth + 1);
        child.alive = true;
        n.weight *= 0.86f;
        float back = -turn * 0.5f;
        float bc = fcos(back), bs = fsin(back);
        float ndx = n.dx * bc - n.dy * bs, ndy = n.dx * bs + n.dy * bc;
        n.dx = ndx; n.dy = ndy;
        n.depth = uint8_t(n.depth + 1);
        ++live;
      }
      ++live;
    }
    liveNodes = live;
  }
  void renderGrowth(float dt) {
    if (liveNodes > 0) {
      // No mark at the moving tip: on a frame that is never cleared, a disc
      // drawn at each tip every frame lays down a dotted trail beside every
      // branch. The ends are capped once the branch stops instead.
      growStep(0.72f + breath * 0.26f);
      for (unsigned i = 0; i < nodeCount; ++i)
        if (!nodes[i].alive) disc(nodes[i].x, nodes[i].y, 1.4f, color(ink[(nodes[i].depth + 1) % 3]));
    } else {
      // Finished. The structure is held, its terminals marked, and then the
      // page is changed.
      growthHold += dt;
      for (unsigned i = 0; i < nodeCount; i += 2)
        disc(nodes[i].x, nodes[i].y, 1.4f + breath * 1.6f, color(ink[(nodes[i].depth + 2) % 3]));
      if (growthHold > (nodeCount < 10 ? 1.5f : 4.0f)) { clear(); seedGrowth(); }
    }
  }

  void placeBodies() {
    bodyCount = 6 + unsigned(unit() * 3.5f);
    for (unsigned i = 0; i < bodyCount; ++i) {
      Body& b = bodies[i];
      b.phase = unit();
      b.rateX = range(0.010f, 0.032f) * (unit() < 0.5f ? -1.0f : 1.0f);
      b.rateY = range(0.008f, 0.028f) * (unit() < 0.5f ? -1.0f : 1.0f);
      b.spanX = range(52.0f, 104.0f);
      b.spanY = range(22.0f, 52.0f);
      b.radius = range(9.0f, 40.0f);
      b.thickness = range(3.0f, 9.0f);
      b.tint = i % 3;
      b.hollow = unit() < 0.45f;
    }
  }
  void renderEclipse() {
    // Flat discs and punched rings, drifting slowly past each other. The
    // composition is whatever their overlaps happen to make; the later a body
    // is drawn, the more of it stays visible.
    clear();
    for (unsigned i = 0; i < bodyCount; ++i) {
      const Body& b = bodies[i];
      float x = 120 + fsin(phase * b.rateX + b.phase) * b.spanX;
      float y = 67 + fsin(phase * b.rateY + b.phase * 1.7f) * b.spanY;
      float r = b.radius * (0.92f + evolving[i % 8] * 0.2f) + breath * 5.0f;
      uint16_t c = color(ink[b.tint], step(i % 3 == 2 ? 1 : 0));
      if (b.hollow) ring(x, y, r, b.thickness, c); else disc(x, y, r, c);
    }
  }

  void renderSkyline() {
    // Three ranks of flat buildings at their own speeds, the far ones washed
    // toward the sky. Every building is hashed from its own index, so the
    // city is endless and never stored.
    clear();
    float sunX = 40 + evolving[0] * 160, sunY = 24 + evolving[1] * 22;
    disc(sunX, sunY, 13 + breath * 3, wash(ink[2], 0.25f));
    static const float speeds[3] = {1.6f, 4.2f, 9.0f};
    static const float washes[3] = {0.62f, 0.34f, 0.0f};
    static const float bases[3] = {96, 112, 132};
    static const float lots[3] = {21, 26, 34};
    for (unsigned rank = 0; rank < 3; ++rank) {
      float scroll = phase * speeds[rank] + float(rank) * 137.0f;
      float lot = lots[rank];
      uint16_t stone = wash(ink[rank % 3], washes[rank]);
      uint16_t lit = wash(ink[(rank + 2) % 3], washes[rank] * 0.5f);
      int first = int(std::floor(scroll / lot)) - 1;
      for (int slot = first; float(slot) * lot - scroll < float(width) + lot; ++slot) {
        uint32_t id = uint32_t(slot * 7 + int(rank) * 991 + int(scene & 0xffffu));
        float x = float(slot) * lot - scroll;
        float w = lot * (0.62f + hashUnit(id, 1) * 0.3f);
        float top = bases[rank] - (14 + hashUnit(id, 2) * (28 + float(rank) * 22));
        rect(x, top, x + w, bases[rank], stone);
        if (hashUnit(id, 3) < 0.35f) {
          // A water tank or a sign box on the roof, the thing that makes a
          // flat block read as a building.
          float bw = w * 0.34f, bx = x + w * (0.1f + hashUnit(id, 4) * 0.5f);
          rect(bx, top - 5 - hashUnit(id, 5) * 4, bx + bw, top, stone);
        }
        if (rank == 0) continue;
        for (float wy = top + 4; wy < bases[rank] - 3; wy += 6)
          for (float wx = x + 3; wx < x + w - 2; wx += 5)
            if (hashUnit(id, uint32_t(wy) * 31u + uint32_t(wx)) < 0.42f + breath * 0.2f)
              rect(wx, wy, wx + 1, wy + 2, lit);
      }
    }
  }

  void placeTiling() {
    for (unsigned i = 0; i < cells.size(); ++i) cells[i] = uint8_t(random() & 3u);
    turnAt = 0;
  }
  void renderTruchet(float dt) {
    // Quarter arcs on a grid. Every cell joins its neighbours whichever way it
    // is turned, so the pattern is always a continuous set of paths; turning a
    // few cells at a time keeps re-routing them.
    turnAt -= dt;
    if (turnAt <= 0) {
      turnAt = 0.35f + evolveUnit() * 0.5f;
      unsigned turns = 1 + unsigned(evolveUnit() * 4.0f);
      for (unsigned i = 0; i < turns; ++i) {
        unsigned at = random() % cells.size();
        cells[at] = uint8_t((cells[at] + 1 + (random() & 1u)) & 3u);
      }
    }
    clear();
    constexpr unsigned columns = 15, rows = 11;
    const float size = float(width) / float(columns);
    unsigned thickness = 2 + unsigned(parameters[2] * 2.0f);
    for (unsigned row = 0; row < rows; ++row)
      for (unsigned col = 0; col < columns; ++col) {
        unsigned index = row * columns + col;
        float x = float(col) * size, y = float(row) * size - 3;
        uint16_t c = color(ink[(col + row + cells[index]) % 3]);
        // Two arcs per cell, cornered on opposite sides.
        for (unsigned half = 0; half < 2; ++half) {
          unsigned corner = (cells[index] + half * 2u) & 3u;
          float cx = (corner == 1 || corner == 2) ? x + size : x;
          float cy = (corner >= 2) ? y + size : y;
          // The arcs draw in a little as the music gets louder, which pulls
          // the paths apart and lets the ground through between them.
          float r = size * (0.5f - breath * 0.05f);
          float px = 0, py = 0;
          for (unsigned seg = 0; seg <= 8; ++seg) {
            float a = float(corner) * 0.25f + float(seg) * (0.25f / 8.0f);
            float ax = cx + fcos(a) * r, ay = cy + fsin(a) * r;
            if (seg) band(px, py, ax, ay, thickness, c);
            px = ax; py = ay;
          }
        }
      }
  }

  void renderTiles() {
    // A grid of flat squares breathing in a travelling wave. Nothing here is
    // drawn on top of anything else, which is what keeps it calm.
    clear();
    unsigned columns = 8 + unsigned(parameters[3] * 5.0f), rows = 5 + unsigned(parameters[4] * 3.0f);
    float cw = float(width) / float(columns), ch = float(height) / float(rows);
    float angle = evolving[2] * 1.0f;
    float dx = fcos(angle) * 0.9f, dy = fsin(angle) * 0.9f;
    for (unsigned row = 0; row < rows; ++row)
      for (unsigned col = 0; col < columns; ++col) {
        float u = (float(col) + 0.5f) / float(columns), v = (float(row) + 0.5f) / float(rows);
        float travel = u * dx + v * dy;
        float w1 = fsin(travel * (1.0f + evolving[3] * 2.5f) + phase * 0.11f);
        float w2 = fsin(travel * 3.0f - phase * 0.07f + evolving[4]);
        float size = 0.22f + 0.62f * (0.5f + 0.5f * w1) + breath * 0.14f;
        float half = std::min(cw, ch) * 0.5f * std::min(1.4f, size);
        float cx = float(col) * cw + cw * 0.5f, cy = float(row) * ch + ch * 0.5f;
        unsigned tint = unsigned((w2 * 0.5f + 0.5f) * 3.0f) % 3;
        static const float tints[3] = {0.0f, 0.34f, 0.0f};
        rect(cx - half, cy - half, cx + half, cy + half,
             wash(ink[tint], tints[(col + row) % 3]));
      }
  }

  void renderParade() {
    // Things going past, on a ground line: a disc, a box, a box with a roof,
    // a tree. Each is hashed from its place in the queue, so the procession
    // never repeats and nothing is kept.
    clear();
    float horizon = 96 + evolving[0] * 14;
    rect(0, horizon, float(width) - 1, float(height) - 1, wash(ink[1], 0.55f));
    for (unsigned rank = 0; rank < 2; ++rank) {
      float speed = rank ? 10.0f : 4.0f;
      float lot = rank ? 46.0f : 62.0f;
      float scroll = phase * speed + float(rank) * 311.0f;
      float baseline = horizon + (rank ? 22.0f : 4.0f);
      float scale = rank ? 1.0f : 0.72f;
      int first = int(std::floor(scroll / lot)) - 1;
      for (int slot = first; float(slot) * lot - scroll < float(width) + lot; ++slot) {
        uint32_t id = uint32_t(slot * 13 + int(rank) * 617 + int(scene & 0xffffu));
        float x = float(slot) * lot - scroll + lot * 0.5f;
        float size = (12 + hashUnit(id, 1) * 16) * scale * (1.0f + breath * 0.12f);
        uint16_t c = wash(ink[hash(id, 2) % 3], rank ? 0.0f : 0.3f);
        switch (hash(id, 3) % 4) {
          case 0: disc(x, baseline - size, size, c); break;
          case 1: rect(x - size, baseline - size * 1.5f, x + size, baseline, c); break;
          case 2:
            rect(x - size * 0.8f, baseline - size, x + size * 0.8f, baseline, c);
            triangle(x - size, baseline - size, x + size, baseline - size, x, baseline - size * 2.1f, c);
            break;
          default:
            rect(x - size * 0.16f, baseline - size, x + size * 0.16f, baseline, wash(ink[2], 0.2f));
            disc(x, baseline - size * 1.3f, size * 0.75f, c);
            break;
        }
      }
    }
  }

 public:
  explicit Painting(uint32_t value = 17) : rng(value ? value : 1) {
    for (unsigned i = 0; i < wave.size(); ++i) wave[i] = std::sin(float(i) * (6.283185307f / 256.0f));
    regenerate();
  }
  void seed(uint32_t value) { rng = value ? value : 1; count = 0; regenerate(); }

  void regenerate() {
    kind = count ? (kind + 1 + random() % (familyCount - 1)) % familyCount : random() % familyCount;
    palette = count ? (palette + 1 + random() % 5) % 6 : random() % 6;
    ++count;
    phase = unit() * 40.0f;
    breath = 0;
    for (auto& p : parameters) p = unit();
    pace = 0.75f + parameters[7] * 0.6f;
    // Daylight palettes: a coloured ground and three flat inks that sit on
    // it, in the register of a faded photograph rather than of emitted light.
    // The ground is the important part. Saturated ink on black is what made
    // every earlier attempt read as neon however the hues were chosen, and
    // against a warm ground the same drawing reads as printed instead. It
    // also suits the panel, which loses shadow detail but has no trouble
    // holding a light field.
    //
    // First entry is the ground, then the three inks.
    static const Color palettes[6][4] = {
      // Shōwa afternoon: faded cream, dusty red, sun-bleached teal, mustard.
      {{232, 220, 192}, {196,  85,  63}, { 78, 138, 134}, {211, 160,  60}},
      // Rainy Ginza: wet pavement grey, slate, brick, bone.
      {{201, 210, 206}, { 62,  90,  99}, {179,  91,  74}, {240, 230, 210}},
      // Park in spring: soft sky, cherry red, leaf, cream.
      {{143, 203, 232}, {226,  88,  75}, { 95, 163,  82}, {251, 240, 216}},
      // Village morning: bone, sage, terracotta, slate blue.
      {{234, 227, 210}, {126, 154, 107}, {192, 107,  78}, {110, 132, 148}},
      // Late sun: warm peach, persimmon, deep olive, plum brown.
      {{240, 201, 160}, {212,  91,  60}, {107, 122,  58}, {122,  74,  70}},
      // Night market: deep indigo, lantern amber, faded rose, pale jade.
      {{ 44,  58,  74}, {232, 163,  61}, {201, 106, 106}, {143, 185, 168}},
    };
    groundColor = palettes[palette][0];
    for (unsigned i = 0; i < 3; ++i) ink[i] = palettes[palette][i + 1];
    ground = color(groundColor);
    evolutionRng = (rng ^ 0x85ebca6bu) | 1u;
    evolutionAt = 0;
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] = goals[i] = evolveUnit();
    newFigure();
    growthHold = 0;
    seedGrowth();
    placeBodies();
    placeTiling();
    scene = (rng ^ 0xc2b2ae35u) | 1u;
    clear();
  }

  unsigned generation() const { return count; }
  unsigned visualFamily() const { return kind; }
  const uint16_t* pixels() const { return frame.data(); }

  void render(float seconds, float audio) {
    seconds = std::max(0.0f, std::min(0.1f, seconds));
    const float dt = seconds;
    phase += seconds * pace;
    if (phase > 100000.0f) phase -= 100000.0f;
    breath += (std::max(0.0f, std::min(1.0f, audio)) - breath) * std::min(1.0f, seconds * 5);
    evolve(seconds);
    switch (kind) {
      case Contour: renderContour(); break;
      case Pendulum: renderPendulum(seconds); break;
      case Growth: renderGrowth(seconds); break;
      case Eclipse: renderEclipse(); break;
      case Skyline: renderSkyline(); break;
      case Truchet: renderTruchet(dt); break;
      case Tiles: renderTiles(); break;
      default: renderParade(); break;
    }
  }
};
}
