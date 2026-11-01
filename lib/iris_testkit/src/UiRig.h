// A complete fake device: all hardware fakes + the real services + the real UI. Used by the UI
// tests and by the console simulator, so both exercise exactly the same wiring.
#pragma once

#include <initializer_list>
#include <string>

#include "FakeBattery.h"
#include "FakeButtons.h"
#include "FakeClock.h"
#include "FakeDisplay.h"
#include "FakeIrReceiver.h"
#include "FakeIrTransmitter.h"
#include "MemoryBlobStore.h"
#include "app/Boot.h"
#include "ui/AppScreens.h"
#include "ui/Ui.h"

namespace iris::testkit {

struct UiRig {
  FakeClock clock;
  MemoryBlobStore flash;
  FakeIrReceiver rx;
  FakeIrTransmitter tx{clock};
  FakeDisplay display;
  FakeButtons buttons;
  FakeBattery battery;
  RingLogSink log{clock, 200};

  LibraryRepository repo{flash};
  RemoteLibrary library;
  SettingsStore settingsStore{flash};
  Settings settings;
  LearnSession learn{rx, clock, &log};
  TransmitService transmit{tx, &log};
  ExplorationRun explore{transmit, clock, &log};
  DeviceInfo info;
  UiContext ctx;
  ButtonDebouncer debouncer{buttons, clock};
  UiApp ui;

  UiRig()
      : ctx(library, repo, settingsStore, settings, learn, transmit, explore, rx, buttons, battery, log, clock, flash,
            info),
        ui(ctx, display, makeHomeScreen()) {
    info.firmware = "sim";
    info.displayController = "FakeDisplay";
    info.irLibrary = "simulado";
    info.storage = "RAM (simulado)";
    info.pinLines = {"Pines: n/d (simulado)"};
    const BootReport boot = loadPersistentData(flash, repo, library, settingsStore, settings, log);
    ctx.persistEnabled = boot.persistEnabled;
    if (!boot.notice.empty()) ctx.toast(boot.notice, 4000);
    display.begin();
    ui.start();
    step();
  }
  UiRig(const UiRig&) = delete;
  UiRig& operator=(const UiRig&) = delete;

  // One main-loop iteration: debounce raw buttons, deliver events, run services and redraw.
  void step() {
    std::vector<KeyEvent> events;
    debouncer.poll(events);
    for (const KeyEvent& e : events) ui.handleKey(e);
    ui.tick();
  }

  void advance(uint32_t ms) {
    for (uint32_t done = 0; done < ms;) {
      const uint32_t chunk = ms - done < 10 ? ms - done : 10;
      clock.advanceMs(chunk);
      done += chunk;
      step();
    }
    step();
  }

  // Physical-style input through the real debouncer.
  void hold(Key k, uint32_t ms) {
    buttons.set(k, true);
    advance(ms);
    buttons.set(k, false);
    advance(60);
  }
  void press(Key k) { hold(k, 80); }
  void press(std::initializer_list<Key> keys) {
    for (Key k : keys) press(k);
  }
  void longPress(Key k) { hold(k, 900); }

  std::string screen() const { return display.grid(); }
  bool shows(const std::string& s) const { return display.showsText(s); }
  std::string topId() { return ui.top() ? ui.top()->id() : ""; }
};

}  // namespace iris::testkit
