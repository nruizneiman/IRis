#include "ui/Widgets.h"

#include <cstdio>

#include "ui/Icons.h"
#include "ui/Utf8.h"

namespace iris {

namespace {

bool isMove(const KeyEvent& e) {
  return (e.key == Key::Up || e.key == Key::Down) && (e.action == KeyAction::Press || e.action == KeyAction::Repeat);
}
bool isPress(const KeyEvent& e, Key k) { return e.key == k && e.action == KeyAction::Press; }
bool isLong(const KeyEvent& e, Key k) { return e.key == k && e.action == KeyAction::LongPress; }

}  // namespace

// ================= ListScreen =================

ListScreen::ListScreen(std::string id, std::string title, Provider provider)
    : id_(std::move(id)), title_(std::move(title)), provider_(std::move(provider)) {}

void ListScreen::refresh(UiContext& c) {
  items_ = provider_(c);
  if (items_.empty()) sel_ = 0;
  else if (sel_ >= items_.size()) sel_ = items_.size() - 1;
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= top_ + Painter::kContentRows) top_ = sel_ + 1 - Painter::kContentRows;
}

Nav ListScreen::onKey(UiContext& c, const KeyEvent& e) {
  if (isMove(e) && !items_.empty()) {
    const size_t n = items_.size();
    sel_ = (e.key == Key::Down) ? (sel_ + 1) % n : (sel_ + n - 1) % n;
    if (sel_ < top_) top_ = sel_;
    if (sel_ >= top_ + Painter::kContentRows) top_ = sel_ + 1 - Painter::kContentRows;
    return Nav::none();
  }
  if (isPress(e, Key::Ok) && !items_.empty()) {
    const auto fn = items_[sel_].onSelect;  // copy: refresh() below replaces items_
    if (!fn) return Nav::none();
    Nav nav = fn(c);
    if (nav.kind == Nav::Kind::None) refresh(c);  // an in-place action (toggle) may change labels
    return nav;
  }
  if (isPress(e, Key::Right) && context_ && !items_.empty()) return context_(c, sel_);
  if (isPress(e, Key::Left)) return Nav::pop();
  if (isLong(e, Key::Left)) return Nav::home();
  return Nav::none();
}

void ListScreen::render(UiContext& c, Painter& p) {
  p.title(titleFn_ ? titleFn_(c) : title_);
  if (items_.empty()) {
    p.text(0, 3, empty_);
  } else {
    for (size_t i = 0; i < static_cast<size_t>(Painter::kContentRows) && top_ + i < items_.size(); ++i) {
      const ListItem& it = items_[top_ + i];
      p.item(Painter::kFirstContentRow + static_cast<int>(i), it.label, it.value, top_ + i == sel_);
    }
    p.scrollbar(items_.size(), top_, Painter::kContentRows);
  }
  p.footer(footer_);
}

// ================= IconGridScreen =================

namespace {
constexpr int kGridCols = 3;
constexpr int kGridMaxItems = 6;
constexpr int kGridLeft = 4;       // the grid uses x = 4..123, the two right-most columns stay free
constexpr int kGridCellW = 40;
constexpr int kGridFirstY = 14;    // icon rows at y = 14 and y = 35 (16 px high, 3 px frame margin)
constexpr int kGridRowStep = 21;
}  // namespace

IconGridScreen::IconGridScreen(std::string id, std::string title, Provider provider)
    : id_(std::move(id)), title_(std::move(title)), provider_(std::move(provider)) {}

void IconGridScreen::refresh(UiContext& c) {
  items_ = provider_(c);
  if (items_.size() > static_cast<size_t>(kGridMaxItems)) items_.resize(kGridMaxItems);
  if (sel_ >= items_.size()) sel_ = items_.empty() ? 0 : items_.size() - 1;
}

Nav IconGridScreen::onKey(UiContext& c, const KeyEvent& e) {
  const size_t n = items_.size();
  if (n == 0) return Nav::none();
  const bool move = e.action == KeyAction::Press || e.action == KeyAction::Repeat;
  if (move && e.key == Key::Down) {
    sel_ = (sel_ + kGridCols) % n;
    return Nav::none();
  }
  if (move && e.key == Key::Up) {
    sel_ = (sel_ + n - (static_cast<size_t>(kGridCols) % n)) % n;
    return Nav::none();
  }
  if (isPress(e, Key::Right)) {
    sel_ = (sel_ + 1) % n;
    return Nav::none();
  }
  if (isPress(e, Key::Left)) {
    sel_ = (sel_ + n - 1) % n;
    return Nav::none();
  }
  if (isPress(e, Key::Ok)) {
    const auto fn = items_[sel_].onSelect;
    if (!fn) return Nav::none();
    Nav nav = fn(c);
    if (nav.kind == Nav::Kind::None) refresh(c);
    return nav;
  }
  return Nav::none();
}

void IconGridScreen::render(UiContext& c, Painter& p) {
  p.title(titleFn_ ? titleFn_(c) : title_);
  if (gaugeFn_) {
    const int segments = gaugeFn_(c);
    if (segments >= 0) p.batteryGauge(segments);
  }
  for (size_t i = 0; i < items_.size(); ++i) {
    const int col = static_cast<int>(i) % kGridCols, row = static_cast<int>(i) / kGridCols;
    const int x = kGridLeft + col * kGridCellW + (kGridCellW - icons::kSize) / 2;
    const int y = kGridFirstY + row * kGridRowStep;
    if (items_[i].icon) p.bitmap(x, y, icons::kSize, icons::kSize, items_[i].icon);
    if (i == sel_) p.frame(x - 3, y - 3, icons::kSize + 6, icons::kSize + 6);
  }
  if (!items_.empty()) p.footerCentered(items_[sel_].label);
}

// ================= InfoScreen =================

InfoScreen::InfoScreen(std::string id, std::string title, Lines lines)
    : id_(std::move(id)), title_(std::move(title)), lines_(std::move(lines)) {}

Nav InfoScreen::onKey(UiContext& c, const KeyEvent& e) {
  if (isMove(e)) {
    if (e.key == Key::Down && scroll_ + Painter::kContentRows < lastCount_) ++scroll_;
    if (e.key == Key::Up && scroll_ > 0) --scroll_;
    return Nav::none();
  }
  if (isPress(e, Key::Ok) && ok_) return ok_(c);
  if (isPress(e, Key::Left)) return Nav::pop();
  if (isLong(e, Key::Left)) return Nav::home();
  return Nav::none();
}

void InfoScreen::render(UiContext& c, Painter& p) {
  const std::vector<std::string> lines = lines_(c);
  lastCount_ = lines.size();
  if (scroll_ > 0 && scroll_ + Painter::kContentRows > lines.size()) {
    scroll_ = lines.size() > static_cast<size_t>(Painter::kContentRows) ? lines.size() - Painter::kContentRows : 0;
  }
  p.title(title_);
  for (size_t i = 0; i < static_cast<size_t>(Painter::kContentRows) && scroll_ + i < lines.size(); ++i) {
    p.text(0, Painter::kFirstContentRow + static_cast<int>(i), lines[scroll_ + i]);
  }
  p.footer(okFooter_);
}

// ================= ConfirmScreen =================

ConfirmScreen::ConfirmScreen(std::string title, std::vector<std::string> lines, std::function<Nav(UiContext&)> onYes)
    : title_(std::move(title)), lines_(std::move(lines)), onYes_(std::move(onYes)) {}

Nav ConfirmScreen::onKey(UiContext& c, const KeyEvent& e) {
  if (isPress(e, Key::Ok)) return onYes_(c);
  if (isPress(e, Key::Left) || isLong(e, Key::Left)) return Nav::pop();
  return Nav::none();
}

void ConfirmScreen::render(UiContext&, Painter& p) {
  p.title(title_);
  for (size_t i = 0; i < lines_.size() && i < static_cast<size_t>(Painter::kContentRows); ++i) {
    p.text(0, Painter::kFirstContentRow + static_cast<int>(i), lines_[i]);
  }
  p.footer("OK:Sí  Izq:No");
}

// ================= TextInputScreen =================

namespace {

// Keyboard pages: rows of ten keys. Row 3 (digits) is the same on every page.
const char* const kKeyPages[TextInputScreen::kPages][TextInputScreen::kKeyRows][TextInputScreen::kKeyCols] = {
    {{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
     {"a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ"},
     {"z", "x", "c", "v", "b", "n", "m", ",", ".", "-"},
     {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}},
    {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
     {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ"},
     {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "-"},
     {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}},
    {{"á", "é", "í", "ó", "ú", "ü", "¿", "¡", "!", "?"},
     {"Á", "É", "Í", "Ó", "Ú", "Ü", "(", ")", ":", ";"},
     {"+", "_", "/", "@", "#", "&", "%", "*", "=", "'"},
     {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}}};

constexpr int kKeyW = 12, kKeyLeft = 4, kKeyTopRow = 2;  // keys: x = 4 + col*12, text rows 2..5; control row = row 6
constexpr int kCtrlW = 32;

// Label of the "next page" key: it shows what pressing it gives.
const char* nextPageLabel(int page) {
  switch (page) {
    case 0: return "ABC";
    case 1: return "á¿!";
    default: return "abc";
  }
}

// Removes the last code point.
void popCodePoint(std::string& s) {
  while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
  if (!s.empty()) s.pop_back();
}

int rowWidth(int row) { return row == TextInputScreen::kCtrlRow ? TextInputScreen::kCtrlCols : TextInputScreen::kKeyCols; }
}  // namespace

TextInputScreen::TextInputScreen(std::string title, std::string initial, size_t maxBytes, Done done)
    : title_(std::move(title)), text_(std::move(initial)), maxBytes_(maxBytes), done_(std::move(done)) {
  page_ = text_.empty() ? 1 : 0;  // a new name starts with a capital letter
}

TextInputScreen& TextInputScreen::presets(std::vector<std::string> list) {
  presets_ = std::move(list);
  if (text_.empty() && !presets_.empty()) mode_ = Mode::Presets;
  return *this;
}

bool TextInputScreen::locate(const std::string& ch, Cell& out) {
  for (int p = 0; p < kPages; ++p)
    for (int r = 0; r < kKeyRows; ++r)
      for (int c = 0; c < kKeyCols; ++c)
        if (ch == kKeyPages[p][r][c]) {
          out = Cell{p, r, c};
          return true;
        }
  return false;
}

Nav TextInputScreen::pressKey(UiContext& c) {
  if (row_ < kCtrlRow) {
    const std::string ch = kKeyPages[page_][row_][col_];
    if (text_.size() + ch.size() > maxBytes_) {
      c.toast("Nombre muy largo");
      return Nav::none();
    }
    text_ += ch;
    if (page_ != 0) page_ = 0;  // one-shot shift
    return Nav::none();
  }
  switch (col_) {
    case 0: page_ = (page_ + 1) % kPages; return Nav::none();
    case 1:
      if (text_.size() + 1 > maxBytes_) c.toast("Nombre muy largo");
      else text_ += ' ';
      return Nav::none();
    case 2: popCodePoint(text_); return Nav::none();
    default: return done_(c, text_);
  }
}

Nav TextInputScreen::onPresetKey(UiContext& c, const KeyEvent& e) {
  const size_t n = presets_.size() + 1;  // the last entry is "Escribir otro..."
  if (isMove(e)) {
    presetSel_ = (e.key == Key::Down) ? (presetSel_ + 1) % n : (presetSel_ + n - 1) % n;
    if (presetSel_ < presetTop_) presetTop_ = presetSel_;
    if (presetSel_ >= presetTop_ + Painter::kContentRows) presetTop_ = presetSel_ + 1 - Painter::kContentRows;
    return Nav::none();
  }
  if (isPress(e, Key::Ok)) {
    if (presetSel_ < presets_.size()) return done_(c, presets_[presetSel_]);
    mode_ = Mode::Keyboard;
    return Nav::none();
  }
  if (isPress(e, Key::Left) || isLong(e, Key::Left)) return Nav::pop();
  return Nav::none();
}

Nav TextInputScreen::onKey(UiContext& c, const KeyEvent& e) {
  if (mode_ == Mode::Presets) return onPresetKey(c, e);
  if (isLong(e, Key::Left)) return Nav::pop();
  if (isMove(e)) {
    if (e.key == Key::Down) {
      const int next = (row_ + 1) % (kCtrlRow + 1);
      col_ = next == kCtrlRow ? col_ * kCtrlCols / kKeyCols : (row_ == kCtrlRow ? col_ * kKeyCols / kCtrlCols + 1 : col_);
      row_ = next;
    } else {
      const int next = (row_ + kCtrlRow) % (kCtrlRow + 1);
      col_ = next == kCtrlRow ? col_ * kCtrlCols / kKeyCols : (row_ == kCtrlRow ? col_ * kKeyCols / kCtrlCols + 1 : col_);
      row_ = next;
    }
    return Nav::none();
  }
  if (isPress(e, Key::Right)) {
    col_ = (col_ + 1) % rowWidth(row_);
    return Nav::none();
  }
  if (isPress(e, Key::Left)) {
    col_ = (col_ + rowWidth(row_) - 1) % rowWidth(row_);
    return Nav::none();
  }
  if (isPress(e, Key::Ok)) return pressKey(c);
  return Nav::none();
}

void TextInputScreen::render(UiContext&, Painter& p) {
  p.title(title_);
  if (mode_ == Mode::Presets) {
    const size_t n = presets_.size() + 1;
    for (size_t i = 0; i < static_cast<size_t>(Painter::kContentRows) && presetTop_ + i < n; ++i) {
      const size_t idx = presetTop_ + i;
      const std::string label = idx < presets_.size() ? presets_[idx] : std::string("Escribir otro...");
      p.item(Painter::kFirstContentRow + static_cast<int>(i), label, "", idx == presetSel_);
    }
    p.scrollbar(n, presetTop_, Painter::kContentRows);
    p.footer("OK:elegir Izq:atrás");
    return;
  }
  // The field shows the end of a long text so the cursor is always visible.
  std::string shown = text_ + "_";
  while (utf8Length(shown) > static_cast<size_t>(Painter::kCols)) shown.erase(0, 1);
  while (!shown.empty() && (static_cast<unsigned char>(shown.front()) & 0xC0) == 0x80) shown.erase(0, 1);
  p.text(0, 1, shown);
  for (int r = 0; r < kKeyRows; ++r)
    for (int cidx = 0; cidx < kKeyCols; ++cidx)
      p.keyCap(kKeyLeft + cidx * kKeyW, (kKeyTopRow + r) * Painter::kCellH, kKeyW, kKeyPages[page_][r][cidx],
               row_ == r && col_ == cidx);
  const char* const ctrl[kCtrlCols] = {nextPageLabel(page_), "Esp.", "Borrar", "Listo"};
  for (int cidx = 0; cidx < kCtrlCols; ++cidx)
    p.keyCap(cidx * kCtrlW, (kKeyTopRow + kKeyRows) * Painter::kCellH, kCtrlW, ctrl[cidx], row_ == kCtrlRow && col_ == cidx);
  p.footer("Izq largo:cancelar");
}

// ================= NumberInputScreen =================

NumberInputScreen::NumberInputScreen(std::string title, uint32_t value, uint32_t min, uint32_t max, unsigned base,
                                     Done done)
    : title_(std::move(title)), value_(value), min_(min), max_(max), base_(base ? base : 10), digits_(1), cursor_(0),
      done_(std::move(done)) {
  uint64_t cap = base_;
  while (cap <= max_) {
    cap *= base_;
    ++digits_;
  }
  cursor_ = digits_ - 1;  // start on the least significant digit
}

Nav NumberInputScreen::onKey(UiContext& c, const KeyEvent& e) {
  auto weight = [&](unsigned posFromLeft) {
    uint64_t w = 1;
    for (unsigned i = posFromLeft + 1; i < digits_; ++i) w *= base_;
    return w;
  };
  if (isMove(e)) {
    const uint64_t w = weight(cursor_);
    const int64_t cur = static_cast<int64_t>((value_ / w) % base_);
    const int64_t next = (cur + (e.key == Key::Down ? -1 : 1) + base_) % base_;  // Down lowers, Up raises
    const int64_t nv = static_cast<int64_t>(value_) + (next - cur) * static_cast<int64_t>(w);
    if (nv >= 0 && nv <= 0xFFFFFFFFll) value_ = static_cast<uint32_t>(nv);
    return Nav::none();
  }
  if (isPress(e, Key::Right)) {
    if (cursor_ + 1 < digits_) ++cursor_;
    return Nav::none();
  }
  if (isPress(e, Key::Left)) {
    if (cursor_ == 0) return Nav::pop();
    --cursor_;
    return Nav::none();
  }
  if (isLong(e, Key::Left)) return Nav::pop();
  if (isPress(e, Key::Ok)) {
    if (value_ < min_ || value_ > max_) {
      char buf[40];
      std::snprintf(buf, sizeof buf, base_ == 16 ? "Rango 0x%lX-0x%lX" : "Rango %lu-%lu",
                    static_cast<unsigned long>(min_), static_cast<unsigned long>(max_));
      c.toast(buf);
      return Nav::none();
    }
    return done_(c, value_);
  }
  return Nav::none();
}

void NumberInputScreen::render(UiContext&, Painter& p) {
  p.title(title_);
  char digits[16] = {0};
  uint64_t v = value_;
  for (int i = static_cast<int>(digits_) - 1; i >= 0; --i) {
    digits[i] = "0123456789ABCDEF"[v % base_];
    v /= base_;
  }
  const std::string prefix = base_ == 16 ? "0x" : "";
  p.text(2, 3, prefix + digits);
  p.text(2 + static_cast<int>(prefix.size()) + static_cast<int>(cursor_), 4, "^");
  char range[40];
  std::snprintf(range, sizeof range, base_ == 16 ? "Rango 0x%lX-0x%lX" : "Rango %lu-%lu",
                static_cast<unsigned long>(min_), static_cast<unsigned long>(max_));
  p.text(0, 5, range);
  p.footer("Izq/Der:dígito OK:ok");
}

// ================= FormScreen =================

FormScreen::FormScreen(std::string id, std::string title) : id_(std::move(id)), title_(std::move(title)) {}

size_t FormScreen::addChoice(std::string label, std::vector<std::string> options, size_t initial) {
  FormField f;
  f.kind = FormField::Kind::Choice;
  f.label = std::move(label);
  f.options = std::move(options);
  f.choice = initial < f.options.size() ? initial : 0;
  fields_.push_back(std::move(f));
  actions_.emplace_back();
  return fields_.size() - 1;
}

size_t FormScreen::addNumber(std::string label, uint32_t value, uint32_t min, uint32_t max, unsigned base) {
  FormField f;
  f.kind = FormField::Kind::Number;
  f.label = std::move(label);
  f.number = value;
  f.min = min;
  f.max = max;
  f.base = base;
  fields_.push_back(std::move(f));
  actions_.emplace_back();
  return fields_.size() - 1;
}

size_t FormScreen::addInfo(std::function<std::string()> text) {
  FormField f;
  f.kind = FormField::Kind::Info;
  f.infoText = std::move(text);
  fields_.push_back(std::move(f));
  actions_.emplace_back();
  return fields_.size() - 1;
}

size_t FormScreen::addAction(std::string label, ActionFn fn) {
  FormField f;
  f.kind = FormField::Kind::Action;
  f.label = std::move(label);
  fields_.push_back(std::move(f));
  actions_.push_back(std::move(fn));
  return fields_.size() - 1;
}

std::vector<size_t> FormScreen::visibleIndexes() const {
  std::vector<size_t> v;
  for (size_t i = 0; i < fields_.size(); ++i) {
    // Info rows are display-only and never take the cursor.
    if (fields_[i].visible && fields_[i].kind != FormField::Kind::Info) v.push_back(i);
  }
  return v;
}

Nav FormScreen::onKey(UiContext& c, const KeyEvent& e) {
  const std::vector<size_t> vis = visibleIndexes();
  if (sel_ >= vis.size()) sel_ = vis.empty() ? 0 : vis.size() - 1;
  if (isMove(e) && !vis.empty()) {
    const size_t n = vis.size();
    sel_ = (e.key == Key::Down) ? (sel_ + 1) % n : (sel_ + n - 1) % n;
    return Nav::none();
  }
  if (isPress(e, Key::Left)) return Nav::pop();
  if (isLong(e, Key::Left)) return Nav::home();
  if (isPress(e, Key::Ok) && !vis.empty()) {
    const size_t idx = vis[sel_];
    FormField& f = fields_[idx];
    switch (f.kind) {
      case FormField::Kind::Choice:
        if (!f.options.empty()) f.choice = (f.choice + 1) % f.options.size();
        if (change_) change_(*this, idx);
        return Nav::none();
      case FormField::Kind::Number:
        return Nav::push(std::make_unique<NumberInputScreen>(
            f.label, f.number, f.min, f.max, f.base, [this, idx](UiContext&, uint32_t v) {
              fields_[idx].number = v;
              if (change_) change_(*this, idx);
              return Nav::pop();
            }));
      case FormField::Kind::Action:
        return actions_[idx] ? actions_[idx](c, *this) : Nav::none();
      case FormField::Kind::Info:
        break;
    }
  }
  return Nav::none();
}

void FormScreen::render(UiContext&, Painter& p) {
  p.title(title_);
  const std::vector<size_t> vis = visibleIndexes();
  int row = Painter::kFirstContentRow;
  // Info rows first (they are previews), then the interactive rows with the cursor.
  for (const FormField& f : fields_) {
    if (f.visible && f.kind == FormField::Kind::Info && row < Painter::kFirstContentRow + Painter::kContentRows) {
      p.text(0, row++, f.infoText ? f.infoText() : "");
    }
  }
  const int room = Painter::kFirstContentRow + Painter::kContentRows - row;
  size_t first = 0;
  if (room > 0 && sel_ >= static_cast<size_t>(room)) first = sel_ + 1 - static_cast<size_t>(room);
  for (size_t i = first; i < vis.size() && row < Painter::kFirstContentRow + Painter::kContentRows; ++i, ++row) {
    const FormField& f = fields_[vis[i]];
    std::string value;
    if (f.kind == FormField::Kind::Choice && !f.options.empty()) value = f.options[f.choice];
    else if (f.kind == FormField::Kind::Number) {
      char buf[16];
      std::snprintf(buf, sizeof buf, f.base == 16 ? "0x%lX" : "%lu", static_cast<unsigned long>(f.number));
      value = buf;
    }
    p.item(row, f.label, value, i == sel_);
  }
  p.footer("OK:cambiar Izq:atrás");
}

}  // namespace iris
