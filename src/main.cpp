// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include <M5Unified.h>
#include <atomic>
#include <esp_system.h>
#include "Garden.h"
#include "Light.h"
#include "Radio.h"
#include "ShakeDetector.h"

// Working title. Display and controls run separately from the audio producer.
static garden::Engine engine;
static light::Painting painting;
static ShakeDetector shake;
static bool infoVisible = false, audioFailed = false;
static uint32_t infoAt = 0, worstVisualUs = 0;
static int16_t buffers[3][512];
static std::atomic<bool> playing{true}, changeRequested{false}, repaintRequested{false};
static std::atomic<uint32_t> sceneInfo{0}, audioLevel{0};
// Notes cross from the audio task to the display loop: the visuals answer
// individual notes, which a level meter cannot tell apart.
static std::atomic<uint32_t> struckNote{0}, struckWeight{600};
// The ensemble's tempo and phase error, handed to the audio task through
// atomics the same way every other request is.
static std::atomic<uint32_t> ensembleTempo{0};
static std::atomic<int32_t> gridTrim{0};
// A key offered by the ensemble, and the epoch it came with so the same
// offer is only handed over once.
static std::atomic<uint32_t> ensembleHarmony{0};
static std::atomic<uint32_t> worstRenderUs{0}, queueErrors{0};
static uint8_t volume = 165;

void audioTask(void*) {
  unsigned index = 0;
  for (;;) {
    if (changeRequested.exchange(false)) engine.newVariation();
    engine.setPlaying(playing.load());
    if (uint32_t bpm = ensembleTempo.exchange(0)) engine.followTempo(bpm);
    if (int32_t trim = gridTrim.exchange(0)) engine.trimGrid(trim);
    if (uint32_t harmony = ensembleHarmony.exchange(0))
      engine.adoptHarmony((harmony >> 8) & 15, harmony & 3);
    uint32_t start = micros();
    engine.render(buffers[index], 512);
    uint32_t elapsed = micros() - start;
    sceneInfo.store(engine.displayInfo());
    uint32_t energy = 0;
    for (auto sample : buffers[index]) energy += unsigned(std::abs(int(sample)));
    audioLevel.store(energy / 512);
    if (uint8_t struck = engine.drainOnset()) {
      struckWeight.store(uint32_t(engine.onsetWeight() * 1000));
      struckNote.store(struck);
    }
    if (elapsed > worstRenderUs) worstRenderUs = elapsed;
    while (!M5.Speaker.playRaw(buffers[index], 512, garden::rate, false, 1, 0)) {
      ++queueErrors;
      vTaskDelay(1);
    }
    index = (index + 1) % 3;
  }
}

void motionTask(void*) {
  for (;;) {
    if (M5.Imu.isEnabled() && (M5.Imu.update() & m5::IMU_Class::sensor_mask_accel)) {
      const auto data = M5.Imu.getImuData();
      if (shake.update(data.accel.x,data.accel.y,data.accel.z,millis())) repaintRequested = true;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void draw() {
  auto& d = M5.Display;
  d.fillScreen(0x1082);
  d.setTextColor(0xD692, 0x1082);
  d.setTextSize(3);
  d.setCursor(16, 14); d.print("RILL");
  d.drawFastHLine(16, 48, 208, 0x4208);
  d.setTextSize(2);
  uint32_t info = sceneInfo.load();
  static const char* tones[] = {"Bongo", "Bars", "Wood", "Bells", "Wire", "Halo", "Synth"};
  static const char* keys[] = {"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
  static const char* modes[] = {"maj","min","dor"};
  static const char* times[] = {"3:5", "Trip", "1:3", "Cross"};
  d.setCursor(16, 57); d.printf("%s %s %s", tones[(info >> 13) & 7], keys[(info >> 9) & 15], modes[(info >> 7) & 3]);
  d.setCursor(16, 82); d.printf("%02u %u BPM %s", unsigned(info >> 18), unsigned(info & 127), times[(info >> 16) & 3]);
  d.setCursor(16, 108);
  if (playing) d.printf("Vol %u%%", unsigned(volume) * 100 / 255);
  else d.print("resting");
  int battery = M5.Power.getBatteryLevel();
  d.setCursor(130,108);
  if (battery >= 0) d.printf("Bat %d%%", std::min(100,battery));
  else d.print("Bat --");
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true; cfg.internal_mic = false; cfg.internal_imu = true;
  M5.begin(cfg);
  Serial.begin(115200);
  engine.seed(esp_random());
  painting.seed(esp_random());
  sceneInfo.store(engine.displayInfo());
  M5.BtnA.setHoldThresh(650);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(100);
  M5.Speaker.setVolume(volume);
  painting.render(0,0);
  M5.Display.pushImage(0,0,240,135,reinterpret_cast<const lgfx::rgb565_t*>(painting.pixels()));
  if (!M5.Speaker.begin()) {
    audioFailed = true; M5.Display.fillScreen(0x1082);
    M5.Display.setTextSize(2); M5.Display.setCursor(16, 62); M5.Display.print("audio error");
    return;
  }
  if (!radio::begin(engine.bpm(), true))
    Serial.println("ensemble radio unavailable; playing alone");
  if (xTaskCreatePinnedToCore(motionTask, "garden-motion", 4096, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("Motion task unavailable");
  if (xTaskCreatePinnedToCore(audioTask, "garden-audio", 4096, nullptr, 3, nullptr, 1) != pdPASS) {
    audioFailed = true; M5.Display.fillScreen(0x1082);
    M5.Display.setTextSize(2); M5.Display.setCursor(16, 62); M5.Display.print("audio error");
  }
}

// Keep the engine on the shared bar line, measured against the bar rather
// than the beat so a device joining late lands where a bar starts.
void serviceEnsemble() {
  if (!radio::up()) return;
  int64_t now = esp_timer_get_time();
  radio::service(now, 4);
  static int64_t lastTrim = 0;
  if (now - lastTrim < 120000) return;
  lastTrim = now;
  ensembleTempo.store(radio::tempo());
  // Only when the offer is new, and never our own: proposing a key and then
  // adopting it back would restart the phrase for nothing.
  static uint32_t lastEpoch = 0;
  uint32_t epoch = radio::harmonyEpoch();
  if (epoch && epoch != lastEpoch) {
    lastEpoch = epoch;
    ensembleHarmony.store(0x10000u | (radio::harmonyTonic() << 8) | radio::harmonyMode());
  }
  int64_t untilBar = 0, barMicros = 0;
  radio::barWindow(now, 4, untilBar, barMicros);
  if (barMicros <= 0) return;
  int64_t barSamples = int64_t(engine.barSamples());
  if (barSamples <= 0) return;
  int64_t want = barSamples - (untilBar * int64_t(garden::rate)) / 1000000;
  while (want < 0) want += barSamples;
  want %= barSamples;
  int64_t error = want - int64_t(engine.barPhase());
  error = ((error % barSamples) + barSamples) % barSamples;
  if (error > barSamples / 2) error -= barSamples;
  // A quarter of the error at a time, spread over several bars so the
  // correction is never heard as a stumble.
  gridTrim.store(int32_t(error / 4));
}

void loop() {
  M5.update();
  serviceEnsemble();
  uint32_t now = millis();
  bool changed = false;
  if (M5.BtnA.wasClicked()) { changeRequested = true; playing = true; changed = true; }
  if (M5.BtnA.wasHold()) { playing = !playing; changed = true; }
  static uint32_t lastScene = 0;
  uint32_t currentScene = sceneInfo.load();
  const bool newMusic = lastScene != 0 && (currentScene >> 18) != (lastScene >> 18);
  if (currentScene != lastScene) { lastScene = currentScene; changed = true; }
  if (M5.BtnB.wasClicked()) {
    volume = volume >= 255 ? 45 : volume + 30;
    M5.Speaker.setVolume(volume); changed = true;
    infoVisible = true; infoAt = now;
  }
  static uint32_t frameAt = 0;
  const bool newVisual = repaintRequested.exchange(false);
  // A new piece here is an offer to the room: whoever was tapped last leads
  // the key.
  if (newMusic) radio::proposeHarmony(engine.keyRoot(), engine.keyMode());
  if (newMusic || newVisual) { painting.regenerate(); infoVisible=false; frameAt=now-83; }
  if (infoVisible && uint32_t(now - infoAt) >= 4000) infoVisible = false;
  static bool wasInfoVisible = false;
  if (!audioFailed) {
    if (infoVisible) {
      if (changed || !wasInfoVisible) draw();
    } else if (wasInfoVisible || uint32_t(now - frameAt) >= 83) {
      float dt = std::min(0.25f,float(uint32_t(now-frameAt))/1000);
      frameAt = now;
      uint32_t started = micros();
      painting.render(dt,float(audioLevel.load())/8000.0f,
                      uint8_t(struckNote.exchange(0)),float(struckWeight.load())/1000.0f);
      M5.Display.pushImage(0,0,240,135,reinterpret_cast<const lgfx::rgb565_t*>(painting.pixels()));
      worstVisualUs = std::max(worstVisualUs,uint32_t(micros()-started));
    }
  }
  wasInfoVisible = infoVisible;
  static uint32_t report = 0;
  if (millis() - report >= 10000) {
    report = millis();
    Serial.printf("render worst=%lu us / 16000 us; queue errors=%lu; heap=%u; scene=%lu BPM=%lu delay=%lu tone=%lu key=%lu mode=%lu visual=%u visual_us=%lu\n",
      (unsigned long)worstRenderUs.load(), (unsigned long)queueErrors.load(), ESP.getFreeHeap(),
      (unsigned long)(currentScene >> 18), (unsigned long)(currentScene & 127),
      (unsigned long)((currentScene >> 16) & 3), (unsigned long)((currentScene >> 13) & 7),
      (unsigned long)((currentScene >> 9) & 15), (unsigned long)((currentScene >> 7) & 3),painting.generation(),(unsigned long)worstVisualUs);
  }
  delay(10);
}
