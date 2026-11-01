// Stage 6 firmware: OLED + 5 buttons + UI + IR receive/transmit + LittleFS persistence, boot history, watchdog.
// Nothing is ever transmitted at boot: the emitter pin is driven low first and only a user action
// (or a user-started exploration) submits a job. Everything here compiles; none of it has run on the
// board yet (see docs/status.md).
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include <vector>

#include "app/Boot.h"
#include "app/MultiReceiver.h"
#include "board_pins.h"
#include "infrastructure/esp32/ArduinoClock.h"
#include "infrastructure/esp32/GpioButtons.h"
#include "infrastructure/esp32/IrReceiverAdapter.h"
#include "infrastructure/esp32/IrTransmitterAdapter.h"
#include "infrastructure/esp32/PersistentStore.h"
#include "infrastructure/esp32/U8g2Display.h"
#include "infrastructure/esp32/UnavailableIr.h"
#include "ui/AppScreens.h"
#include "ui/Ui.h"

namespace {

const char* resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "encendido";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "WDT int.";
    case ESP_RST_TASK_WDT: return "WDT tarea";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_EXT: return "pin EN";
    default: return "otro";
  }
}

// Everything the UI needs, built once in setup().
struct Device {
  ArduinoClock clock;
  PersistentStore store;
  GpioButtons buttons;
  U8g2Display display;
  NoBatteryMonitor battery;
  IrReceiverAdapter receiver1{board::kIrRx1, 0, clock};
#ifdef IRIS_ENABLE_RX2
  IrReceiverAdapter receiver2{board::kIrRx2, 1, clock};  // second TSOP, off unless wired and enabled
#endif
  iris::MultiReceiver receivers;
  IrTransmitterAdapter transmitter{board::kIrTx1, &receiver1};
  iris::RingLogSink log{clock, 100};

  iris::LibraryRepository repo{store};
  iris::RemoteLibrary library;
  iris::SettingsStore settingsStore{store};
  iris::Settings settings;
  iris::LearnSession learn{receivers, clock, &log};
  iris::TransmitService transmit{transmitter, &log};
  iris::ExplorationRun explore{transmit, clock, &log};
  iris::DeviceInfo info;
  iris::UiContext ctx;
  iris::ButtonDebouncer debouncer{buttons, clock};
  iris::UiApp ui;

  Device()
      : ctx(library, repo, settingsStore, settings, learn, transmit, explore, receivers, buttons, battery, log, clock,
            store, info),
        ui(ctx, display, iris::makeHomeScreen()) {
    receivers.add(receiver1);
#ifdef IRIS_ENABLE_RX2
    receivers.add(receiver2);
#endif
  }
};

Device* device = nullptr;
std::vector<iris::KeyEvent> events;

}  // namespace

void setup() {
  // First thing: the emitter gate low. (A hardware pull-down on the gate covers the time before this.)
  pinMode(board::kIrTx1, OUTPUT);
  digitalWrite(board::kIrTx1, LOW);

  Serial.begin(115200);
  delay(200);
  device = new Device();
  Device& d = *device;

  d.buttons.begin();
  char line[48];
  d.info.firmware = "0.6-dev";
  d.info.displayController = kIrisPanelName;
  d.info.irLibrary = "IRremoteESP8266 2.9.0";
  d.info.resetReason = resetReasonName();
  std::snprintf(line, sizeof line, "I2C SDA%u SCL%u", board::kI2cSda, board::kI2cScl);
  d.info.pinLines.push_back(line);
  std::snprintf(line, sizeof line, "RX %u/%u TX %u", board::kIrRx1, board::kIrRx2, board::kIrTx1);
  d.info.pinLines.push_back(line);
  std::snprintf(line, sizeof line, "Btn %u %u %u %u %u", board::kBtnUp, board::kBtnDown, board::kBtnLeft,
                board::kBtnRight, board::kBtnOk);
  d.info.pinLines.push_back(line);

  d.info.storage = "LittleFS";
  d.info.storageDegraded = [&d]() { return d.store.degraded(); };
  d.info.storageFormat = [&d]() { return d.store.format(); };
  d.info.storageUsage = [&d]() {
    if (d.store.degraded()) return std::string("RAM (sin flash)");
    char buf[32];
    std::snprintf(buf, sizeof buf, "Flash %uK/%uK", static_cast<unsigned>(d.store.usedBytes() / 1024),
                  static_cast<unsigned>(d.store.totalBytes() / 1024));
    return std::string(buf);
  };
  d.info.freeHeapBytes = []() { return static_cast<size_t>(ESP.getMaxAllocHeap()); };  // largest block: what a save needs
  d.info.adapterCounters = [&d]() {
    char buf[40];
    std::vector<std::string> lines;
    std::snprintf(buf, sizeof buf, "Heap %uK min %uK", static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  static_cast<unsigned>(ESP.getMinFreeHeap() / 1024));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "RX vistos:%u rep:%u", static_cast<unsigned>(d.receiver1.seen()),
                  static_cast<unsigned>(d.receiver1.droppedRepeat()));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "RX propio:%u", static_cast<unsigned>(d.receiver1.droppedOwnTx()));
    lines.push_back(buf);
    std::snprintf(buf, sizeof buf, "TX ok:%u canc:%u rech:%u", static_cast<unsigned>(d.transmitter.sent()),
                  static_cast<unsigned>(d.transmitter.cancelled()), static_cast<unsigned>(d.transmitter.refused()));
    lines.push_back(buf);
    return lines;
  };

  iris::Status txStatus = d.transmitter.begin();
  if (!txStatus.ok()) d.log.log(iris::LogLevel::Error, "boot", "tx: " + txStatus.message);
  iris::Status fsStatus = d.store.begin();
  if (!fsStatus.ok()) d.log.log(iris::LogLevel::Error, "boot", "storage: " + fsStatus.message);
  const iris::BootReport boot = iris::loadPersistentData(d.store, d.repo, d.library, d.settingsStore, d.settings, d.log);
  d.ctx.persistEnabled = boot.persistEnabled;
  if (!boot.notice.empty()) d.ctx.toast(boot.notice, 5000);

  iris::BootHistory history(d.store);  // diagnostic only: failures are ignored
  history.record(d.info.resetReason);
  d.info.bootLines = iris::BootHistory::format(history.load());

  iris::Status st = d.display.begin();
  Serial.printf("[iris] boot (%s), reset: %s, display: %s, storage: %s, tx: %s\n", d.info.firmware.c_str(),
                d.info.resetReason.c_str(), st.ok() ? "ok" : st.message.c_str(),
                fsStatus.ok() ? "ok" : fsStatus.message.c_str(), txStatus.ok() ? "ok" : txStatus.message.c_str());
  if (!st.ok()) d.ctx.displayHealthy = false;  // the UI keeps running and retries the panel
  d.ui.start();

  // If the main loop ever stops responding (for example a blocked bus), reset instead of freezing; the
  // reason shows up in the boot history. Subscribed last so a slow first boot cannot trigger it.
  esp_task_wdt_init(10, /*panic=*/true);
  esp_task_wdt_add(nullptr);
}

void loop() {
  Device& d = *device;
  events.clear();
  d.debouncer.poll(events);
  for (const iris::KeyEvent& e : events) d.ui.handleKey(e);
  d.ui.tick();
  esp_task_wdt_reset();
  delay(2);
}
