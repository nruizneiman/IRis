#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <string>
#include <vector>

#include "board_pins.h"
#include "hardware/IFileSink.h"
#include "hardware/IFileSource.h"

// The microSD module (SPI on its own pin set, see board_pins.h). The card is mounted on first use and, when there is
// none, retried at most every few seconds so an absent card never slows the UI down. Reading serves the IRDB files
// (copied with tools/irdb_upload.py or on a PC); writing happens only when the user exports from the menus.
class SdFileSource : public iris::IFileSource, public iris::IFileSink {
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

  bool writable() override { return available(); }

  // Writes to "<path>.tmp" and renames it over the target, so a card pulled half-way never leaves a broken file.
  iris::Status writeText(const std::string& path, const std::string& text) override {
    if (!available()) return iris::Status::fail(iris::Err::Unavailable, "no card");
    for (size_t slash = path.find('/', 1); slash != std::string::npos; slash = path.find('/', slash + 1)) {
      const std::string dir = path.substr(0, slash);
      if (!SD.exists(dir.c_str()) && !SD.mkdir(dir.c_str())) return iris::Status::fail(iris::Err::StorageFailure, "mkdir failed");
    }
    const std::string tmp = path + ".tmp";
    SD.remove(tmp.c_str());
    File f = SD.open(tmp.c_str(), FILE_WRITE);
    if (!f) return iris::Status::fail(iris::Err::StorageFailure, "cannot create the file");
    const size_t wrote = f.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    f.close();
    if (wrote != text.size()) {
      SD.remove(tmp.c_str());
      return iris::Status::fail(iris::Err::StorageFailure, "short write");
    }
    if (SD.exists(path.c_str())) SD.remove(path.c_str());
    if (!SD.rename(tmp.c_str(), path.c_str())) return iris::Status::fail(iris::Err::StorageFailure, "rename failed");
    return iris::Status::success();
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
