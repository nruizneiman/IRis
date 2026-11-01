// Bounded in-memory event log: usable on the device (diagnostics screen) and in tests.
#pragma once

#include <deque>
#include <string>

#include "hardware/IClock.h"
#include "hardware/ILogSink.h"

namespace iris {

struct LogEntry {
  uint32_t timeMs = 0;
  LogLevel level = LogLevel::Info;
  std::string tag;
  std::string message;
};

class RingLogSink : public ILogSink {
 public:
  RingLogSink(const IClock& clock, size_t capacity) : clock_(clock), capacity_(capacity ? capacity : 1) {}

  void log(LogLevel level, const char* tag, const std::string& message) override {
    if (entries_.size() == capacity_) entries_.pop_front();
    entries_.push_back(LogEntry{clock_.nowMs(), level, tag ? tag : "", message});
  }

  const std::deque<LogEntry>& entries() const { return entries_; }
  size_t size() const { return entries_.size(); }
  bool contains(const std::string& needle) const {
    for (const auto& e : entries_) {
      if (e.message.find(needle) != std::string::npos) return true;
    }
    return false;
  }

 private:
  const IClock& clock_;
  size_t capacity_;
  std::deque<LogEntry> entries_;
};

}  // namespace iris
