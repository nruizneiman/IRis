// Console simulator: drives the REAL app services (library, learn, transmit, exploration,
// persistence) against fake hardware. It is a command interpreter, not a UI: screens and
// button navigation arrive in Stage 4. Everything it shows is simulated, not measured.
#pragma once

#include <string>
#include <vector>

#include "UiRig.h"
#include "app/ExplorationRun.h"
#include "app/LearnSession.h"
#include "app/RemoteLibrary.h"
#include "app/TransmitService.h"
#include "diagnostics/RingLogSink.h"
#include "persistence/LibraryRepository.h"

namespace iris::sim {

class Simulator {
 public:
  Simulator();

  // Runs one command line and returns the text to print (may be multi-line, no trailing newline).
  std::string execute(const std::string& line);

  // Direct access for tests that want to assert on state instead of text.
  testkit::UiRig& rig() { return rig_; }
  RemoteLibrary& library() { return library_; }
  testkit::MemoryBlobStore& flash() { return flash_; }
  testkit::FakeIrTransmitter& transmitter() { return tx_; }
  testkit::FakeClock& clock() { return clock_; }
  LearnSession& learn() { return learn_; }
  ExplorationRun& exploration() { return explore_; }
  RingLogSink& log() { return log_; }

 private:
  void step();  // one main-loop iteration: learn, transmit, exploration
  std::string help() const;
  std::string status() const;
  std::string list() const;
  std::string cmdAdvance(const std::vector<std::string>& a);
  std::string cmdInject(const std::vector<std::string>& a);
  std::string cmdLearnSave(const std::vector<std::string>& a);
  std::string cmdManual(const std::vector<std::string>& a);
  std::string cmdManualRaw(const std::vector<std::string>& a);
  std::string cmdSend(const std::vector<std::string>& a);
  std::string cmdExplore(const std::vector<std::string>& a);
  std::string cmdFault(const std::vector<std::string>& a);
  std::string cmdReboot();
  std::string cmdKey(const std::vector<std::string>& a);

  testkit::UiRig rig_;  // fake hardware + real services + real UI (shared with the UI tests)
  testkit::FakeClock& clock_ = rig_.clock;
  testkit::MemoryBlobStore& flash_ = rig_.flash;
  testkit::FakeIrReceiver& rx_ = rig_.rx;
  testkit::FakeIrTransmitter& tx_ = rig_.tx;
  testkit::FakeBattery& battery_ = rig_.battery;
  RingLogSink& log_ = rig_.log;
  LibraryRepository& repo_ = rig_.repo;
  RemoteLibrary& library_ = rig_.library;
  LearnSession& learn_ = rig_.learn;
  TransmitService& transmit_ = rig_.transmit;
  ExplorationRun& explore_ = rig_.explore;
};

}  // namespace iris::sim
