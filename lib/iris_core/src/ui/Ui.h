// UI framework: screens, navigation commands, shared context and the app loop.
// Screens only talk to app services through UiContext; they never touch hardware directly
// (the raw button levels exposed for the button test screen are the one read-only exception).
#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/ExplorationRun.h"
#include "app/LearnSession.h"
#include "app/RemoteLibrary.h"
#include "app/SequenceRunner.h"
#include "app/Settings.h"
#include "app/TransmitService.h"
#include "diagnostics/RingLogSink.h"
#include "hardware/IBatteryMonitor.h"
#include "hardware/IBlobStore.h"
#include "hardware/IButtonInput.h"
#include "hardware/IFileSink.h"
#include "hardware/IFileSource.h"
#include "hardware/IDisplay.h"
#include "hardware/IIrReceiver.h"
#include "persistence/LibraryRepository.h"
#include "ui/ButtonDebouncer.h"
#include "ui/Painter.h"

namespace iris {

class Screen;

// What a screen asks the app to do after handling a key.
struct Nav {
  enum class Kind : uint8_t { None, Push, Pop, Replace, HomeThen };
  Kind kind = Kind::None;
  std::unique_ptr<Screen> screen;  // Push / Replace / HomeThen
  int count = 1;                   // Pop

  static Nav none() { return Nav{}; }
  static Nav push(std::unique_ptr<Screen> s);
  static Nav replace(std::unique_ptr<Screen> s);
  static Nav pop(int n = 1);
  static Nav home();                                  // back to the first screen
  static Nav homeThen(std::unique_ptr<Screen> s);     // back to the first screen, then open `s`
};

// Facts the firmware (or simulator) knows and the core does not.
struct DeviceInfo {
  std::string firmware = "dev";
  std::string displayController = "desconocido";
  std::string irLibrary = "sin biblioteca IR";
  std::string storage = "desconocido";
  std::string resetReason = "n/d";
  std::vector<std::string> pinLines;  // human-readable wiring summary

  // Optional hooks the firmware fills in; all may be empty (e.g. in the simulator).
  std::function<std::string()> storageUsage;                 // e.g. "Flash 12/1408 KB"
  std::function<bool()> storageDegraded;                     // true when flash could not be mounted
  std::function<Status()> storageFormat;                     // user-confirmed erase + remount
  std::function<std::vector<std::string>()> adapterCounters; // extra diagnostics lines (RX/TX counters)
  std::function<size_t()> freeHeapBytes;                      // largest block available for a save (heap guard)
  std::vector<std::string> bootLines;                         // recent boots, newest first ("#12 encendido")
};

struct CaptureRecord {
  uint32_t timeMs = 0;
  IrSignal signal;
};

struct UiContext {
  UiContext(RemoteLibrary& library_, LibraryRepository& repo_, SettingsStore& settingsStore_, Settings& settings_,
            LearnSession& learn_, TransmitService& transmit_, ExplorationRun& explore_, IIrReceiver& receiver_,
            const IButtonInput& rawButtons_, IBatteryMonitor& battery_, RingLogSink& log_, const IClock& clock_,
            IBlobStore& store_, const DeviceInfo& info_)
      : library(library_), repo(repo_), settingsStore(settingsStore_), settings(settings_), learn(learn_),
        transmit(transmit_), explore(explore_), receiver(receiver_), rawButtons(rawButtons_), battery(battery_),
        log(log_), clock(clock_), store(store_), info(info_), sequence(transmit_, clock_, &log_) {}

  RemoteLibrary& library;
  LibraryRepository& repo;
  SettingsStore& settingsStore;
  Settings& settings;
  LearnSession& learn;
  TransmitService& transmit;
  ExplorationRun& explore;
  IIrReceiver& receiver;
  const IButtonInput& rawButtons;
  IBatteryMonitor& battery;
  RingLogSink& log;
  const IClock& clock;
  IBlobStore& store;
  const DeviceInfo& info;
  SequenceRunner sequence;  // loop, crazy and Be Gone modes; serviced on every tick whatever screen is up

  IFileSource* files = nullptr;  // removable card with Flipper-IRDB files; null when the build has no card support
  IFileSink* sink = nullptr;     // the same card for exporting; null when the build cannot write to it
  bool displayHealthy = true;
  // False when the stored data could not be read for a reason other than damage (read error, low memory,
  // newer firmware): saving would overwrite data we never saw. The UI keeps working in memory only.
  bool persistEnabled = true;
  std::deque<CaptureRecord> captures;  // newest first, bounded
  static constexpr size_t kMaxCaptures = 8;

  void toast(std::string text, uint32_t ms = 2000);
  const std::string* activeToast() const;
  Status persist();          // saves the library; marks it clean on success
  Status persistSettings();  // saves the preferences

 private:
  std::string toastText_;
  uint32_t toastUntilMs_ = 0;
  bool toastActive_ = false;
};

class Screen {
 public:
  virtual ~Screen() = default;
  virtual const char* id() const = 0;                    // stable name (tests, logs)
  virtual void onEnter(UiContext&) {}                    // when pushed
  virtual void onResume(UiContext&) {}                   // when it becomes the top again
  virtual void onExit(UiContext&) {}                     // when popped or replaced
  virtual Nav onKey(UiContext&, const KeyEvent&) = 0;
  virtual void onTick(UiContext&) {}
  virtual void render(UiContext&, Painter&) = 0;
};

// Spanish user-facing text for an error (developer messages stay in English).
std::string spanishError(const Status& status);

class UiApp {
 public:
  UiApp(UiContext& ctx, IDisplay& display, std::unique_ptr<Screen> home);

  void start();                     // shows the home screen
  void handleKey(const KeyEvent&);
  // One main-loop iteration: services (learn / transmit / exploration), top screen tick, redraw.
  void tick();

  size_t depth() const { return stack_.size(); }
  Screen* top() { return stack_.empty() ? nullptr : stack_.back().get(); }
  UiContext& context() { return ctx_; }
  uint32_t flushCount() const { return flushCount_; }

 private:
  void apply(Nav nav);
  void renderIfNeeded(bool force);

  UiContext& ctx_;
  IDisplay& display_;
  Painter painter_;
  std::unique_ptr<Screen> home_;
  std::vector<std::unique_ptr<Screen>> stack_;
  uint32_t lastHash_ = 0;
  bool haveHash_ = false;
  bool dirty_ = true;
  uint32_t lastRenderMs_ = 0;
  uint32_t lastDisplayRetryMs_ = 0;
  uint32_t flushCount_ = 0;
  LearnState prevLearn_ = LearnState::Idle;
  TxState prevTx_ = TxState::Idle;
  ExplorationState prevExplore_ = ExplorationState::Idle;
  uint32_t prevExploreSent_ = 0;
  SequenceState prevSeq_ = SequenceState::Idle;
  uint32_t prevSeqSent_ = 0;
  bool displayErrorLogged_ = false;
};

}  // namespace iris
