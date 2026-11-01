#include "ui/Ui.h"

#include <algorithm>

#include "persistence/LibrarySerializer.h"

namespace iris {

// ---------- Nav ----------

Nav Nav::push(std::unique_ptr<Screen> s) { Nav n; n.kind = Kind::Push; n.screen = std::move(s); return n; }
Nav Nav::replace(std::unique_ptr<Screen> s) { Nav n; n.kind = Kind::Replace; n.screen = std::move(s); return n; }
Nav Nav::pop(int c) { Nav n; n.kind = Kind::Pop; n.count = c < 1 ? 1 : c; return n; }
Nav Nav::home() { return homeThen(nullptr); }
Nav Nav::homeThen(std::unique_ptr<Screen> s) { Nav n; n.kind = Kind::HomeThen; n.screen = std::move(s); return n; }

// ---------- UiContext ----------

void UiContext::toast(std::string text, uint32_t ms) {
  toastText_ = std::move(text);
  toastUntilMs_ = clock.nowMs() + ms;
  toastActive_ = true;
}

const std::string* UiContext::activeToast() const {
  if (!toastActive_) return nullptr;
  if (static_cast<int32_t>(clock.nowMs() - toastUntilMs_) >= 0) return nullptr;
  return &toastText_;
}

Status UiContext::persist() {
  if (!persistEnabled) {
    return Status::fail(Err::Unavailable, "saving is disabled: stored data was not loaded");
  }
  if (info.freeHeapBytes) {
    // Serializing one remote needs its text plus the JSON document (about 2x the text); keep a margin.
    size_t largest = 0;
    for (const Remote& r : library.data().remotes) largest = std::max(largest, estimateRemoteFileBytes(r));
    const size_t needed = largest * 3 + 8 * 1024;
    if (info.freeHeapBytes() < needed) {
      log.log(LogLevel::Error, "store", "not enough free memory to save");
      return Status::fail(Err::LimitReached, "not enough free memory to save");
    }
  }
  Status st = repo.save(library);
  if (st.ok()) {
    library.markClean();
  } else {
    log.log(LogLevel::Error, "store", "library save failed: " + st.message);
  }
  return st;
}

Status UiContext::persistSettings() {
  Status st = settingsStore.save(settings);
  if (!st.ok()) log.log(LogLevel::Error, "store", "settings save failed: " + st.message);
  return st;
}

std::string spanishError(const Status& st) {
  switch (st.code) {
    case Err::Ok: return "Listo";
    case Err::InvalidArgument: return "Dato inválido";
    case Err::OutOfRange: return "Valor fuera de rango";
    case Err::LimitReached: return "Límite alcanzado";
    case Err::NotFound: return "No encontrado";
    case Err::AlreadyExists: return "Ya existe ese nombre";
    case Err::InUse: return "En uso: vacíelo antes";
    case Err::Corrupt: return "Datos dañados";
    case Err::UnsupportedVersion: return "Versión no soportada";
    case Err::StorageFailure: return "Error al guardar";
    case Err::Unavailable: return "No disponible ahora";
  }
  return "Error";
}

// ---------- UiApp ----------

UiApp::UiApp(UiContext& ctx, IDisplay& display, std::unique_ptr<Screen> home)
    : ctx_(ctx), display_(display), painter_(display), home_(std::move(home)) {}

void UiApp::start() {
  stack_.clear();
  // The home screen object is kept alive by the stack; no clone is needed.
  stack_.push_back(std::move(home_));
  stack_.back()->onEnter(ctx_);
  dirty_ = true;
}

void UiApp::apply(Nav nav) {
  switch (nav.kind) {
    case Nav::Kind::None:
      return;
    case Nav::Kind::Push:
      stack_.push_back(std::move(nav.screen));
      stack_.back()->onEnter(ctx_);
      break;
    case Nav::Kind::Replace:
      if (!stack_.empty()) {
        stack_.back()->onExit(ctx_);
        stack_.pop_back();
      }
      stack_.push_back(std::move(nav.screen));
      stack_.back()->onEnter(ctx_);
      break;
    case Nav::Kind::Pop:
      for (int i = 0; i < nav.count && stack_.size() > 1; ++i) {
        stack_.back()->onExit(ctx_);
        stack_.pop_back();
      }
      stack_.back()->onResume(ctx_);
      break;
    case Nav::Kind::HomeThen:
      while (stack_.size() > 1) {
        stack_.back()->onExit(ctx_);
        stack_.pop_back();
      }
      stack_.back()->onResume(ctx_);
      if (nav.screen) {
        stack_.push_back(std::move(nav.screen));
        stack_.back()->onEnter(ctx_);
      }
      break;
  }
  dirty_ = true;
}

void UiApp::handleKey(const KeyEvent& ev) {
  if (stack_.empty()) return;
  apply(stack_.back()->onKey(ctx_, ev));
  dirty_ = true;
}

void UiApp::tick() {
  // Services run whatever screen is visible: capture, transmission and exploration never depend on the UI.
  ctx_.learn.tick();
  ctx_.transmit.service();
  ctx_.explore.tick();

  if (ctx_.learn.state() == LearnState::Captured && prevLearn_ != LearnState::Captured && ctx_.learn.result()) {
    ctx_.captures.push_front(CaptureRecord{ctx_.clock.nowMs(), *ctx_.learn.result()});
    while (ctx_.captures.size() > UiContext::kMaxCaptures) ctx_.captures.pop_back();
  }
  // A change in any service state is visible on screen: redraw now instead of at the next 200 ms tick.
  if (ctx_.learn.state() != prevLearn_ || ctx_.transmit.state() != prevTx_ || ctx_.explore.state() != prevExplore_ ||
      ctx_.explore.sentCount() != prevExploreSent_) {
    dirty_ = true;
  }
  prevLearn_ = ctx_.learn.state();
  prevTx_ = ctx_.transmit.state();
  prevExplore_ = ctx_.explore.state();
  prevExploreSent_ = ctx_.explore.sentCount();

  if (!stack_.empty()) stack_.back()->onTick(ctx_);
  renderIfNeeded(false);
}

void UiApp::renderIfNeeded(bool force) {
  if (stack_.empty()) return;
  const uint32_t now = ctx_.clock.nowMs();
  const bool periodic = now - lastRenderMs_ >= 200;
  if (!force && !dirty_ && !periodic) return;
  if (!ctx_.displayHealthy && now - lastDisplayRetryMs_ < 2000) return;  // do not hammer a dead bus

  lastRenderMs_ = now;
  painter_.begin();
  // The toast is drawn first and owns the footer row, so the screen's own footer cannot overlap it.
  if (const std::string* toast = ctx_.activeToast()) painter_.toastFooter(*toast);
  stack_.back()->render(ctx_, painter_);
  display_.setInverted(ctx_.settings.invertDisplay);

  if (ctx_.displayHealthy && haveHash_ && !dirty_ && painter_.hash() == lastHash_) return;
  dirty_ = false;
  Status st = painter_.present();
  if (st.ok()) {
    lastHash_ = painter_.hash();
    haveHash_ = true;
    ++flushCount_;
    if (!ctx_.displayHealthy) ctx_.log.log(LogLevel::Info, "display", "recovered");
    ctx_.displayHealthy = true;
    displayErrorLogged_ = false;
  } else {
    haveHash_ = false;
    lastDisplayRetryMs_ = now;
    ctx_.displayHealthy = false;
    if (!displayErrorLogged_) {
      ctx_.log.log(LogLevel::Error, "display", "flush failed: " + st.message);
      displayErrorLogged_ = true;
    }
  }
}

}  // namespace iris
