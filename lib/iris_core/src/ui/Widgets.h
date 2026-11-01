// Reusable screens. Key conventions (the same everywhere):
//   Up/Down  move / change value      OK     choose / confirm
//   Left     back (or delete)         Right  context actions (lists) / next
//   Left long press: back to home (lists) or cancel (editors)
#pragma once

#include "ui/Ui.h"

namespace iris {

struct ListItem {
  std::string label;
  std::string value;                          // right-aligned, optional
  std::function<Nav(UiContext&)> onSelect;    // OK
};

class ListScreen : public Screen {
 public:
  using Provider = std::function<std::vector<ListItem>(UiContext&)>;
  using TitleFn = std::function<std::string(UiContext&)>;

  ListScreen(std::string id, std::string title, Provider provider);

  ListScreen& footer(std::string text) { footer_ = std::move(text); return *this; }
  ListScreen& emptyText(std::string text) { empty_ = std::move(text); return *this; }
  ListScreen& titleFn(TitleFn fn) { titleFn_ = std::move(fn); return *this; }
  // Right key: receives the selected index. Without it Right does nothing.
  ListScreen& onContext(std::function<Nav(UiContext&, size_t)> fn) { context_ = std::move(fn); return *this; }

  const char* id() const override { return id_.c_str(); }
  void onEnter(UiContext& c) override { refresh(c); }
  void onResume(UiContext& c) override { refresh(c); }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

  size_t selected() const { return sel_; }
  const std::vector<ListItem>& items() const { return items_; }

 private:
  void refresh(UiContext&);
  std::string id_, title_, footer_ = "OK:elegir Izq:atrás", empty_ = "(vacío)";
  Provider provider_;
  TitleFn titleFn_;
  std::function<Nav(UiContext&, size_t)> context_;
  std::vector<ListItem> items_;
  size_t sel_ = 0, top_ = 0;
};

struct GridItem {
  std::string label;
  const uint8_t* icon = nullptr;             // 16x16 XBM (ui/Icons.h)
  std::function<Nav(UiContext&)> onSelect;   // OK
};

// Icon menu: up to 6 icons in 3 columns x 2 rows, the selected one framed and its name centred on the bottom row.
// Left/Right move by one, Up/Down by a whole row of three (wrapping), OK opens. Left does not leave the screen:
// it is the home screen.
class IconGridScreen : public Screen {
 public:
  using Provider = std::function<std::vector<GridItem>(UiContext&)>;
  using TitleFn = std::function<std::string(UiContext&)>;

  IconGridScreen(std::string id, std::string title, Provider provider);
  IconGridScreen& titleFn(TitleFn fn) { titleFn_ = std::move(fn); return *this; }

  const char* id() const override { return id_.c_str(); }
  void onEnter(UiContext& c) override { refresh(c); }
  void onResume(UiContext& c) override { refresh(c); }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

  size_t selected() const { return sel_; }
  const std::vector<GridItem>& items() const { return items_; }

 private:
  void refresh(UiContext&);
  std::string id_, title_;
  Provider provider_;
  TitleFn titleFn_;
  std::vector<GridItem> items_;
  size_t sel_ = 0;
};

// Read-only text page (Up/Down scroll). An optional OK action (e.g. "send").
class InfoScreen : public Screen {
 public:
  using Lines = std::function<std::vector<std::string>(UiContext&)>;
  InfoScreen(std::string id, std::string title, Lines lines);
  InfoScreen& okAction(std::string footerText, std::function<Nav(UiContext&)> fn) {
    okFooter_ = std::move(footerText);
    ok_ = std::move(fn);
    return *this;
  }
  const char* id() const override { return id_.c_str(); }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

 private:
  std::string id_, title_, okFooter_ = "Izq:atrás";
  Lines lines_;
  std::function<Nav(UiContext&)> ok_;
  size_t scroll_ = 0;
  size_t lastCount_ = 0;
};

// Yes/No. OK = yes, anything else that leaves the screen = no. Destructive actions use this.
class ConfirmScreen : public Screen {
 public:
  ConfirmScreen(std::string title, std::vector<std::string> lines, std::function<Nav(UiContext&)> onYes);
  const char* id() const override { return "confirm"; }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

 private:
  std::string title_;
  std::vector<std::string> lines_;
  std::function<Nav(UiContext&)> onYes_;
};

// Text entry with an on-screen keyboard (4 rows of 10 keys plus a row of Aa / Espacio / Borrar / Listo).
// Arrows move over the keys, OK presses the highlighted one. Three pages (lower case, upper case, accents and symbols):
// a new text starts on the upper-case page and every letter returns to lower case, like a phone's shift.
// Left long press cancels. With `presets()` set and an empty initial text the screen first shows a list of typical
// names (OK picks one, the last entry opens the keyboard).
class TextInputScreen : public Screen {
 public:
  using Done = std::function<Nav(UiContext&, const std::string&)>;

  static constexpr int kPages = 3, kKeyRows = 4, kKeyCols = 10, kCtrlRow = 4, kCtrlCols = 4;
  struct Cell {
    int page = 0, row = 0, col = 0;
  };

  TextInputScreen(std::string title, std::string initial, size_t maxBytes, Done done);
  TextInputScreen& presets(std::vector<std::string> list);

  const char* id() const override { return "text-input"; }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

  // Where a character sits on the keyboard (the first page that has it). False when it is not on any key.
  static bool locate(const std::string& ch, Cell& out);
  // Inspection (tests, scripted input).
  bool inPresets() const { return mode_ == Mode::Presets; }
  const std::vector<std::string>& presetList() const { return presets_; }
  size_t presetIndex() const { return presetSel_; }  // == presetList().size() is the "Escribir otro..." entry
  int page() const { return page_; }
  int row() const { return row_; }
  int col() const { return col_; }
  const std::string& text() const { return text_; }

 private:
  enum class Mode : uint8_t { Presets, Keyboard };
  Nav pressKey(UiContext&);
  Nav onPresetKey(UiContext&, const KeyEvent&);
  std::string title_, text_;
  size_t maxBytes_;
  Done done_;
  Mode mode_ = Mode::Keyboard;
  std::vector<std::string> presets_;
  size_t presetSel_ = 0, presetTop_ = 0;
  int page_ = 0, row_ = 0, col_ = 0;
};

// Edit a number digit by digit. Left/Right move the cursor, Up/Down change the digit, OK accepts
// (rejected with a message when outside [min, max]). Left on the first digit cancels.
class NumberInputScreen : public Screen {
 public:
  using Done = std::function<Nav(UiContext&, uint32_t)>;
  NumberInputScreen(std::string title, uint32_t value, uint32_t min, uint32_t max, unsigned base, Done done);
  const char* id() const override { return "number-input"; }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

 private:
  std::string title_;
  uint32_t value_, min_, max_;
  unsigned base_, digits_;
  unsigned cursor_;  // 0 = most significant digit
  Done done_;
};

struct FormField {
  enum class Kind : uint8_t { Choice, Number, Action, Info };
  Kind kind = Kind::Info;
  std::string label;
  std::vector<std::string> options;  // Choice
  size_t choice = 0;
  uint32_t number = 0, min = 0, max = 0;  // Number
  unsigned base = 10;
  bool visible = true;
  std::function<std::string()> infoText;  // Info: computed each frame
};

// A list of editable fields. OK cycles a Choice, opens an editor for a Number, runs an Action.
class FormScreen : public Screen {
 public:
  using ActionFn = std::function<Nav(UiContext&, FormScreen&)>;
  using ChangeFn = std::function<void(FormScreen&, size_t fieldIndex)>;

  FormScreen(std::string id, std::string title);

  size_t addChoice(std::string label, std::vector<std::string> options, size_t initial = 0);
  size_t addNumber(std::string label, uint32_t value, uint32_t min, uint32_t max, unsigned base);
  size_t addInfo(std::function<std::string()> text);
  size_t addAction(std::string label, ActionFn fn);
  FormScreen& onChange(ChangeFn fn) { change_ = std::move(fn); return *this; }

  FormField& field(size_t i) { return fields_[i]; }
  const FormField& field(size_t i) const { return fields_[i]; }
  size_t fieldCount() const { return fields_.size(); }

  const char* id() const override { return id_.c_str(); }
  Nav onKey(UiContext&, const KeyEvent&) override;
  void render(UiContext&, Painter&) override;

 private:
  std::vector<size_t> visibleIndexes() const;
  std::string id_, title_;
  std::vector<FormField> fields_;
  std::vector<ActionFn> actions_;  // parallel to fields_
  ChangeFn change_;
  size_t sel_ = 0;  // index into visibleIndexes()
};

}  // namespace iris
