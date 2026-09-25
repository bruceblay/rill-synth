// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>

// The clock half of ensemble sync: what a device broadcasts, how a follower
// works out the offset between its clock and the conductor's, and where the
// next beat lands in local time.
//
// None of this touches the radio or the audio hardware, so it can be driven
// with a fake clock on the host, which is the only way to test a protocol
// whose failure mode is a few hundred microseconds of skew.
//
// Two rules from the design proposal are built in here. Receivers schedule
// future events from the shared grid rather than triggering on packet
// arrival, so a late or lost packet costs nothing. And corrections move a
// scheduled deadline gradually; the running sample clock is never jumped.
namespace ensemble {

constexpr uint32_t magic = 0x524c4e4b;  // "RLNK"
// Named ensemble rather than link: POSIX has a link() and the Arduino
// headers drag it in, so a namespace by that name collides on the device
// while compiling perfectly well on the host.
constexpr uint8_t version = 2;  // 2: tempo proposals, and devices that defer the clock

// Everything an ensemble needs to agree on, in explicit integers. Sent as
// one ESP-NOW broadcast, well under the 250-byte payload limit.
struct Packet {
  uint32_t magic;
  uint8_t version;
  uint8_t role;        // 0 listening, 1 conducting
  uint16_t sequence;
  uint32_t conductor;  // low four bytes of the conductor's MAC: a stable id
  uint32_t session;    // changes when the conductor restarts or regenerates
  int64_t now;         // conductor's monotonic microseconds at send
  int64_t nextBeat;    // conductor's time of the next beat
  uint32_t beatPeriod; // microseconds between beats
  uint32_t beatIndex;  // beats since the session began
  uint16_t tempo;      // beats per minute, for display and for a solo fallback
  uint16_t bars;       // beats to a bar, so joiners land on a bar line
  // Harmony travels separately from the clock and on its own authority. The
  // device that last proposed a key holds the highest epoch, so whoever was
  // tapped most recently leads the ensemble into a new key -- which is what a
  // player expects from the device in their hand, and is not something the
  // clock's election should decide, since a drum machine has no key to offer.
  uint32_t harmonyEpoch;
  uint32_t harmonyFrom;  // proposer's id, to break a tie between equal epochs
  uint8_t tonic;         // 0-11
  uint8_t mode;          // 0-2
  uint8_t pitched;       // 1 if this device has a key of its own to offer
  // 1 if the sender only keeps the clock when nobody else will: a texture
  // with no pulse of its own (World) is the worst device to hold the tempo.
  uint8_t defers;
  // Tempo, like harmony, can be proposed by any device and the newest
  // proposal wins. Every device switches on the same shared beat, a bar line
  // far enough ahead for the proposal to have reached them all.
  uint32_t tempoEpoch;
  uint32_t tempoFrom;
  uint32_t tempoBeat;    // shared beat index the new tempo starts on
  uint16_t tempoWanted;  // beats per minute
} __attribute__((packed));

class Clock {
 public:
  // A device conducts until it hears from one that outranks it, which is the
  // whole of the election: stable, needs no negotiation, and settles in one
  // packet. A device that defers ranks below every one that does not; after
  // that the lower id wins. Ties cannot happen, since the id is a MAC.
  void begin(uint32_t id, int64_t now, uint32_t tempo) {
    self_ = id;
    conductor_ = id;
    conductorDefers_ = defers_;
    session_ = id ^ uint32_t(now);
    setTempo(tempo);
    beatIndex_ = 0;
    nextBeat_ = now + int64_t(beatPeriod_);
    offset_ = 0;
    offsetJitter_ = 0;
    drift_ = 0;
    lastHeard_ = 0;
    received_ = 0;
    missed_ = 0;
    lastSequence_ = 0;
    haveOffset_ = false;
  }
  // Keep the clock only when no other device will.
  void setDefers(bool yes) { defers_ = yes; if (conductor_ == self_) conductorDefers_ = yes; }
  static bool outranks(bool aDefers, uint32_t a, bool bDefers, uint32_t b) {
    return aDefers != bDefers ? !aDefers : a < b;
  }
  // Ask the ensemble for a new tempo. It starts on the first bar line at
  // least a whole bar away, so the proposal reaches every device first.
  void proposeTempo(unsigned bpm) {
    const uint32_t bars = bars_ ? bars_ : 4;
    tempoWanted_ = uint16_t(std::max(20u, std::min(240u, bpm)));
    tempoBeat_ = (beatIndex_ / bars + 2) * bars;
    ++tempoEpoch_;
    tempoFrom_ = self_;
  }
  // The tempo the ensemble is headed for: the proposal if one is waiting.
  uint32_t tempoAhead() const { return appliedEpoch_ != tempoEpoch_ ? tempoWanted_ : tempo_; }
  void setTempo(uint32_t tempo) {
    tempo_ = std::max(20u, std::min(240u, tempo));
    beatPeriod_ = 60000000u / tempo_;
  }

  bool conducting() const { return conductor_ == self_; }
  uint32_t conductor() const { return conductor_; }
  uint32_t tempo() const { return tempo_; }
  uint32_t beatPeriod() const { return beatPeriod_; }
  uint32_t beatIndex() const { return beatIndex_; }
  int64_t nextBeat() const { return nextBeat_; }
  // What the last packets say about the two clocks: the offset between them,
  // how much that estimate is moving about, and the rate the two crystals are
  // running apart at.
  int64_t offset() const { return offset_; }
  int64_t offsetJitter() const { return offsetJitter_; }
  float driftPpm() const { return drift_; }
  uint32_t received() const { return received_; }
  uint32_t missed() const { return missed_; }
  int64_t lastHeard() const { return lastHeard_; }

  // Fill a packet to broadcast. Only a conductor should send one, but the
  // role travels in the packet so a listener's silence is explicit.
  Packet outgoing(int64_t now) {
    Packet p{};
    p.magic = magic;
    p.version = version;
    p.role = conducting() ? 1 : 0;
    p.sequence = ++sequence_;
    p.conductor = conductor_;
    p.session = session_;
    p.now = now;
    p.nextBeat = nextBeat_;
    p.beatPeriod = beatPeriod_;
    p.beatIndex = beatIndex_;
    p.tempo = uint16_t(tempo_);
    p.bars = bars_;
    p.harmonyEpoch = epoch_;
    p.harmonyFrom = harmonyFrom_;
    p.tonic = tonic_;
    p.mode = mode_;
    p.pitched = pitched_ ? 1 : 0;
    p.defers = defers_ ? 1 : 0;
    p.tempoEpoch = tempoEpoch_;
    p.tempoFrom = tempoFrom_;
    p.tempoBeat = tempoBeat_;
    p.tempoWanted = tempoWanted_;
    return p;
  }

  // A packet has arrived. `now` is the local clock at the moment of receipt.
  // A device with a key of its own. One without -- a drum machine, a noise
  // wash -- still carries harmony for the others, since relaying costs
  // nothing and keeps a key alive across a device that cannot use it.
  void setPitched(bool yes) { pitched_ = yes; }
  // Called when this device generates a new piece. The epoch is what makes
  // the most recent proposal win, whoever made it.
  void proposeHarmony(unsigned tonic, unsigned mode) {
    tonic_ = uint8_t(tonic % 12);
    mode_ = uint8_t(mode % 3);
    ++epoch_;
    harmonyFrom_ = self_;
  }
  unsigned harmonyTonic() const { return tonic_; }
  unsigned harmonyMode() const { return mode_; }
  uint32_t harmonyEpoch() const { return epoch_; }

  void receive(const Packet& p, int64_t now) {
    if (p.magic != magic || p.version != version) return;
    // Harmony rides on every packet, from a conductor or not: the key is not
    // the clock's to decide.
    bool newer = p.harmonyEpoch > epoch_ ||
                 (p.harmonyEpoch == epoch_ && p.harmonyEpoch != 0 && p.harmonyFrom < harmonyFrom_);
    if (newer && p.harmonyEpoch != 0) {
      epoch_ = p.harmonyEpoch;
      harmonyFrom_ = p.harmonyFrom;
      tonic_ = p.tonic;
      mode_ = p.mode;
    }
    // So does tempo: the newest proposal is taken up wherever it came from.
    bool newerTempo = p.tempoEpoch > tempoEpoch_ ||
                      (p.tempoEpoch == tempoEpoch_ && p.tempoEpoch != 0 && p.tempoFrom < tempoFrom_);
    if (newerTempo && p.tempoEpoch != 0) {
      tempoEpoch_ = p.tempoEpoch;
      tempoFrom_ = p.tempoFrom;
      tempoBeat_ = p.tempoBeat;
      tempoWanted_ = p.tempoWanted;
    }
    if (p.role != 1) return;
    // The higher-ranked device conducts. A device that was conducting steps
    // down here, and one that was following ignores anything from a device
    // ranked below the conductor it already has.
    if (outranks(conductorDefers_, conductor_, p.defers != 0, p.conductor)) return;
    bool changed = p.conductor != conductor_ || p.session != session_;
    conductor_ = p.conductor;
    conductorDefers_ = p.defers != 0;
    session_ = p.session;
    if (!changed) {
      uint16_t expected = uint16_t(lastSequence_ + 1);
      if (p.sequence != expected) missed_ += uint16_t(p.sequence - expected);
    }
    lastSequence_ = p.sequence;
    ++received_;
    lastHeard_ = now;

    // The offset the packet implies, ignoring flight time: ESP-NOW delivers
    // in a couple of milliseconds, and since every packet pays roughly the
    // same it shows up as a constant bias rather than as jitter. The
    // diagnostic reports the spread so the real figure can be measured
    // instead of assumed.
    int64_t sample = p.now - now;
    if (!haveOffset_ || changed) {
      offset_ = sample;
      offsetJitter_ = 0;
      drift_ = 0;
      haveOffset_ = true;
      driftAnchorLocal_ = now;
      driftAnchorOffset_ = sample;
    } else {
      int64_t error = sample - offset_;
      offsetJitter_ += (std::abs(error) - offsetJitter_) / 8;
      // A slow filter: a single late packet should not move the grid.
      offset_ += error / 8;
      int64_t span = now - driftAnchorLocal_;
      if (span > 5000000) {
        drift_ = float(double(offset_ - driftAnchorOffset_) / double(span) * 1e6);
        driftAnchorLocal_ = now;
        driftAnchorOffset_ = offset_;
      }
    }

    // Once this device has changed tempo on the agreed beat, a packet sent
    // before the sender reached that beat (its next beat is still the agreed
    // one, or earlier) carries the old tempo and the old grid. Taking
    // it would pull the tempo back for a moment; wait for the next one.
    if (tempoEpoch_ && p.tempoEpoch == tempoEpoch_ && appliedEpoch_ == tempoEpoch_ &&
        int32_t(p.beatIndex - tempoBeat_) <= 0 && int32_t(beatIndex_ - tempoBeat_) > 0)
      return;
    setTempo(p.tempo);
    beatPeriod_ = p.beatPeriod;
    bars_ = p.bars;
    int64_t period = int64_t(beatPeriod_);
    // Their grid in local time. The beat this names may already have passed,
    // since the packet took a moment to arrive; what matters is the grid it
    // describes, not that one beat.
    int64_t theirNext = p.nextBeat - offset_;
    if (changed) {
      // Joining: take their grid outright, at the first beat still ahead.
      int64_t beat = theirNext;
      int64_t steps = 0;
      while (beat < now) { beat += period; ++steps; }
      nextBeat_ = beat;
      beatIndex_ = p.beatIndex + uint32_t(steps);
    } else {
      // Correct the phase only, folded to the nearest beat. Comparing raw
      // times instead compares two different beats whenever a packet arrives
      // near a beat line, and easing toward that walks the clock a fraction
      // of a beat at a time until whole beats have been gained or dropped.
      int64_t error = ((theirNext - nextBeat_) % period + period) % period;
      if (error > period / 2) error -= period;
      nextBeat_ += error / 4;
      // Keep the count aligned to theirs without moving the deadline.
      // Rounded down, not toward zero: a follower that has not yet ticked past
      // a beat is behind the packet's beat, the numerator goes negative, and
      // C++ division would round it up to zero -- counting this device a whole
      // beat ahead until the next packet put it back. Only the count showed it
      // until tempo changes began on an agreed beat: then the follower changed
      // a beat early. (Rill Chamber found and fixed this first.)
      int64_t numerator = nextBeat_ - theirNext + period / 2;
      int64_t ahead = numerator >= 0 ? numerator / period : -((-numerator + period - 1) / period);
      beatIndex_ = uint32_t(int64_t(p.beatIndex) + ahead);
    }
  }

  // Has a beat arrived? Call often; it returns true once per beat and moves
  // the deadline on by exactly one period, so a slow caller never drifts.
  // A proposed tempo takes over on its beat: the time to the beat after it is
  // already the new period, on every device at once.
  bool due(int64_t now) {
    if (now < nextBeat_) return false;
    if (tempoEpoch_ && appliedEpoch_ != tempoEpoch_) {
      const int32_t ahead = int32_t(tempoBeat_ - beatIndex_);
      // A beat far ahead belongs to a timeline since restarted: take it now.
      if (ahead <= 0 || ahead > 256) {
        appliedEpoch_ = tempoEpoch_;
        setTempo(tempoWanted_);
      }
    }
    nextBeat_ += int64_t(beatPeriod_);
    ++beatIndex_;
    return true;
  }

  // No conductor heard for a while: take over. Keeping the beat grid where
  // it is means the take-over is silent.
  bool checkTimeout(int64_t now, int64_t patience = 2000000) {
    if (conducting()) return false;
    if (lastHeard_ == 0 || now - lastHeard_ < patience) return false;
    conductor_ = self_;
    conductorDefers_ = defers_;
    session_ = self_ ^ uint32_t(now);
    haveOffset_ = false;
    offset_ = 0;
    return true;
  }

 private:
  uint32_t self_ = 0, conductor_ = 0, session_ = 0;
  uint32_t tempo_ = 96, beatPeriod_ = 625000, beatIndex_ = 0;
  uint16_t sequence_ = 0, lastSequence_ = 0, bars_ = 4;
  int64_t nextBeat_ = 0, offset_ = 0, offsetJitter_ = 0, lastHeard_ = 0;
  int64_t driftAnchorLocal_ = 0, driftAnchorOffset_ = 0;
  float drift_ = 0;
  uint32_t received_ = 0, missed_ = 0;
  uint32_t epoch_ = 0, harmonyFrom_ = 0;
  uint8_t tonic_ = 0, mode_ = 0;
  bool haveOffset_ = false, pitched_ = false, defers_ = false, conductorDefers_ = false;
  uint32_t tempoEpoch_ = 0, tempoFrom_ = 0, tempoBeat_ = 0, appliedEpoch_ = 0;
  uint16_t tempoWanted_ = 0;
};
}
