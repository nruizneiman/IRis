// Lightweight error handling shared by every layer. No exceptions (ESP32 builds may disable them).
#pragma once

#include <string>
#include <utility>

namespace iris {

enum class Err {
  Ok = 0,
  InvalidArgument,
  OutOfRange,
  LimitReached,
  NotFound,
  AlreadyExists,
  InUse,
  Corrupt,
  UnsupportedVersion,
  StorageFailure,
  Unavailable,
};

const char* errName(Err code);

struct Status {
  Err code = Err::Ok;
  std::string message;

  bool ok() const { return code == Err::Ok; }
  explicit operator bool() const { return ok(); }

  static Status success() { return Status{}; }
  static Status fail(Err c, std::string msg) { return Status{c, std::move(msg)}; }
};

// T must be default constructible; the value is only meaningful when ok().
template <class T>
class Result {
 public:
  Result(T value) : value_(std::move(value)) {}  // NOLINT: implicit by design
  Result(Status status) : status_(std::move(status)) {}  // NOLINT: implicit by design

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  Status status_;
  T value_{};
};

}  // namespace iris
