// Exports saved controls to the card as Flipper `.ir` files, one folder per category:
//   /iris/export/<category>/<control>.ir
// File and folder names keep ASCII letters, digits, space, "-", "_", "." and "()"; anything else becomes "_" (the card's
// file system is not trusted with other characters), so an accented letter is written as "_". A category with no
// control to write leaves no folder.
#pragma once

#include <string>

#include "Result.h"
#include "app/RemoteLibrary.h"
#include "hardware/IFileSink.h"

namespace iris {

inline constexpr const char* kExportRoot = "/iris/export";

struct ExportSummary {
  size_t remotes = 0;          // files written
  size_t buttons = 0;          // buttons inside them
  size_t skippedButtons = 0;   // empty buttons or signals the format cannot carry
};

std::string exportFileName(const std::string& name);  // sanitized, never empty

// Each returns the first failure (the summary keeps what was written before it). A control with no button that can be
// written is not exported at all.
Status exportRemote(IFileSink& sink, const RemoteLibrary& library, Id remoteId, ExportSummary& summary);
Status exportCategory(IFileSink& sink, const RemoteLibrary& library, Id categoryId, ExportSummary& summary);
Status exportAll(IFileSink& sink, const RemoteLibrary& library, ExportSummary& summary);

}  // namespace iris
