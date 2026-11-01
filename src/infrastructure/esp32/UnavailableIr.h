#pragma once

#include "hardware/IBatteryMonitor.h"
#include "hardware/IIrReceiver.h"
#include "hardware/IIrTransmitter.h"

// Honest placeholders until the IR adapters exist (Stage 5): every request fails with
// Err::Unavailable, so the UI reports "not available" instead of pretending to capture or send.
class UnavailableIrReceiver : public iris::IIrReceiver {
 public:
  iris::Status start() override { return iris::Status::fail(iris::Err::Unavailable, "IR receiver adapter not implemented"); }
  void stop() override {}
  bool running() const override { return false; }
  std::optional<iris::ReceivedCapture> poll() override { return std::nullopt; }
};

class UnavailableIrTransmitter : public iris::IIrTransmitter {
 public:
  iris::Status submit(const iris::TxJob&) override {
    return iris::Status::fail(iris::Err::Unavailable, "IR transmitter adapter not implemented");
  }
  void service() override {}
  void cancel() override {}
  iris::TxState state() const override { return iris::TxState::Idle; }
};

// No battery measurement circuit is built yet: report "not available", never a made-up value.
class NoBatteryMonitor : public iris::IBatteryMonitor {
 public:
  std::optional<uint16_t> readMillivolts() override { return std::nullopt; }
};
