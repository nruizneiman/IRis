#include <doctest.h>

#include "FakeButtons.h"
#include "FakeClock.h"
#include "FakeDisplay.h"
#include "MemoryBlobStore.h"
#include <cstdio>

#include "app/Settings.h"
#include "persistence/Crc32.h"
#include "ui/ButtonDebouncer.h"
#include "ui/Painter.h"
#include "ui/Ui.h"
#include "ui/Utf8.h"

using namespace iris;
using namespace iris::testkit;

TEST_CASE("utf8 helpers count code points, never cut a character") {
  CHECK(utf8Length("Configuración") == 13);
  CHECK(utf8Length("ñandú") == 5);
  CHECK(utf8Truncate("Señal", 3) == "Señ");
  CHECK(utf8Truncate("Señal", 100) == "Señal");
  CHECK(utf8Truncate("ñññ", 2) == "ññ");
  CHECK(utf8Truncate("abc", 0).empty());
  CHECK(utf8Justify("Bat", "3.87 V", 21).size() == 21);
  CHECK(utf8Justify("Bat", "3.87 V", 21).substr(15) == "3.87 V");
  CHECK(utf8Length(utf8Justify("Una etiqueta larguísima", "OK", 12)) == 12);  // left is truncated first
  CHECK(utf8Justify("abc", "", 6) == "abc   ");
}

TEST_CASE("painter clips to the 21x8 grid and hashes content") {
  FakeDisplay d;
  Painter p(d);
  p.begin();
  p.text(0, 0, "123456789012345678901234567890");
  p.text(30, 1, "off grid");   // ignored
  p.text(0, 9, "off grid");    // ignored
  p.present();
  REQUIRE(d.shown().size() == 1);
  CHECK(utf8Length(d.shown()[0].text) == 21);
  const uint32_t h1 = p.hash();
  p.begin();
  p.text(0, 0, "123456789012345678901234567890");
  CHECK(p.hash() == h1);  // same picture, same hash
  p.begin();
  p.text(0, 0, "otra cosa");
  CHECK(p.hash() != h1);
  p.begin();
  p.item(2, "Etiqueta", "ON", true);
  p.present();
  CHECK(d.shown()[0].text.substr(0, 2) == "> ");
  CHECK(d.shown()[0].text.size() == 21);
}

namespace {
struct DebounceRig {
  FakeClock clock;
  FakeButtons buttons;
  ButtonDebouncer deb{buttons, clock};
  std::vector<KeyEvent> events;
  void run(uint32_t ms, uint32_t stepMs = 5) {
    for (uint32_t t = 0; t < ms; t += stepMs) {
      clock.advanceMs(stepMs);
      deb.poll(events);
    }
  }
  size_t count(Key k, KeyAction a) const {
    size_t n = 0;
    for (const auto& e : events) n += (e.key == k && e.action == a) ? 1 : 0;
    return n;
  }
};
}  // namespace

TEST_CASE("debouncer: contact bounce produces exactly one press, delivered on release") {
  DebounceRig r;
  r.run(50);
  // 20 ms of chatter: toggles every 5 ms, never stable for 30 ms
  for (int i = 0; i < 4; ++i) {
    r.buttons.set(Key::Ok, i % 2 == 0);
    r.run(5);
  }
  r.buttons.set(Key::Ok, false);
  r.run(100);
  CHECK(r.events.empty());  // glitch shorter than the debounce time is ignored

  r.buttons.set(Key::Ok, true);
  r.run(200);
  CHECK(r.events.empty());  // a short press is reported when the key is released
  r.buttons.set(Key::Ok, false);
  r.run(20);
  CHECK(r.events.empty());  // release itself must also be stable first
  r.run(30);
  CHECK(r.count(Key::Ok, KeyAction::Press) == 1);
  r.run(300);
  CHECK(r.events.size() == 1);

  // Chatter on release does not create extra presses.
  r.buttons.set(Key::Ok, true);
  r.run(100);
  for (int i = 0; i < 6; ++i) {
    r.buttons.set(Key::Ok, i % 2 == 1);
    r.run(5);
  }
  r.buttons.set(Key::Ok, false);
  r.run(100);
  CHECK(r.count(Key::Ok, KeyAction::Press) == 2);
}

TEST_CASE("debouncer: long press fires once while held and the release stays silent") {
  DebounceRig r;
  r.run(50);
  r.buttons.set(Key::Left, true);
  r.run(650);
  CHECK(r.events.empty());
  r.run(100);
  CHECK(r.count(Key::Left, KeyAction::LongPress) == 1);
  r.run(2000);
  CHECK(r.count(Key::Left, KeyAction::LongPress) == 1);
  CHECK(r.count(Key::Left, KeyAction::Repeat) == 0);
  r.buttons.set(Key::Left, false);
  r.run(100);
  CHECK(r.count(Key::Left, KeyAction::Press) == 0);  // a long press never also acts as a short press
  // pressing again after release works
  r.buttons.set(Key::Left, true);
  r.run(100);
  r.buttons.set(Key::Left, false);
  r.run(100);
  CHECK(r.count(Key::Left, KeyAction::Press) == 1);
}

TEST_CASE("debouncer: Up/Down act on touch and auto-repeat after the delay, never long-press") {
  DebounceRig r;
  r.run(50);
  r.buttons.set(Key::Down, true);
  r.run(100);
  CHECK(r.count(Key::Down, KeyAction::Press) == 1);  // immediate
  r.run(300);
  CHECK(r.count(Key::Down, KeyAction::Repeat) == 0);
  r.run(100);
  CHECK(r.count(Key::Down, KeyAction::Repeat) >= 1);
  r.run(1000);
  CHECK(r.count(Key::Down, KeyAction::Repeat) >= 6);
  CHECK(r.count(Key::Down, KeyAction::LongPress) == 0);
  CHECK(r.count(Key::Down, KeyAction::Press) == 1);
}

TEST_CASE("debouncer: a key held at startup is ignored until released") {
  FakeClock clock;
  FakeButtons buttons;
  buttons.set(Key::Ok, true);  // e.g. held while powering on
  ButtonDebouncer deb(buttons, clock);
  std::vector<KeyEvent> ev;
  for (int i = 0; i < 100; ++i) {
    clock.advanceMs(10);
    deb.poll(ev);
  }
  CHECK(ev.empty());
  buttons.set(Key::Ok, false);
  for (int i = 0; i < 10; ++i) { clock.advanceMs(10); deb.poll(ev); }
  buttons.set(Key::Ok, true);
  for (int i = 0; i < 10; ++i) { clock.advanceMs(10); deb.poll(ev); }
  buttons.set(Key::Ok, false);
  for (int i = 0; i < 10; ++i) { clock.advanceMs(10); deb.poll(ev); }
  CHECK(ev.size() == 1);  // a genuine press after the release is delivered
}

TEST_CASE("debouncer: simultaneous keys are independent and the clock may wrap") {
  DebounceRig r;
  r.clock.setUs((1ull << 32) * 1000ull - 200000ull);  // 200 ms before the ms counter wraps
  r.run(50);
  r.buttons.set(Key::Up, true);
  r.buttons.set(Key::Ok, true);
  r.run(400);  // crosses the wrap
  CHECK(r.count(Key::Up, KeyAction::Press) == 1);
  CHECK(r.count(Key::Ok, KeyAction::Press) == 0);  // still held: reported on release
  r.run(600);
  CHECK(r.count(Key::Ok, KeyAction::LongPress) == 1);
}

TEST_CASE("settings: round trip, defaults and corruption") {
  Settings s;
  s.invertDisplay = true;
  s.captureTimeoutSec = 30;
  auto back = deserializeSettings(serializeSettings(s));
  REQUIRE(back.ok());
  CHECK(back.value().invertDisplay);
  CHECK(back.value().captureTimeoutSec == 30);

  MemoryBlobStore store;
  SettingsStore ss(store);
  Status st;
  Settings def = ss.load(&st);  // nothing stored
  CHECK(st.ok());
  CHECK_FALSE(def.invertDisplay);
  CHECK(def.captureTimeoutSec == 15);

  REQUIRE(ss.save(s).ok());
  CHECK(ss.load().captureTimeoutSec == 30);

  store.flipByte(SettingsStore::key(), 25);
  Settings fallback = ss.load(&st);
  CHECK(st.code == Err::Corrupt);
  CHECK(fallback.captureTimeoutSec == 15);  // defaults, never garbage

  store.put(SettingsStore::key(), "garbage");
  CHECK(ss.load(&st).captureTimeoutSec == 15);
  CHECK_FALSE(st.ok());

  store.setFailReads(true);
  CHECK(ss.load(&st).captureTimeoutSec == 15);
  CHECK_FALSE(st.ok());
}

TEST_CASE("settings: out-of-range values fall back to defaults, unknown keys are ignored") {
  // Build a file with valid CRC but odd content.
  const std::string body = "invert=1\ntimeout=7\nfuture=9\n";
  char header[48];
  std::snprintf(header, sizeof header, "IRISCFG 1 %08lx %lu\n", static_cast<unsigned long>(crc32(body)),
                static_cast<unsigned long>(body.size()));
  auto r = deserializeSettings(std::string(header) + body);
  REQUIRE(r.ok());
  CHECK(r.value().invertDisplay);
  CHECK(r.value().captureTimeoutSec == 15);  // 7 is not an allowed timeout
  CHECK(deserializeSettings("IRISCFG 2 00000000 0\n").status().code == Err::UnsupportedVersion);
}

TEST_CASE("every user-facing error text fits the 21-column screen") {
  for (Err e : {Err::Ok, Err::InvalidArgument, Err::OutOfRange, Err::LimitReached, Err::NotFound, Err::AlreadyExists,
                Err::InUse, Err::Corrupt, Err::UnsupportedVersion, Err::StorageFailure, Err::Unavailable}) {
    const std::string text = spanishError(Status::fail(e, "x"));
    CHECK_MESSAGE(utf8Length(text) <= static_cast<size_t>(Painter::kCols), text);
    CHECK_FALSE(text.empty());
  }
}
