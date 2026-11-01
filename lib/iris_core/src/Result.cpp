#include "Result.h"

namespace iris {

const char* errName(Err code) {
  switch (code) {
    case Err::Ok: return "Ok";
    case Err::InvalidArgument: return "InvalidArgument";
    case Err::OutOfRange: return "OutOfRange";
    case Err::LimitReached: return "LimitReached";
    case Err::NotFound: return "NotFound";
    case Err::AlreadyExists: return "AlreadyExists";
    case Err::InUse: return "InUse";
    case Err::Corrupt: return "Corrupt";
    case Err::UnsupportedVersion: return "UnsupportedVersion";
    case Err::StorageFailure: return "StorageFailure";
    case Err::Unavailable: return "Unavailable";
  }
  return "Unknown";
}

}  // namespace iris
