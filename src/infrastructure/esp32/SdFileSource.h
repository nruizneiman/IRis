#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <string>
#include <vector>

#include "board_pins.h"
#include "hardware/IFileSource.h"

// Read-only access to the microSD module (SPI on its own pin set, see board_pins.h). The card is mounted on first use
// and, when there is none, retried at most every few seconds so an absent card never slows the UI down. Nothing is ever
// written here: the files are copied to the card with tools/irdb_upload.py (bench firmware) or on a PC.
class SdFileSource : public iris::IFileSource {
 public:
  bool available() override {
    if (mounted_) return true;
    const uint32_t now = millis();
    if (everTried_ && now - lastTryMs_ < kRetryMs) return false;
    everTried_ = true;
    lastTryMs_ = now;
    spi_.begin(board::kSdSck, board::kSdMiso, board::kSdMosi, board::kSdCs);
    mounted_ = SD.begin(board::kSdCs, spi_, 4000000);
    return mounted_;
  }

  iris::Result<std::vector<iris::FileEntry>> list(const std::string& dir, size_t maxEntries) override {
    if (!available()) return iris::Status::fail(iris::Err::Unavailable, "no card");
    File d = SD.open(dir.c_str());
    if (!d || !d.isDirectory()) return iris::Status::fail(iris::Err::NotFound, "not a folder");
    std::vector<iris::FileEntry> out;
    for (File e = d.openNextFile(); e && out.size() < maxEntries; e = d.openNextFile()) {
      iris::FileEntry fe;
      fe.name = e.name();
      fe.isDir = e.isDirectory();
      fe.size = static_cast<uint32_t>(e.size());
      out.push_back(std::move(fe));
    }
    return out;
  }

  iris::Result<std::string> readText(const std::string& path, size_t maxBytes) override {
    if (!available()) return iris::Status::fail(iris::Err::Unavailable, "no card");
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f || f.isDirectory()) return iris::Status::fail(iris::Err::NotFound, "no such file");
    if (static_cast<size_t>(f.size()) > maxBytes) return iris::Status::fail(iris::Err::OutOfRange, "file too large");
    std::string text;
    text.resize(f.size());
    const size_t got = f.read(reinterpret_cast<uint8_t*>(&text[0]), text.size());
    if (got != text.size()) return iris::Status::fail(iris::Err::StorageFailure, "short read");
    return text;
  }

 private:
  static constexpr uint32_t kRetryMs = 3000;
  SPIClass spi_{VSPI};
  bool mounted_ = false;
  bool everTried_ = false;
  uint32_t lastTryMs_ = 0;
};
