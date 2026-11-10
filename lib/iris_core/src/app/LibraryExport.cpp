#include "app/LibraryExport.h"

#include "app/RemoteView.h"
#include "import/FlipperIrWriter.h"

namespace iris {

std::string exportFileName(const std::string& name) {
  std::string out;
  for (unsigned char ch : name) {
    if ((ch & 0xC0) == 0x80) continue;  // the tail of a UTF-8 character: the lead byte already became one "_"
    const bool keep = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' ||
                      ch == '_' || ch == '.' || ch == '(' || ch == ')';
    out += keep ? static_cast<char>(ch) : '_';
  }
  while (!out.empty() && (out.front() == ' ' || out.front() == '.')) out.erase(out.begin());
  while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
  return out.empty() ? "_" : out;
}

namespace {

Status writeOne(IFileSink& sink, const RemoteLibrary& library, const Remote& r, ExportSummary& summary) {
  Result<RemoteView> view = viewSavedRemote(library, r.id);
  if (!view.ok()) return view.status();
  const WrittenIr w = writeFlipperIr(view.value().buttons);
  summary.skippedButtons += w.skipped;
  if (w.written == 0) return Status::success();
  const Category* cat = library.findCategory(r.categoryId);
  const std::string path = std::string(kExportRoot) + "/" + exportFileName(cat ? cat->name : "") + "/" + exportFileName(r.name) + ".ir";
  Status st = sink.writeText(path, w.text);
  if (!st.ok()) return st;
  ++summary.remotes;
  summary.buttons += w.written;
  return Status::success();
}

}  // namespace

Status exportRemote(IFileSink& sink, const RemoteLibrary& library, Id remoteId, ExportSummary& summary) {
  if (!sink.writable()) return Status::fail(Err::Unavailable, "no card");
  const Remote* r = library.findRemote(remoteId);
  if (!r) return Status::fail(Err::NotFound, "remote not found");
  return writeOne(sink, library, *r, summary);
}

Status exportCategory(IFileSink& sink, const RemoteLibrary& library, Id categoryId, ExportSummary& summary) {
  if (!sink.writable()) return Status::fail(Err::Unavailable, "no card");
  if (!library.findCategory(categoryId)) return Status::fail(Err::NotFound, "category not found");
  for (const Remote& r : library.data().remotes) {
    if (r.categoryId != categoryId) continue;
    Status st = writeOne(sink, library, r, summary);
    if (!st.ok()) return st;
  }
  return Status::success();
}

Status exportAll(IFileSink& sink, const RemoteLibrary& library, ExportSummary& summary) {
  if (!sink.writable()) return Status::fail(Err::Unavailable, "no card");
  for (const Remote& r : library.data().remotes) {
    Status st = writeOne(sink, library, r, summary);
    if (!st.ok()) return st;
  }
  return Status::success();
}

}  // namespace iris
