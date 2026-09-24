// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <atomic>
#include <cstring>
#include "Ensemble.h"

// The device half of ensemble sync: ESP-NOW glue around ensemble::Clock, and
// the loop that keeps a local engine on the shared grid.
//
// The conductor is not the master here. Both devices run the same clock and
// both trim their engine onto it; the radio only keeps the two clocks
// agreeing. One code path, and a conductor sounds no different from a
// follower, which is what makes swapping roles at a bar line safe.
namespace radio {

inline ensemble::Clock clock_;
inline SemaphoreHandle_t lock_ = nullptr;
inline volatile bool pending_ = false;
inline ensemble::Packet inbox_{};
inline volatile int64_t inboxAt_ = 0;
inline uint8_t broadcast_[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
inline int64_t lastSend_ = 0;
inline bool up_ = false;

inline void onReceive(const uint8_t*, const uint8_t* data, int length) {
  int64_t at = esp_timer_get_time();
  if (length != int(sizeof(ensemble::Packet)) || pending_) return;
  std::memcpy(const_cast<ensemble::Packet*>(&inbox_), data, sizeof(ensemble::Packet));
  inboxAt_ = at;
  pending_ = true;
}

inline uint32_t deviceId() {
  uint8_t mac[6]{};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  return (uint32_t(mac[2]) << 24) | (uint32_t(mac[3]) << 16) | (uint32_t(mac[4]) << 8) | mac[5];
}

inline bool begin(unsigned tempo) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // A fixed channel: there is no access point to agree one with.
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t peer{};
  std::memcpy(peer.peer_addr, broadcast_, 6);
  peer.channel = 1;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) return false;
  lock_ = xSemaphoreCreateMutex();
  clock_.begin(deviceId(), esp_timer_get_time(), tempo);
  up_ = true;
  return true;
}

inline bool up() { return up_; }
inline bool conducting() { return clock_.conducting(); }
inline unsigned tempo() { return clock_.tempo(); }
inline uint32_t beatIndex() { return clock_.beatIndex(); }
inline int64_t offset() { return clock_.offset(); }
inline uint32_t heard() { return clock_.received(); }

// Called often from the loop: take in whatever arrived, speak if conducting,
// and hand back where the shared bar line sits.
inline void service(int64_t now, unsigned beatsPerBar) {
  if (!up_) return;
  if (xSemaphoreTake(lock_, portMAX_DELAY) != pdTRUE) return;
  if (pending_) {
    ensemble::Packet copy = inbox_;
    int64_t at = inboxAt_;
    pending_ = false;
    clock_.receive(copy, at);
  }
  clock_.checkTimeout(now);
  clock_.due(now);
  bool speak = clock_.conducting() && now - lastSend_ > 250000;
  ensemble::Packet out{};
  if (speak) { out = clock_.outgoing(now); lastSend_ = now; }
  xSemaphoreGive(lock_);
  if (speak) esp_now_send(broadcast_, reinterpret_cast<const uint8_t*>(&out), sizeof(out));
  (void)beatsPerBar;
}

// Microseconds from now until the next shared bar line, and how long a bar
// lasts. The engine compares its own place in the bar against this.
inline void barWindow(int64_t now, unsigned beatsPerBar, int64_t& untilBar, int64_t& barMicros) {
  barMicros = int64_t(clock_.beatPeriod()) * beatsPerBar;
  uint32_t index = clock_.beatIndex();
  int64_t next = clock_.nextBeat();
  // The beat that starts a bar is the next one whose index divides evenly.
  unsigned toGo = (beatsPerBar - (index % beatsPerBar)) % beatsPerBar;
  untilBar = next - now + int64_t(clock_.beatPeriod()) * toGo;
  while (untilBar < 0) untilBar += barMicros;
}
}
