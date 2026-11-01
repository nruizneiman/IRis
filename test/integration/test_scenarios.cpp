// Scripted end-to-end flows through the console simulator. These exercise the real app services
// against FAKE hardware: they prove the software flow, not electrical or optical behaviour.
#include <doctest.h>

#include "Simulator.h"

using iris::sim::Simulator;

namespace {
bool has(const std::string& text, const std::string& needle) { return text.find(needle) != std::string::npos; }
}  // namespace

TEST_CASE("scenario: learn a recognised NEC signal, save, reboot, send") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("remote add 1 Sala");
  sim.execute("button add 2 Power");
  CHECK(has(sim.execute("learn 5000"), "waiting"));
  sim.execute("inject nec 0 8");
  const std::string shown = sim.execute("learn-show");
  CHECK(has(shown, "recognized"));
  CHECK(has(shown, "NEC"));
  CHECK(has(shown, "original RAW kept"));
  CHECK(has(sim.execute("learn-save 2 3"), "saved"));
  CHECK(has(sim.execute("status"), "UNSAVED"));

  CHECK(has(sim.execute("save"), "saved"));
  CHECK(has(sim.execute("reboot"), "primary file"));
  CHECK(has(sim.execute("list"), "Power"));

  CHECK(has(sim.execute("send 2 3"), "sending"));
  sim.execute("advance 500");
  CHECK(has(sim.execute("txlog"), "completed"));
}

TEST_CASE("scenario: unsaved changes are lost on reboot, saved ones survive") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("save");
  sim.execute("cat add Audio");  // never saved
  CHECK(has(sim.execute("reboot"), "primary file"));
  const std::string list = sim.execute("list");
  CHECK(has(list, "TV"));
  CHECK_FALSE(has(list, "Audio"));
}

TEST_CASE("scenario: unknown signal is stored as RAW, empty capture is rejected") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("remote add 1 Sala");
  sim.execute("button add 2 Raro");
  sim.execute("learn 5000");
  sim.execute("inject unknown");
  CHECK(has(sim.execute("learn-show"), "unknown"));
  CHECK(has(sim.execute("learn-save 2 3"), "saved"));

  sim.execute("learn 5000");
  sim.execute("inject empty");
  CHECK(has(sim.execute("learn-show"), "rejected"));
  CHECK(has(sim.execute("learn-save 2 3"), "ERROR nothing captured"));
}

TEST_CASE("scenario: learning times out and can be cancelled") {
  Simulator sim;
  sim.execute("learn 1000");
  CHECK(has(sim.execute("advance 1100"), "learn=timed-out"));
  sim.execute("learn 1000");
  sim.execute("learn-cancel");
  CHECK(has(sim.execute("status"), "learn=idle"));
}

TEST_CASE("scenario: manual signals are validated before they are stored") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("remote add 1 Sala");
  sim.execute("button add 2 Power");
  CHECK(has(sim.execute("manual 2 3 nec 0 8 32"), "stored"));
  CHECK(has(sim.execute("manual 2 3 nec 0 300 32"), "ERROR OutOfRange"));
  CHECK(has(sim.execute("manual 2 3 sony 0 8 99"), "ERROR OutOfRange"));
  CHECK(has(sim.execute("manual 2 3 betamax 0 8 32"), "unknown protocol"));
  CHECK(has(sim.execute("manual-raw 2 3 38000 9000 4500 560 560"), "stored"));
  CHECK(has(sim.execute("manual-raw 2 3 100 9000 4500"), "ERROR OutOfRange"));
  CHECK(has(sim.execute("manual-raw 2 3 0 9000 0"), "ERROR InvalidArgument"));
  CHECK(has(sim.execute("list"), "raw[4]"));
}

TEST_CASE("scenario: hardware errors are reported and recoverable") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("remote add 1 Sala");
  sim.execute("button add 2 Power");
  sim.execute("manual 2 3 nec 0 8 32");

  sim.execute("fault rx-start on");
  CHECK(has(sim.execute("learn 1000"), "ERROR Unavailable"));
  CHECK(has(sim.execute("status"), "learn=failed"));
  sim.execute("fault rx-start off");
  CHECK(has(sim.execute("learn 1000"), "waiting"));
  sim.execute("learn-cancel");

  sim.execute("fault tx-submit on");
  CHECK(has(sim.execute("send 2 3"), "ERROR Unavailable"));
  CHECK(has(sim.execute("status"), "tx=FAILED"));
  CHECK(has(sim.execute("send 2 3"), "sending"));  // retry from Failed works

  sim.execute("fault write on");
  CHECK(has(sim.execute("save"), "ERROR StorageFailure"));
  CHECK(has(sim.execute("status"), "UNSAVED"));  // failed save must not clear the dirty flag
  sim.execute("fault write off");
  CHECK(has(sim.execute("save"), "saved"));
}

TEST_CASE("scenario: corrupted primary file recovers from backup") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("save");
  sim.execute("cat add Audio");
  sim.execute("save");
  sim.execute("corrupt");
  const std::string boot = sim.execute("reboot");
  CHECK(has(boot, "BACKUP"));
  CHECK(has(sim.execute("list"), "TV"));
}

TEST_CASE("scenario: both files corrupt keeps files untouched and library not replaced") {
  Simulator sim;
  sim.flash().put("library.irs", "junk");
  sim.flash().put("library.prev", "junk");
  sim.execute("cat add Mine");
  const std::string boot = sim.execute("reboot");
  CHECK(has(boot, "NOT replaced"));
  CHECK(has(sim.execute("list"), "Mine"));  // in-memory work is not thrown away
}

TEST_CASE("scenario: cancel stops a long transmission between frames") {
  Simulator sim;
  sim.execute("cat add TV");
  sim.execute("remote add 1 Sala");
  sim.execute("button add 2 Power");
  // Two raw frames' worth of data in one frame is not a repeat sequence; use a long raw frame.
  sim.execute("manual-raw 2 3 38000 9000 4500 560 560 560 1690 560 20000");
  sim.execute("send 2 3");
  sim.execute("advance 5");
  CHECK(has(sim.execute("cancel"), "between frames"));
  sim.execute("advance 100");
  const std::string log = sim.execute("txlog");
  CHECK(has(log, "CANCELLED"));
  CHECK(has(sim.execute("status"), "tx=idle"));
}

TEST_CASE("scenario: exploration respects confirmation, stop and limits") {
  Simulator sim;
  // Hard cap
  CHECK(has(sim.execute("explore nec 32 0 0 0 255 5000 500"), "ERROR LimitReached"));
  CHECK(has(sim.execute("explore nec 32 0 0 0 255 100 10"), "ERROR OutOfRange"));  // gap too short
  CHECK(has(sim.execute("explore nec 32 0 0 0 255 100 500"), "explore-confirm"));
  CHECK(has(sim.execute("explore-start"), "confirmation required"));
  CHECK(has(sim.execute("explore-confirm"), "confirmed"));
  CHECK(has(sim.execute("explore-start"), "running"));
  sim.execute("advance 2000");
  CHECK(has(sim.execute("explore-status"), "running"));
  sim.execute("explore-stop");
  const std::string after = sim.execute("explore-status");
  CHECK(has(after, "stopped"));
  sim.execute("advance 3000");
  CHECK(sim.execute("explore-status") == after);  // nothing sent after stop
  CHECK(has(sim.execute("explore-save"), "log saved"));
  CHECK(sim.flash().exists("explore.log"));
}

TEST_CASE("scenario: small exploration completes on its own") {
  Simulator sim;
  CHECK(has(sim.execute("explore sony 12 1 1 0 2 10 300"), "ready"));
  sim.execute("explore-start");
  sim.execute("advance 3000");
  CHECK(has(sim.execute("explore-status"), "completed 3/3"));
}

TEST_CASE("scenario: battery is reported as not available when unmeasured") {
  Simulator sim;
  CHECK(has(sim.execute("status"), "battery=not available"));
}

TEST_CASE("scenario: bad input never crashes the interpreter") {
  Simulator sim;
  for (const char* line : {"", "   ", "nonsense", "cat", "cat add", "remote add x y", "button add 999 X", "send 1 1",
                           "send a b", "advance abc", "advance 99999999", "inject", "inject nec 999 1", "learn",
                           "learn abc", "manual 1", "manual-raw 1 1", "explore", "explore nec", "fault", "fault zzz on",
                           "learn-save", "learn-save 1 1"}) {
    CHECK_NOTHROW(sim.execute(line));
  }
  CHECK(has(sim.execute("nonsense"), "unknown command"));
}
