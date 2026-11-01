#include "domain/SignalValidation.h"

#include "domain/Limits.h"

namespace iris {

namespace {
Status invalid(const char* msg) { return Status::fail(Err::InvalidArgument, msg); }

size_t frameRawEntries(const Frame& f) {
  return f.kind == Frame::Kind::Raw ? f.raw.durationsUs.size() : 0;
}
}  // namespace

Status validateDecodedFrame(const DecodedFrame& frame) {
  const ProtocolInfo* info = findProtocol(frame.protocol);
  if (!info) return invalid("unknown protocol");
  const ProtocolVariant* variant = findVariant(*info, frame.bits);
  if (!variant) return Status::fail(Err::OutOfRange, "unsupported bit count for protocol");
  if (frame.address > maskForBits(variant->addressBits)) {
    return Status::fail(Err::OutOfRange, "address out of range");
  }
  if (frame.command > maskForBits(variant->commandBits)) {
    return Status::fail(Err::OutOfRange, "command out of range");
  }
  if (frame.toggle && !info->hasToggle) return invalid("protocol has no toggle bit");
  if (!frame.state.empty()) return invalid("protocol does not use a state payload");
  return Status::success();
}

Status validateRawFrame(const RawFrame& frame) {
  const size_t n = frame.durationsUs.size();
  if (n == 0) return invalid("raw frame is empty");
  if (n < 2) return invalid("raw frame needs at least a mark and a space");
  if (n > limits::kMaxRawEntries) return Status::fail(Err::LimitReached, "raw frame too long");
  for (uint16_t d : frame.durationsUs) {
    if (d < limits::kMinRawDurationUs) return invalid("raw duration must be > 0");
  }
  if (frame.carrierHz != 0 &&
      (frame.carrierHz < limits::kMinCarrierHz || frame.carrierHz > limits::kMaxCarrierHz)) {
    return Status::fail(Err::OutOfRange, "carrier frequency out of range");
  }
  return Status::success();
}

Status validateFrame(const Frame& frame) {
  return frame.kind == Frame::Kind::Decoded ? validateDecodedFrame(frame.decoded)
                                            : validateRawFrame(frame.raw);
}

Status validateSignal(const IrSignal& s) {
  if (s.recognition == Recognition::Invalid) return invalid("invalid captures cannot be stored");
  if (s.initial.empty() && s.repeat.empty()) return invalid("signal has no frames");
  if (s.initial.size() > limits::kMaxFramesPerList || s.repeat.size() > limits::kMaxFramesPerList) {
    return Status::fail(Err::LimitReached, "too many frames");
  }
  if (s.repeatCount > limits::kMaxRepeatCount) return Status::fail(Err::OutOfRange, "repeat count too large");
  if (s.repeatCount > 0 && s.repeat.empty()) return invalid("repeat count without repeat frames");
  if (s.repeatGapUs > limits::kMaxRepeatGapUs) return Status::fail(Err::OutOfRange, "repeat gap too large");
  for (const Frame& f : s.initial) {
    Status st = validateFrame(f);
    if (!st.ok()) return st;
  }
  for (const Frame& f : s.repeat) {
    Status st = validateFrame(f);
    if (!st.ok()) return st;
  }
  if (s.originalCapture) {
    Status st = validateRawFrame(*s.originalCapture);
    if (!st.ok()) return st;
  }
  return Status::success();
}

size_t rawEntryCount(const IrSignal& s) {
  size_t n = s.originalCapture ? s.originalCapture->durationsUs.size() : 0;
  for (const Frame& f : s.initial) n += frameRawEntries(f);
  for (const Frame& f : s.repeat) n += frameRawEntries(f);
  return n;
}

CaptureAnalysis analyzeCapture(const std::vector<uint16_t>& d) {
  CaptureAnalysis out;
  if (d.empty()) {
    out.verdict = Recognition::Invalid;
    out.quality.empty = true;
    out.status = invalid("empty capture");
    return out;
  }
  if (d.size() < 4) {
    out.verdict = Recognition::Invalid;
    out.status = invalid("capture too short");
    return out;
  }
  if (d.size() >= limits::kMaxRawEntries) out.quality.truncated = true;
  size_t shortCount = 0;
  for (uint16_t v : d) {
    if (v < 100) ++shortCount;
  }
  if (shortCount * 10 > d.size() * 3) out.quality.possibleNoise = true;
  out.verdict = Recognition::Unknown;  // decoding is decided by the codec adapter, not here
  return out;
}

}  // namespace iris
