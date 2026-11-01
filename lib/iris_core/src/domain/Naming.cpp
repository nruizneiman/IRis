#include "domain/Naming.h"

#include "domain/Limits.h"

namespace iris {

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool validUtf8(std::string_view s) {
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t extra;
    if (c < 0x80) extra = 0;
    else if (c >= 0xC2 && c <= 0xDF) extra = 1;
    else if (c >= 0xE0 && c <= 0xEF) extra = 2;
    else if (c >= 0xF0 && c <= 0xF4) extra = 3;
    else return false;
    for (size_t k = 1; k <= extra; ++k) {
      if (i + k >= s.size() || (static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    }
    i += extra + 1;
  }
  return true;
}

}  // namespace

std::string trimName(std::string_view name) {
  size_t b = 0, e = name.size();
  while (b < e && isSpace(name[b])) ++b;
  while (e > b && isSpace(name[e - 1])) --e;
  return std::string(name.substr(b, e - b));
}

Status validateName(std::string_view name) {
  if (name.empty()) return Status::fail(Err::InvalidArgument, "name is empty");
  if (name.size() > limits::kMaxNameBytes) return Status::fail(Err::OutOfRange, "name too long");
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || u == 0x7F) return Status::fail(Err::InvalidArgument, "name has control characters");
  }
  if (!validUtf8(name)) return Status::fail(Err::InvalidArgument, "name is not valid UTF-8");
  return Status::success();
}

bool namesEqual(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

}  // namespace iris
