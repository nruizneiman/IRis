// One control as the modes see it, wherever it comes from: the saved library or a Flipper-IRDB file on the card.
// A card control is read when it is opened and never stored; the modes (send, loop, crazy, Be Gone) work on this
// view and do not care which one they were given.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Result.h"
#include "app/RemoteLibrary.h"
#include "domain/IrSignal.h"
#include "domain/Library.h"
#include "hardware/IFileSource.h"
#include "import/FlipperIr.h"

namespace iris {

struct ViewButton {
  std::string name;
  std::optional<IrSignal> signal;  // a saved button can still be empty
};

struct RemoteView {
  std::string name;
  bool fromCard = false;
  Id savedId = 0;           // library controls
  std::string path;         // card controls
  std::vector<ViewButton> buttons;
  ImportReport report;      // card controls: what the file held and what could not be converted
};

Result<RemoteView> viewSavedRemote(const RemoteLibrary& library, Id remoteId);
// Reads and converts one `.ir` file. Fails with Unavailable (no card), OutOfRange (file too large), Corrupt or
// UnsupportedVersion (not an IRDB file), NotFound. Buttons beyond kMaxCardButtons are dropped and counted in the report.
Result<RemoteView> viewCardRemote(IFileSource& files, const std::string& path);

std::string remoteNameFromFile(const std::string& fileName);  // "LG_TV.ir" -> "LG TV", within the name length limit
bool endsWithIr(const std::string& name);

// Which of the buttons is the power button. For "on": an explicit on button if there is one, else the toggle; for "off":
// an explicit off button, else the toggle. Null when there is none with a signal.
enum class PowerWanted : uint8_t { Off, On };
const ViewButton* findPowerButton(const RemoteView& view, PowerWanted wanted);

}  // namespace iris
