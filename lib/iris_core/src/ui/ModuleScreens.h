// The modes and the card-aware control screens: loop, crazy, Be Gone, active listening, controls read straight from the
// card (alone or next to the saved ones) and the generic card browser.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/Ui.h"
#include "ui/Widgets.h"

namespace iris {

// A control to operate on: a saved one (savedId) or a card file (path).
struct RemoteRef {
  Id savedId = 0;
  std::string path;
  bool fromCard() const { return !path.empty(); }
};

struct CardBrowserOptions {
  std::string id = "card-browser";
  std::string rootTitle = "Tarjeta SD";   // title shown at `root`; inside a folder the title is its name
  std::string root = "/irdb";
  std::string dir = "/irdb";              // the folder this screen shows
  // OK on a file: the screen to go to. Required.
  std::function<Nav(UiContext&, const std::string& dir, const std::string& file)> onFile;
  // When set, the first row of every folder is `folderLabel`; OK runs this with the folder's path.
  std::function<Nav(UiContext&, const std::string& dir)> onFolder;
  std::string folderLabel = "» Usar esta carpeta";
};
std::unique_ptr<Screen> makeCardBrowser(CardBrowserOptions options);

// The card's top-level folders as rows for the Controles list ("TVs/" with "SD" on the right); empty when there is no
// card or the setting that mixes them in is off.
std::vector<ListItem> cardLibraryItems(UiContext& c);

std::unique_ptr<Screen> makeCardRemoteScreen(const std::string& path);  // buttons of a card control; OK sends
// Imports every `.ir` file directly inside `dir` as a saved control: asks for the category (a new one named after the
// folder, or an existing one), then works through the files a few at a time and can be cancelled.
std::unique_ptr<Screen> makeImportFolderPicker(const std::string& dir);

// Picks a control from the saved ones or from the card, then calls `onPick`.
std::unique_ptr<Screen> makeRemoteSourcePicker(const std::string& title, std::function<Nav(UiContext&, RemoteRef)> onPick);

std::unique_ptr<Screen> makeLoopSetup(RemoteRef remote);   // choose buttons and the pause, then run
std::unique_ptr<Screen> makeCrazySetup(RemoteRef remote);  // choose the pause, then run
std::unique_ptr<Screen> makeLoopMode();                    // home entries: pick the control first
std::unique_ptr<Screen> makeCrazyMode();
std::unique_ptr<Screen> makeBeGoneMode();
std::unique_ptr<Screen> makeListenMode();

}  // namespace iris
