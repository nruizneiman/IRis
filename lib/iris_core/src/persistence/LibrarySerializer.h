// Text formats of the stored library (format 2: one index file + one file per remote).
//
// Every file is "IRIS <version> <crc32 hex, 8 digits> <body length>\n" + JSON body; the CRC covers the
// body. The index lists the remote files with the CRC of each whole file, so a file that is damaged
// or belongs to another generation is detected. RAW durations are stored as hex text (4 digits per
// entry, big endian): far smaller to hold in RAM than a JSON number array.
//
// Format 1 (a single "library.irs" with number arrays) is still readable (legacy loader) and is
// migrated on the next save. Newer formats are refused, never guessed.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Result.h"
#include "domain/Library.h"

namespace iris {

constexpr unsigned kLegacyFormatVersion = 1;
constexpr unsigned kFormatVersion = 2;

// ---- envelope ----
std::string wrapEnvelope(unsigned version, const std::string& body);
struct Envelope {
  unsigned version = 0;
  std::string_view body;  // points into the text passed to openEnvelope
};
// Err::Corrupt for a bad header, length or checksum; Err::UnsupportedVersion for newer versions.
Result<Envelope> openEnvelope(const std::string& text);
// Header, length and CRC only (no JSON parsing): cheap integrity check.
Status checkEnvelope(const std::string& text);

// ---- format 2 ----
struct IndexEntry {
  Id id = 0;
  std::string name;
  Id categoryId = 0;
  std::string fileKey;
  uint32_t fileCrc = 0;  // CRC32 of the whole remote file text
};

struct IndexData {
  Id nextId = 1;
  uint32_t generation = 0;
  std::vector<Category> categories;
  std::vector<IndexEntry> remotes;
};

// Err::LimitReached when the JSON document could not be built in the available memory.
Result<std::string> serializeIndex(const IndexData& index);
Result<IndexData> deserializeIndex(const std::string& text);

// Only the buttons (and their signals) of one remote; name and category live in the index.
Result<std::string> serializeRemoteFile(const Remote& remote);
Result<Remote> deserializeRemoteFile(const std::string& text);  // remote.id from the file, name/category empty

// ---- format 1 (read only) ----
Result<LibraryData> deserializeLegacyLibrary(const std::string& text);

// Size in bytes of the text serializeRemoteFile() would produce, without building it (for the heap guard).
size_t estimateRemoteFileBytes(const Remote& remote);

}  // namespace iris
