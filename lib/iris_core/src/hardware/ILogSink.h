#pragma once

#include <cstdint>
#include <string>

namespace iris {

enum class LogLevel : uint8_t { Debug, Info, Warn, Error };

class ILogSink {
 public:
  virtual ~ILogSink() = default;
  virtual void log(LogLevel level, const char* tag, const std::string& message) = 0;
};

}  // namespace iris
