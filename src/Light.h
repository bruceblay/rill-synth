// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Six generative visual families, drawn as light on a dark ground.
//
// The common material is a subpixel additive splat: every mark is laid down
// with bilinear weights and dithered against moving hash noise, so a thin
// stroke reads as a drawn line at 240x135 and repeated passes accumulate into
// glow rather than into five-bit banding. Field renders dither against an
// ordered matrix instead, which is steadier for a whole-screen gradient.
// Three families (Current, Harmonograph, Delta) keep the previous frame and
// let it decay, which makes the screen a long exposure of its own history;
// the other three (Interference, Strata, Veil) resolve a whole field each
// frame. All six drift through slow parameter goals, so a family revisited
// minutes later is not the arrangement it was, and all six answer the audio
// level through `breath` rather than jumping on transients.
//
// Shared by firmware and the host preview tools. No allocation, no locks;
// the only sizeable state is the frame itself.
namespace light {
class Painting {
 public:
  static constexpr unsigned width = 240, height = 135;
  static constexpr unsigned familyCount = 6;
  enum Family : unsigned { Current = 0, Interference, Strata, Harmonograph, Delta, Veil };

 private:
  struct Color { float r, g, b; };
  // Current: one drifting mark, remembering where it was so it can be drawn
  // as a segment rather than a dot.
  struct Drop { float x, y, px, py, life, span, speed; unsigned tint; };
  // Strata: a settled band, identified by the contour of its lower edge.
  struct Layer { float base, target, amp1, freq1, ph1, amp2, freq2, ph2, tone, glow; };
  // Delta: a growing tip. Dead tips stay in the array as terminals so the
  // finished structure can keep breathing.
  struct Node { float x, y, dx, dy, weight; uint8_t depth; bool alive; };

  std::array<uint16_t, width * height> frame{};
  std::array<uint16_t, height> rowGround{};
  std::array<float, 257> wave{};

  uint32_t rng = 1, evolutionRng = 1;
  unsigned kind = 0, palette = 0, count = 0;
  float phase = 0, breath = 0, pace = 1;
  unsigned tick = 0;
  std::array<float, 8> parameters{};
  std::array<float, 8> evolving{}, goals{};
  float evolutionAt = 0;
  Color ink[3]{}, glow{};

  std::array<Drop, 88> drops{};
  std::array<Layer, 14> layers{};
  unsigned layerCount = 0;
  float settleAt = 0;
  float penT = 0, penDecay = 0, penEnvelope = 1, figureAt = 0;
  std::array<float, 4> penFreq{}, penPhase{}, penSize{};
  std::array<Node, 168> nodes{};
  unsigned nodeCount = 0, liveNodes = 0;
  float growthHold = 0;

  unsigned random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  float unit() { return float(random() >> 8) / 16777216.0f; }
  float range(float low, float high) { return low + unit() * (high - low); }
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
    float frac = f - float(i);
    return wave[i] + (wave[i + 1] - wave[i]) * frac;
  }
  float fcos(float turns) const { return fsin(turns + 0.25f); }
  static float dither(int x, int y) {
    static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return float(bayer[(y & 3) * 4 + (x & 3)]) * (1.0f / 16.0f);
  }
  static float grain(int x, int y) {
    unsigned h = unsigned(x) * 374761393u + unsigned(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h >> 24) & 255) * (1.0f / 255.0f);
  }
  // The same hash, moved every frame. A still pattern would be rounded up in
  // the same cells forever, which freezes the noise into the picture and
  // leaves a permanent haze wherever the decay should have reached ground.
  float shimmer(int x, int y) const { return grain(x + int(tick * 37u), y + int(tick * 17u)); }

  void add(int x, int y, const Color& c, float a) {
    if (a <= 0.0f || x < 0 || y < 0 || x >= int(width) || y >= int(height)) return;
    a = std::min(a, 6.0f);
    uint16_t& p = frame[unsigned(y) * width + unsigned(x)];
    // Moving hash noise rather than the ordered matrix: accumulated strokes
    // would otherwise show the matrix as a visible checkerboard.
    float d = shimmer(x, y);
    int r = int((p >> 11) & 31) + int(c.r * a * (31.0f / 255.0f) + d);
    int g = int((p >> 5) & 63) + int(c.g * a * (63.0f / 255.0f) + d);
    int b = int(p & 31) + int(c.b * a * (31.0f / 255.0f) + d);
    p = uint16_t((std::min(r, 31) << 11) | (std::min(g, 63) << 5) | std::min(b, 31));
  }
  // A mark placed between pixels: the weights are what make a one-pixel line
  // read as a drawn stroke instead of a staircase.
  void splat(float x, float y, const Color& c, float a) {
    float fx = std::floor(x), fy = std::floor(y);
    int xi = int(fx), yi = int(fy);
    float u = x - fx, v = y - fy;
    add(xi, yi, c, a * (1 - u) * (1 - v));
    add(xi + 1, yi, c, a * u * (1 - v));
    add(xi, yi + 1, c, a * (1 - u) * v);
    add(xi + 1, yi + 1, c, a * u * v);
  }
  void stroke(float x0, float y0, float x1, float y1, const Color& c, float a) {
    float dx = x1 - x0, dy = y1 - y0;
    int steps = int(std::max(std::abs(dx), std::abs(dy))) + 1;
    steps = std::min(steps, 96);
    float ix = dx / float(steps), iy = dy / float(steps);
    float x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) { splat(x, y, c, a); x += ix; y += iy; }
  }
  void bloom(float cx, float cy, float r, const Color& c, float a) {
    int top = std::max(0, int(cy - r)), bottom = std::min(int(height) - 1, int(cy + r));
    int left = std::max(0, int(cx - r)), right = std::min(int(width) - 1, int(cx + r));
    float inv = 1.0f / (r * r);
    for (int y = top; y <= bottom; ++y)
      for (int x = left; x <= right; ++x) {
        float dx = float(x) + 0.5f - cx, dy = float(y) + 0.5f - cy;
        float falloff = 1 - (dx * dx + dy * dy) * inv;
        if (falloff > 0) add(x, y, c, a * falloff * falloff);
      }
  }
  // Decay toward the ground colour. The dither term is what stops a pixel
  // one step above the ground from sitting there forever.
  void settle(unsigned keep) {
    for (unsigned y = 0; y < height; ++y) {
      uint16_t ground = rowGround[y];
      int gr = int((ground >> 11) & 31), gg = int((ground >> 5) & 63), gb = int(ground & 31);
      for (unsigned x = 0; x < width; ++x) {
        uint16_t& p = frame[y * width + x];
        if (p == ground) continue;
        int d = int(shimmer(int(x), int(y)) * 255.0f);
        int r = gr + int((((p >> 11) & 31) - unsigned(gr)) * keep + unsigned(d)) / 256;
        int g = gg + int((((p >> 5) & 63) - unsigned(gg)) * keep + unsigned(d)) / 256;
        int b = gb + int(((p & 31) - unsigned(gb)) * keep + unsigned(d)) / 256;
        p = uint16_t((std::min(std::max(r, 0), 31) << 11) | (std::min(std::max(g, 0), 63) << 5) | std::min(std::max(b, 0), 31));
      }
    }
  }
  void clear() {
    for (unsigned y = 0; y < height; ++y) {
      uint16_t c = rowGround[y];
      for (unsigned x = 0; x < width; ++x) frame[y * width + x] = c;
    }
  }

  void evolve(float dt) {
    evolutionAt -= dt;
    if (evolutionAt <= 0) {
      evolutionAt = 5 + evolveUnit() * 8;
      for (auto& g : goals) g = evolveUnit();
    }
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] += (goals[i] - evolving[i]) * dt * 0.4f;
  }

  // Shared slow field. Current reads it for drift, Delta for the way a
  // branch leans, so the two families feel like the same weather.
  float flowAngle(float x, float y, float t) const {
    return fsin(x * 0.0041f + t * 0.031f) * 0.30f
         + fsin(y * 0.0063f - t * 0.024f) * 0.26f
         + fsin((x + y * 0.7f) * 0.0027f + t * 0.017f) * 0.34f;
  }

  void respawn(Drop& d, unsigned index) {
    // Sources wander, so a family left running keeps finding new paths
    // through the field instead of re-tracing the first ones.
    float sx = 30 + evolving[0] * 180, sy = 20 + evolving[1] * 95;
    float spread = 44 + evolving[2] * 90;
    d.x = sx + (unit() - 0.5f) * spread * 2;
    d.y = sy + (unit() - 0.5f) * spread;
    d.x = std::min(std::max(d.x, -6.0f), float(width) + 6);
    d.y = std::min(std::max(d.y, -6.0f), float(height) + 6);
    d.px = d.x; d.py = d.y;
    d.life = 1;
    d.span = range(1.6f, 4.8f);
    d.speed = range(0.55f, 1.45f);
    d.tint = index % 3;
  }
  void renderCurrent(float dt) {
    settle(241);
    float speed = (26 + evolving[3] * 46) * (0.72f + breath * 0.7f);
    unsigned active = 60 + unsigned(evolving[4] * float(drops.size() - 60));
    for (unsigned i = 0; i < active; ++i) {
      Drop& d = drops[i];
      d.life -= dt / d.span;
      bool gone = d.life <= 0 || d.x < -8 || d.x > float(width) + 8 || d.y < -8 || d.y > float(height) + 8;
      if (gone) { respawn(d, i); continue; }
      float angle = flowAngle(d.x, d.y, phase);
      d.px = d.x; d.py = d.y;
      d.x += fcos(angle) * speed * d.speed * dt;
      d.y += fsin(angle) * speed * d.speed * dt * 0.66f;
      // Fade in and out at the ends of a life so no stroke starts or stops
      // abruptly; the field should look like it has no edges in time.
      float ends = std::min(1.0f, std::min((1 - d.life) * 7.0f, d.life * 3.2f));
      float a = ends * (0.62f + breath * 0.70f);
      stroke(d.px, d.py, d.x, d.y, ink[d.tint], a);
      if (d.life > 0.82f) splat(d.x, d.y, glow, a * 0.7f);
    }
  }

  void renderInterference() {
    float t = phase;
    float ax = 120 + fcos(t * 0.015f) * (34 + evolving[0] * 62);
    float ay = 67 + fsin(t * 0.012f) * (18 + evolving[1] * 34);
    float bx = 120 - fcos(t * 0.009f + 0.31f) * (28 + evolving[2] * 70);
    float by = 67 - fsin(t * 0.017f + 0.17f) * (16 + evolving[3] * 38);
    float ka = 0.0055f + evolving[4] * 0.0075f;
    float kb = 0.0042f + evolving[5] * 0.0085f;
    float tilt = evolving[6];
    float px = fcos(tilt) * 0.0065f, py = fsin(tilt) * 0.0065f;
    float bands = 6.5f + parameters[0] * 7.0f;
    float sharp = 2.1f + breath * 2.2f;
    float lift = 0.6f + breath * 0.7f;
    for (unsigned y = 0; y < height; ++y) {
      float fy = float(y) + 0.5f;
      float day = fy - ay, dby = fy - by;
      uint16_t ground = rowGround[y];
      int gr = int((ground >> 11) & 31), gg = int((ground >> 5) & 63), gb = int(ground & 31);
      for (unsigned x = 0; x < width; ++x) {
        float fx = float(x) + 0.5f;
        float dax = fx - ax, dbx = fx - bx;
        float da = std::sqrt(dax * dax + day * day), db = std::sqrt(dbx * dbx + dby * dby);
        float v = fsin(da * ka - t * 0.062f) * 0.4f
                + fsin(db * kb + t * 0.048f) * 0.36f
                + fsin(fx * px + fy * py + t * 0.028f) * 0.24f;
        float u = v * 0.5f + 0.5f;
        // Contour lines: the triangle wave crosses zero at each band edge and
        // the cube turns the crossing into a thin, soft line.
        float f = u * bands;
        float edge = 1 - std::abs(2 * (f - std::floor(f)) - 1);
        float e2 = edge * edge, e4 = e2 * e2;
        float strength = e4 * e4 * sharp * lift;
        if (strength > 3.0f) strength = 3.0f;
        // Colour follows the field, not the contour, so a single line changes
        // ink as it crosses the frame.
        float mix = u * 2.0f;
        unsigned lo = unsigned(mix) % 3, hi = (lo + 1) % 3;
        float blend = mix - float(unsigned(mix));
        Color c = {ink[lo].r + (ink[hi].r - ink[lo].r) * blend,
                   ink[lo].g + (ink[hi].g - ink[lo].g) * blend,
                   ink[lo].b + (ink[hi].b - ink[lo].b) * blend};
        float d = dither(int(x), int(y));
        int r = gr + int(c.r * strength * (31.0f / 255.0f) + d);
        int g = gg + int(c.g * strength * (63.0f / 255.0f) + d);
        int b = gb + int(c.b * strength * (31.0f / 255.0f) + d);
        frame[y * width + x] = uint16_t((std::min(r, 31) << 11) | (std::min(g, 63) << 5) | std::min(b, 31));
      }
    }
  }

  void pushLayer() {
    for (unsigned i = layers.size() - 1; i > 0; --i) layers[i] = layers[i - 1];
    Layer& l = layers[0];
    l.base = 0;
    l.target = range(7.0f, 16.5f);
    l.amp1 = range(2.5f, 8.5f);
    l.freq1 = range(0.0035f, 0.011f);
    l.ph1 = unit();
    l.amp2 = range(0.6f, 2.6f);
    l.freq2 = range(0.016f, 0.038f);
    l.ph2 = unit();
    l.tone = unit();
    l.glow = range(0.35f, 1.0f);
    if (layerCount < layers.size()) ++layerCount;
    float cumulative = 0;
    for (unsigned i = 0; i < layerCount; ++i) { cumulative += layers[i].target; layers[i].target = cumulative; }
  }
  void renderStrata(float dt) {
    settleAt -= dt;
    if (settleAt <= 0) { settleAt = 5 + evolveUnit() * 5; pushLayer(); }
    for (unsigned i = 0; i < layerCount; ++i) {
      Layer& l = layers[i];
      l.base += (l.target - l.base) * std::min(1.0f, dt * 0.85f);
    }
    float sway = evolving[0] * 4 - 2;
    clear();
    for (unsigned x = 0; x < width; ++x) {
      float fx = float(x);
      float previous = -4;
      for (unsigned i = 0; i < layerCount; ++i) {
        const Layer& l = layers[i];
        float edge = l.base + l.amp1 * fsin(fx * l.freq1 + l.ph1 + phase * 0.012f)
                            + l.amp2 * fsin(fx * l.freq2 + l.ph2 - phase * 0.021f) + sway * float(i) * 0.12f;
        unsigned mix3 = unsigned(l.tone * 3.0f) % 3;
        const Color& body = ink[mix3];
        int top = std::max(0, int(previous));
        int bottom = std::min(int(height) - 1, int(edge));
        float thickness = std::max(1.0f, edge - previous);
        for (int y = top; y <= bottom; ++y) {
          float local = (float(y) - previous) / thickness;
          // Each band is darkest where it was buried and lightest where it
          // meets the next; the grain keeps the fill from looking printed.
          float shade = 0.22f + 0.50f * local + 0.07f * grain(int(x), y);
          shade *= 0.60f + 0.40f * l.tone;
          float d = dither(int(x), y);
          int r = int(body.r * shade * (31.0f / 255.0f) + d);
          int g = int(body.g * shade * (63.0f / 255.0f) + d);
          int b = int(body.b * shade * (31.0f / 255.0f) + d);
          frame[unsigned(y) * width + x] = uint16_t((std::min(r, 31) << 11) | (std::min(g, 63) << 5) | std::min(b, 31));
        }
        splat(fx, edge, glow, l.glow * (0.55f + breath * 0.7f));
        splat(fx, edge - 1.1f, ink[(mix3 + 1) % 3], l.glow * (0.30f + breath * 0.40f));
        previous = edge;
        if (edge > float(height)) break;
      }
      // Basement. The settling stack is not guaranteed to reach the bottom
      // of the frame, and an unfilled column below it reads as a hole.
      const Color& deep = ink[unsigned(layers[layerCount - 1].tone * 3.0f) % 3];
      for (int y = std::max(0, int(previous)); y < int(height); ++y) {
        float shade = 0.11f + 0.05f * grain(int(x), y);
        float d = dither(int(x), y);
        int r = int(deep.r * shade * (31.0f / 255.0f) + d);
        int g = int(deep.g * shade * (63.0f / 255.0f) + d);
        int b = int(deep.b * shade * (31.0f / 255.0f) + d);
        frame[unsigned(y) * width + x] = uint16_t((std::min(r, 31) << 11) | (std::min(g, 63) << 5) | std::min(b, 31));
      }
    }
  }

  void newFigure() {
    // Near-integer frequency ratios draw closed figures; the small detune is
    // what makes the curve precess instead of retracing one line.
    float base = range(1.4f, 3.1f);
    static const float ratios[6] = {1.0f, 2.0f, 3.0f, 1.5f, 2.5f, 4.0f};
    for (unsigned i = 0; i < 4; ++i) {
      penFreq[i] = base * ratios[unsigned(unit() * 6.0f) % 6] * (1 + (unit() - 0.5f) * 0.012f);
      penPhase[i] = unit();
      penSize[i] = range(0.25f, 1.0f);
    }
    float sum01 = penSize[0] + penSize[1], sum23 = penSize[2] + penSize[3];
    penSize[0] /= sum01; penSize[1] /= sum01; penSize[2] /= sum23; penSize[3] /= sum23;
    penT = 0;
    penEnvelope = 1;
    // Per unit of pen time, not per second: the pen advances about a tenth of
    // a unit a second, so this is a figure that has visibly drawn itself
    // tighter by the time it is replaced.
    penDecay = range(0.12f, 0.34f);
    figureAt = range(17.0f, 27.0f);
  }
  void renderHarmonograph(float dt) {
    // The pen is deliberately slow. Run it at the speed the figure is
    // traversed and it closes its whole loop several times a second, which
    // fills the frame in a moment and then has nothing left to show. At this
    // speed it stays a line that is still drawing itself, and the decay
    // clears the oldest part of the stroke as the newest arrives.
    settle(249);
    figureAt -= dt;
    if (figureAt <= 0) newFigure();
    float span = dt * (0.10f + parameters[1] * 0.07f) * (1.0f + breath * 0.3f);
    unsigned steps = 18 + unsigned(evolving[0] * 22);
    float step = span / float(steps);
    float rx = 74 + evolving[1] * 38, ry = 42 + evolving[2] * 20;
    float decayPerStep = std::exp(-penDecay * step);
    float lx = 0, ly = 0, env = penEnvelope;
    for (unsigned i = 0; i <= steps; ++i) {
      float t = penT + step * float(i);
      if (i) env *= decayPerStep;
      // The envelope bottoms out rather than collapsing to a point, so a long
      // figure ends up tighter than it started but still worth watching.
      float amp = 0.34f + 0.66f * env;
      float x = 120 + rx * amp * (penSize[0] * fsin(penFreq[0] * t + penPhase[0])
                                + penSize[1] * fsin(penFreq[1] * t + penPhase[1]));
      float y = 67 + ry * amp * (penSize[2] * fsin(penFreq[2] * t + penPhase[2])
                               + penSize[3] * fsin(penFreq[3] * t + penPhase[3]));
      if (i == 0) { lx = x; ly = y; continue; }
      // Colour travels along the stroke, so the trail reads as a gradient
      // from the head back into what has already been drawn.
      float hue = t * 0.85f;
      float band = hue - std::floor(hue);
      unsigned a = unsigned(band * 3.0f) % 3, b = (a + 1) % 3;
      float blend = band * 3.0f - float(unsigned(band * 3.0f));
      Color c = {ink[a].r + (ink[b].r - ink[a].r) * blend,
                 ink[a].g + (ink[b].g - ink[a].g) * blend,
                 ink[a].b + (ink[b].b - ink[a].b) * blend};
      float weight = 0.46f + breath * 0.44f;
      stroke(lx, ly, x, y, c, weight);
      // A second pass across the stroke gives the ribbon some body; a single
      // hairline at this size disappears into the ground.
      float ox = y - ly, oy = lx - x;
      float length = std::sqrt(ox * ox + oy * oy);
      if (length > 0.001f) {
        ox = ox / length * 0.8f; oy = oy / length * 0.8f;
        stroke(lx + ox, ly + oy, x + ox, y + oy, c, weight * 0.45f);
        stroke(lx - ox, ly - oy, x - ox, y - oy, c, weight * 0.45f);
      }
      lx = x; ly = y;
    }
    splat(lx, ly, glow, 0.5f + breath * 0.5f);
    penT += span;
    penEnvelope = env;
  }

  void seedGrowth() {
    // Roots enter from one of the short sides and cross the long axis, the
    // only direction with room enough for a structure to develop before it
    // runs out of frame.
    nodeCount = 0;
    bool fromLeft = unit() < 0.5f;
    unsigned roots = 2 + unsigned(unit() * 2.5f);
    for (unsigned i = 0; i < roots; ++i) {
      Node& n = nodes[nodeCount++];
      n.x = fromLeft ? range(-2.0f, 8.0f) : range(232.0f, 242.0f);
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
  void growStep() {
    unsigned live = 0;
    unsigned existing = nodeCount;
    for (unsigned i = 0; i < existing; ++i) {
      Node& n = nodes[i];
      if (!n.alive) continue;
      float reach = 1.5f + n.weight * 0.45f;
      // The lean is per step and accumulates, so it stays small: a tenth of
      // this and the branch spirals instead of reaching.
      float lean = flowAngle(n.x, n.y, phase) * 0.011f + (unit() - 0.5f) * 0.006f;
      float c = fcos(lean), s = fsin(lean);
      float dx = n.dx * c - n.dy * s, dy = n.dx * s + n.dy * c;
      float length = std::sqrt(dx * dx + dy * dy);
      n.dx = dx / length; n.dy = dy / length;
      float nx = n.x + n.dx * reach, ny = n.y + n.dy * reach;
      const Color& c1 = ink[n.depth % 3];
      stroke(n.x, n.y, nx, ny, c1, 0.07f + n.weight * 0.075f);
      if (n.weight > 1.3f) {
        // Heavy branches are drawn twice, offset across their direction, so
        // the structure tapers from a trunk to a hair.
        float ox = -n.dy * n.weight * 0.26f, oy = n.dx * n.weight * 0.26f;
        stroke(n.x + ox, n.y + oy, nx + ox, ny + oy, c1, 0.035f + n.weight * 0.035f);
        stroke(n.x - ox, n.y - oy, nx - ox, ny - oy, c1, 0.035f + n.weight * 0.035f);
      }
      n.x = nx; n.y = ny;
      n.weight *= 0.9955f;
      bool escaped = nx < 2 || nx > float(width) - 2 || ny < 2 || ny > float(height) - 2;
      if (escaped || n.weight < 0.30f || n.depth > 30) { n.alive = false; continue; }
      // A tip that runs into ground already taken stops there. Growth then
      // competes for the empty parts of the frame instead of piling up into
      // a white mass, and the structure ends up with its own negative space.
      int ax = int(nx + n.dx * reach * 2), ay = int(ny + n.dy * reach * 2);
      if (ax >= 0 && ax < int(width) && ay >= 0 && ay < int(height)) {
        uint16_t here = frame[unsigned(ay) * width + unsigned(ax)];
        unsigned lit = ((here >> 11) & 31) + (((here >> 5) & 63) >> 1) + (here & 31);
        if (lit > 40) { n.alive = false; continue; }
      }
      // A split costs both children some weight, so the structure thins as
      // it spreads and finishes on its own rather than filling the screen.
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
  void renderDelta(float dt) {
    if (liveNodes > 0) {
      // One to three growth steps a frame keeps the structure arriving at a watchable
      // pace on the device's twelve frames a second.
      unsigned ticks = 1 + unsigned(evolving[0] * 1.6f);
      for (unsigned i = 0; i < ticks; ++i) growStep();
      for (unsigned i = 0; i < nodeCount; ++i)
        if (nodes[i].alive) bloom(nodes[i].x, nodes[i].y, 1.3f + nodes[i].weight * 0.7f, glow, 0.07f + breath * 0.16f);
    } else {
      // Finished. The structure breathes with the music, dims, and a new one
      // starts from somewhere else.
      growthHold += dt;
      settle(growthHold > 5.0f ? 230 : 249);
      // Terminals keep a small light on them, so the finished structure
      // pulses with the music instead of standing still. The alpha has to
      // stay under the decay or the same pixels burn out to white.
      for (unsigned i = 0; i < nodeCount; i += 2) {
        const Node& n = nodes[i];
        if (n.depth < 3) continue;
        bloom(n.x, n.y, 1.4f, ink[n.depth % 3], 0.012f + breath * 0.075f);
      }
      if (growthHold > 8.5f) { clear(); seedGrowth(); }
    }
  }

  void renderVeil() {
    float t = phase;
    constexpr unsigned curtains = 4;
    float centre[curtains], weightScale[curtains], hueMix[curtains];
    for (unsigned k = 0; k < curtains; ++k) {
      float fk = float(k);
      centre[k] = (fk + 0.5f) * (float(width) / float(curtains)) + (evolving[k] - 0.5f) * 76;
      weightScale[k] = 9 + evolving[(k + 3) % 8] * 17;
      hueMix[k] = fk / float(curtains - 1);
    }
    float shimmerRate = 0.05f + parameters[2] * 0.09f;
    float lift = 3.2f + breath * 2.4f;
    for (unsigned y = 0; y < height; ++y) {
      float fy = float(y);
      uint16_t ground = rowGround[y];
      int gr = int((ground >> 11) & 31), gg = int((ground >> 5) & 63), gb = int(ground & 31);
      // Curtains hang from the top and brighten toward their feet, then cut
      // off softly, the way an aurora sits above a horizon.
      float foot = 1 - std::abs(fy / float(height) - 0.72f) * 1.55f;
      foot = std::max(0.0f, foot);
      float vertical = foot * foot * (0.35f + 0.65f * (fy / float(height)));
      float cx[curtains], amp[curtains];
      for (unsigned k = 0; k < curtains; ++k) {
        cx[k] = centre[k] + fsin(fy * 0.0075f + t * (0.021f + 0.008f * float(k)) + float(k) * 0.37f) * (18 + evolving[6] * 34)
                          + fsin(fy * 0.021f - t * 0.033f) * 5.0f;
        amp[k] = vertical * lift * (0.55f + 0.45f * fsin(t * 0.045f + float(k) * 0.41f));
      }
      for (unsigned x = 0; x < width; ++x) {
        float fx = float(x) + 0.5f;
        float r = 0, g = 0, b = 0;
        for (unsigned k = 0; k < curtains; ++k) {
          float offset = fx - cx[k];
          float d = offset / weightScale[k];
          float intensity = amp[k] / (1 + d * d * 3.2f);
          if (intensity < 0.006f) continue;
          // Fine vertical rays. The striation is measured across the curtain
          // rather than across the screen, so it travels with the sheet
          // instead of standing on the frame as a fixed comb.
          float rays = 0.72f + 0.28f * fsin(offset * (0.085f + 0.03f * float(k)) + t * shimmerRate + fy * 0.006f);
          intensity *= rays;
          float m = hueMix[k];
          unsigned a = unsigned(m * 2.0f) % 3, c2 = (a + 1) % 3;
          float blend = m * 2.0f - float(unsigned(m * 2.0f));
          r += (ink[a].r + (ink[c2].r - ink[a].r) * blend) * intensity;
          g += (ink[a].g + (ink[c2].g - ink[a].g) * blend) * intensity;
          b += (ink[a].b + (ink[c2].b - ink[a].b) * blend) * intensity;
        }
        float d = dither(int(x), int(y));
        int ri = gr + int(r * (31.0f / 255.0f) + d);
        int gi = gg + int(g * (63.0f / 255.0f) + d);
        int bi = gb + int(b * (31.0f / 255.0f) + d);
        frame[y * width + x] = uint16_t((std::min(ri, 31) << 11) | (std::min(gi, 63) << 5) | std::min(bi, 31));
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
    // Three clear, well separated hues and a white for highlights. The first
    // set was built from muted neighbours on the wheel and read as dull on
    // the panel. These are poster colours, far enough apart that a blend
    // between any two of them stays a colour instead of turning to grey.
    static const Color palettes[6][4] = {
      {{255,  78,  66}, { 64, 132, 255}, {255, 214,  56}, {255, 255, 250}},  // poster
      {{255,  72, 160}, { 40, 224, 238}, {176, 245,  84}, {250, 255, 255}},  // playground
      {{255, 138,  30}, {236,  66, 190}, { 68, 220, 124}, {255, 252, 240}},  // fruit
      {{  0, 226, 206}, {116, 112, 255}, {255, 202,  58}, {240, 255, 255}},  // sea
      {{255, 124, 190}, { 92, 192, 255}, {124, 246, 190}, {255, 250, 255}},  // candy
      {{154,  92, 255}, {255,  76, 122}, {255, 206,  72}, {255, 248, 252}},  // berry
    };
    for (unsigned i = 0; i < 3; ++i) ink[i] = palettes[palette][i];
    glow = palettes[palette][3];
    // One ground colour for the whole screen, tinted toward the palette and
    // held within a step of black. A gradient was tried first and could not
    // survive five bits of red: it arrived on the display as two or three
    // horizontal bands, which is worse than no gradient at all. Depth is the
    // drawing's job here, not the ground's.
    Color base = {ink[1].r * 0.012f + 3, ink[1].g * 0.012f + 4, ink[1].b * 0.016f + 9};
    uint16_t ground = uint16_t((std::min(int(base.r * (31.0f / 255.0f) + 0.5f), 31) << 11)
                             | (std::min(int(base.g * (63.0f / 255.0f) + 0.5f), 63) << 5)
                             | std::min(int(base.b * (31.0f / 255.0f) + 0.5f), 31));
    rowGround.fill(ground);
    evolutionRng = (rng ^ 0x85ebca6bu) | 1u;
    evolutionAt = 0;
    for (unsigned i = 0; i < goals.size(); ++i) evolving[i] = goals[i] = evolveUnit();
    for (unsigned i = 0; i < drops.size(); ++i) respawn(drops[i], i);
    layerCount = 0;
    settleAt = 0;
    for (unsigned i = 0; i < 13; ++i) pushLayer();
    for (unsigned i = 0; i < layerCount; ++i) layers[i].base = layers[i].target;
    newFigure();
    growthHold = 0;
    seedGrowth();
    clear();
  }

  unsigned generation() const { return count; }
  unsigned visualFamily() const { return kind; }
  const uint16_t* pixels() const { return frame.data(); }

  void render(float seconds, float audio) {
    seconds = std::max(0.0f, std::min(0.1f, seconds));
    ++tick;
    phase += seconds * pace;
    if (phase > 100000.0f) phase -= 100000.0f;
    breath += (std::max(0.0f, std::min(1.0f, audio)) - breath) * std::min(1.0f, seconds * 5);
    evolve(seconds);
    switch (kind) {
      case Current: renderCurrent(seconds); break;
      case Interference: renderInterference(); break;
      case Strata: renderStrata(seconds); break;
      case Harmonograph: renderHarmonograph(seconds); break;
      case Delta: renderDelta(seconds); break;
      default: renderVeil(); break;
    }
  }
};
}
