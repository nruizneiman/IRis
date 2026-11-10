// Helpers that live in AppScreens.cpp and are shared with the mode screens (ModuleScreens.cpp). Thin wrappers: the real
// code stays where it was.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/RemoteView.h"
#include "ui/Ui.h"
#include "ui/Widgets.h"

namespace iris::kit {

std::string numStr(unsigned long long v);
std::string shortSignal(const IrSignal& s);
std::vector<std::string> signalLines(const IrSignal& s);
// Reports a failed library change; on success saves and shows `okMsg`. Returns whether the change itself worked.
bool finishChange(UiContext& c, const Status& st, const std::string& okMsg);
void sendSignal(UiContext& c, const IrSignal& s);
IrSignal rawReplay(const IrSignal& captured);

using PickFn = std::function<Nav(UiContext&, Id remote, Id button, int extraPops)>;
std::unique_ptr<Screen> makeRemotePicker(const std::string& title, PickFn onPick);  // saved controls only
std::unique_ptr<Screen> makeRemoteScreen(Id remote);
std::unique_ptr<Screen> makeCaptureDetail(const CaptureRecord& rec);

std::vector<std::string> categoryPresets();
std::unique_ptr<TextInputScreen> withPresets(std::unique_ptr<TextInputScreen> screen, std::vector<std::string> list);

// Creates a saved control from a view (a card control, usually) in `category`. Names are made unique; on failure nothing
// is left behind (and the category is removed again when `removeCategoryOnFailure`, for one created a moment ago).
// On success it goes home and opens the new control.
Nav importView(UiContext& c, const RemoteView& view, Id category, bool removeCategoryOnFailure);
// The same without any screen change and without saving to flash; the caller saves when it is done.
Result<Id> importViewQuiet(UiContext& c, const RemoteView& view, Id category);
// Asks which category to save the view into (existing ones or a new one), then imports it.
std::unique_ptr<Screen> makeImportViewCategoryPicker(std::shared_ptr<RemoteView> view);

}  // namespace iris::kit
