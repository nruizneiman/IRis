#pragma once

#include <Arduino.h>
#include <IRsend.h>

#include <atomic>
#include <vector>

#include "domain/Limits.h"
#include "hardware/IIrTransmitter.h"
#include "infrastructure/esp32/IrReceiverAdapter.h"
#include "ir/IrCodec.h"

// Software-PWM emitter on one GPIO driving a logic-level MOSFET gate (never an LED directly).
//
// IRremoteESP8266 sends are blocking, so transmissions run in a dedicated FreeRTOS task on core 0
// (the Arduino loop and the UI stay on core 1). submit() returns immediately. cancel() takes effect
// between library calls: a frame (or a Sony-style burst of copies) already on the air is finished.
//
// Safety limits below are PROVISIONAL and independent of what the caller asks for. They exist so
// a bug cannot hold the emitter on; the right numbers depend on the LED, resistor and MOSFET stage
// that have not been identified yet (see docs/firmware/ir.md and docs/hardware/bring-up.md).
struct TxSafety {
  uint32_t maxMarkUs = 25000;       // longest single carrier burst accepted (NEC leader is ~9 ms)
  uint32_t maxSequenceMs = 3000;    // estimated on-air time of one job
  uint32_t idleAfterMs = 5;         // forced quiet time after each job
  uint32_t decodedFrameEstimateMs = 120;  // upper estimate for a library-generated frame
};

class IrTransmitterAdapter : public iris::IIrTransmitter {
 public:
  IrTransmitterAdapter(uint8_t pin, IrReceiverAdapter* muteReceiver = nullptr, TxSafety safety = {})
      : pin_(pin), irsend_(pin), mute_(muteReceiver), safety_(safety) {}

  // Drives the pin low and starts the worker task. Nothing is ever transmitted by begin().
  iris::Status begin() {
    irsend_.begin();  // pinMode(OUTPUT) + LED off
    digitalWrite(pin_, LOW);
    if (task_) return iris::Status::success();
    const BaseType_t ok = xTaskCreatePinnedToCore(&IrTransmitterAdapter::trampoline, "ir_tx", 6144, this, 3, &task_, 0);
    if (ok != pdPASS) {
      task_ = nullptr;
      state_ = iris::TxState::Failed;
      return iris::Status::fail(iris::Err::Unavailable, "cannot start IR transmit task");
    }
    return iris::Status::success();
  }

  iris::Status submit(const iris::TxJob& job) override {
    if (!task_) return iris::Status::fail(iris::Err::Unavailable, "transmitter not started");
    const iris::TxState s = state_;
    if (s == iris::TxState::Sending || s == iris::TxState::Cancelling) {
      return iris::Status::fail(iris::Err::Unavailable, "transmitter busy");
    }
    if ((job.channelMask & 0x01) == 0) {
      ++refused_;
      return iris::Status::fail(iris::Err::InvalidArgument, "only emitter channel 0 exists");
    }
    iris::Status st = prepare(job);
    if (!st.ok()) {
      ++refused_;
      return st;
    }
    cancel_ = false;
    state_ = iris::TxState::Sending;
    xTaskNotifyGive(task_);
    return iris::Status::success();
  }

  void service() override {}  // state is driven by the task

  void cancel() override {
    if (state_ == iris::TxState::Sending) {
      cancel_ = true;
      state_ = iris::TxState::Cancelling;
    }
  }

  iris::TxState state() const override { return state_; }

  uint32_t sent() const { return sent_; }
  uint32_t cancelled() const { return cancelled_; }
  uint32_t refused() const { return refused_; }

 private:
  struct Step {
    bool raw = false;
    iris::LibFrame lib;
    std::vector<uint16_t> durations;
    uint16_t hz = 38000;
    uint32_t gapAfterUs = 0;
  };

  // Validates against the safety limits and converts everything the task needs, so the task itself
  // cannot fail halfway for a reason that was knowable up front.
  iris::Status prepare(const iris::TxJob& job) {
    steps_.clear();
    uint64_t estimateUs = 0;
    auto addFrame = [&](const iris::Frame& f) -> iris::Status {
      Step st;
      st.gapAfterUs = job.signal.repeatGapUs;
      if (f.kind == iris::Frame::Kind::Raw) {
        st.raw = true;
        st.durations = f.raw.durationsUs;
        st.hz = static_cast<uint16_t>(f.raw.carrierHz ? f.raw.carrierHz : job.defaultCarrierHz);
        for (size_t i = 0; i < st.durations.size(); ++i) {
          if ((i & 1) == 0 && st.durations[i] > safety_.maxMarkUs) {
            return iris::Status::fail(iris::Err::OutOfRange, "a mark exceeds the emitter safety limit");
          }
          estimateUs += st.durations[i];
        }
      } else {
        auto lf = iris::toLibraryFrame(irsend_, f.decoded);
        if (!lf) return iris::Status::fail(iris::Err::InvalidArgument, "frame has no library encoding");
        st.lib = *lf;
        estimateUs += static_cast<uint64_t>(safety_.decodedFrameEstimateMs) * 1000u * (1u + lf->repeat);
      }
      estimateUs += st.gapAfterUs;
      steps_.push_back(std::move(st));
      return iris::Status::success();
    };
    for (const iris::Frame& f : job.signal.initial) {
      iris::Status st = addFrame(f);
      if (!st.ok()) return st;
    }
    for (uint8_t r = 0; r < job.signal.repeatCount; ++r) {
      for (const iris::Frame& f : job.signal.repeat) {
        iris::Status st = addFrame(f);
        if (!st.ok()) return st;
      }
    }
    if (steps_.empty()) return iris::Status::fail(iris::Err::InvalidArgument, "nothing to send");
    if (estimateUs / 1000u > safety_.maxSequenceMs) {
      return iris::Status::fail(iris::Err::OutOfRange, "sequence longer than the safety limit");
    }
    return iris::Status::success();
  }

  static void trampoline(void* self) { static_cast<IrTransmitterAdapter*>(self)->run(); }

  // Waits up to `us`, waking early on cancel. Returns false if cancelled.
  bool pause(uint32_t us) {
    while (us > 0) {
      if (cancel_) return false;
      const uint32_t chunk = us > 2000 ? 2000 : us;
      if (chunk >= 1000) vTaskDelay(1);  // let the idle task run
      else delayMicroseconds(chunk);
      us -= chunk;
    }
    return !cancel_;
  }

  void run() {
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      if (mute_) mute_->suspend(true);
      bool aborted = false;
      for (size_t i = 0; i < steps_.size(); ++i) {
        if (cancel_) {
          aborted = true;
          break;
        }
        const Step& s = steps_[i];
        if (s.raw) irsend_.sendRaw(s.durations.data(), static_cast<uint16_t>(s.durations.size()), s.hz);
        else irsend_.send(s.lib.type, s.lib.data, s.lib.nbits, s.lib.repeat);
        if (i + 1 < steps_.size() && s.gapAfterUs && !pause(s.gapAfterUs)) {
          aborted = true;
          break;
        }
      }
      digitalWrite(pin_, LOW);  // the library leaves the LED off after each mark; make sure the gate ends low
      vTaskDelay(pdMS_TO_TICKS(safety_.idleAfterMs ? safety_.idleAfterMs : 1));
      if (mute_) mute_->suspend(false);
      if (aborted) ++cancelled_;
      else ++sent_;
      state_ = iris::TxState::Idle;
    }
  }

  uint8_t pin_;
  IRsend irsend_;
  IrReceiverAdapter* mute_;
  TxSafety safety_;
  TaskHandle_t task_ = nullptr;
  std::vector<Step> steps_;
  std::atomic<iris::TxState> state_{iris::TxState::Idle};
  std::atomic<bool> cancel_{false};
  std::atomic<uint32_t> sent_{0}, cancelled_{0}, refused_{0};
};
