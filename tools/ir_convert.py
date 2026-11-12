#!/usr/bin/env python3
"""Convert IR codes from other sources into Flipper `.ir` files, leaving out what Flipper-IRDB already has.

Usage:
  python tools/ir_convert.py INPUT [INPUT ...] -o OUTDIR [--irdb FLIPPER_IRDB] [--keep-known]

INPUT is a file or a folder (searched recursively). The format is taken from the content:
  Flipper `.ir`     for example what the device exports to /iris/export
  LIRC `.conf`      SPACE_ENC, RC5 / RC6 / SHIFT_ENC and RAW_CODES remotes
  Pronto hex text   one code per line, `Name: 0000 006D ...` (also `=`, `,` or a tab); learned codes (0000, 0100)
                    and the short RC5 (5000) and RC6 (6000) forms

A signal whose timings match a protocol Flipper knows is written as `parsed`, anything else as `raw`.

With --irdb, every signal is looked up in a Flipper-IRDB checkout. A remote whose signals are all in one file of the
database is a duplicate and is not written (--keep-known writes it anyway). The others are written whole, and the
report says how many of their signals the database already has and where.

Output: OUTDIR/<folders of the input>/<remote>.ir. A summary goes to stdout. Exit code 0 unless nothing could be read.
"""
import os
import re
import sys

MAX_RAW = 1024          # entries the firmware accepts in one raw signal
FRAME_GAP_US = 10000    # a longer space separates two frames

# ---------------------------------------------------------------------------------------------------------------------
# Protocol decoding: timings -> (protocol, address, command), with the field meaning of Flipper's own decoders.
# ---------------------------------------------------------------------------------------------------------------------

PULSE_DISTANCE = {  # header mark, header space, bit mark, one space, zero space, allowed bit counts
    "nec": (9000, 4500, 560, 1690, 560, (32, 42)),
    "samsung": (4500, 4500, 550, 1650, 550, (32,)),
    "kaseikyo": (3456, 1728, 432, 1296, 432, (48,)),
    "rca": (4000, 4000, 500, 2000, 1000, (24,)),
    "pioneer": (8500, 4225, 500, 1500, 500, (32,)),
}


def near(value, target, tolerance):
    return abs(value - target) <= tolerance


def header_tolerance(target):
    return min(240, target * 0.2)  # tight enough to tell NEC (9000) from Pioneer (8500)


def bit_tolerance(target):
    return max(150, target * 0.25)


def split_frames(durations):
    """Cut a mark/space list at the long spaces. Every frame starts and ends with a mark."""
    frames, cur = [], []
    for i, d in enumerate(durations):
        if i % 2 == 1 and d >= FRAME_GAP_US:
            frames.append(cur)
            cur = []
        else:
            cur.append(d)
    if cur:
        frames.append(cur)
    return [f[:-1] if len(f) % 2 == 0 else f for f in frames if f]


def decode_pulse_distance(frame, spec):
    hm, hs, bm, one, zero, counts = spec
    if len(frame) < 5 or len(frame) % 2 == 0:
        return None
    bits = (len(frame) - 3) // 2
    if bits not in counts:
        return None
    if not near(frame[0], hm, header_tolerance(hm)) or not near(frame[1], hs, header_tolerance(hs)):
        return None
    value = 0
    for i in range(bits):
        mark, space = frame[2 + 2 * i], frame[3 + 2 * i]
        if not near(mark, bm, bit_tolerance(bm)):
            return None
        if near(space, one, bit_tolerance(one)):
            value |= 1 << i
        elif not near(space, zero, bit_tolerance(zero)):
            return None
    if not near(frame[-1], bm, bit_tolerance(bm)):
        return None
    return bits, value


def decode_nec(frame):
    r = decode_pulse_distance(frame, PULSE_DISTANCE["nec"])
    if not r:
        return None
    bits, v = r
    if bits == 32:
        b = [(v >> (8 * i)) & 0xFF for i in range(4)]
        if b[1] == (~b[0] & 0xFF) and b[3] == (~b[2] & 0xFF):
            return "NEC", b[0], b[2]
        return "NECext", b[0] | (b[1] << 8), b[2] | (b[3] << 8)
    address, address_inv = v & 0x1FFF, (v >> 13) & 0x1FFF
    command, command_inv = (v >> 26) & 0xFF, (v >> 34) & 0xFF
    if address == (~address_inv & 0x1FFF) and command == (~command_inv & 0xFF):
        return "NEC42", address, command
    return "NEC42ext", address | (address_inv << 13), command | (command_inv << 8)


def decode_samsung(frame):
    r = decode_pulse_distance(frame, PULSE_DISTANCE["samsung"])
    if not r:
        return None
    b = [(r[1] >> (8 * i)) & 0xFF for i in range(4)]
    if b[0] == b[1] and b[3] == (~b[2] & 0xFF):
        return "Samsung32", b[0], b[2]
    return None


def decode_kaseikyo(frame):
    r = decode_pulse_distance(frame, PULSE_DISTANCE["kaseikyo"])
    if not r:
        return None
    b = [(r[1] >> (8 * i)) & 0xFF for i in range(6)]
    vendor_parity = b[0] ^ b[1]
    vendor_parity = (vendor_parity & 0xF) ^ (vendor_parity >> 4)
    if (b[2] & 0xF) != vendor_parity or b[5] != (b[2] ^ b[3] ^ b[4]):
        return None
    vendor, genre1, genre2 = b[0] | (b[1] << 8), b[2] >> 4, b[3] & 0xF
    command, ident = (b[3] >> 4) | ((b[4] & 0x3F) << 4), b[4] >> 6
    return "Kaseikyo", (ident << 24) | (vendor << 8) | (genre1 << 4) | genre2, command


def decode_rca(frame):
    r = decode_pulse_distance(frame, PULSE_DISTANCE["rca"])
    if not r:
        return None
    v = r[1]
    address, command = v & 0xF, (v >> 4) & 0xFF
    if address == (~(v >> 12) & 0xF) and command == (~(v >> 16) & 0xFF):
        return "RCA", address, command
    return None


def decode_pioneer(frame):
    r = decode_pulse_distance(frame, PULSE_DISTANCE["pioneer"])
    if not r:
        return None
    b = [(r[1] >> (8 * i)) & 0xFF for i in range(4)]
    if b[1] == (~b[0] & 0xFF) and b[3] == (~b[2] & 0xFF):
        return "Pioneer", b[0], b[2]
    return None


def decode_sirc(frame):
    if len(frame) < 5 or len(frame) % 2 == 0 or not near(frame[0], 2400, 300) or not near(frame[1], 600, 200):
        return None
    bits = (len(frame) - 1) // 2
    if bits not in (12, 15, 20):
        return None
    value = 0
    for i in range(bits):
        mark = frame[2 + 2 * i]
        if near(mark, 1200, 250):
            value |= 1 << i
        elif not near(mark, 600, 250):
            return None
        if i + 1 < bits and not near(frame[3 + 2 * i], 600, 250):
            return None
    return {12: "SIRC", 15: "SIRC15", 20: "SIRC20"}[bits], value >> 7, value & 0x7F


def half_bits(frame, unit, max_units):
    """Expand a mark/space list into half-bit levels (1 mark, 0 space) for a Manchester code."""
    out = []
    for i, d in enumerate(frame):
        n = int(round(d / unit))
        if n < 1 or n > max_units or abs(d - n * unit) > unit * 0.35:
            return None
        out.extend([1 if i % 2 == 0 else 0] * n)
    return out


def decode_rc5(frame):
    levels = half_bits(frame, 889, 2)
    if levels is None:
        return None
    levels = [0] + levels  # the first half of the start bit is a space nobody can see
    if len(levels) == 27:
        levels.append(0)   # so is the last half when the frame ends with a zero
    if len(levels) != 28:
        return None
    bits = []
    for i in range(14):
        pair = (levels[2 * i], levels[2 * i + 1])
        if pair == (0, 1):
            bits.append(1)
        elif pair == (1, 0):
            bits.append(0)
        else:
            return None
    if bits[0] != 1:
        return None
    address = int("".join(map(str, bits[3:8])), 2)
    command = int("".join(map(str, bits[8:14])), 2)
    return ("RC5" if bits[1] else "RC5X"), address, command


def decode_rc6(frame):
    if len(frame) < 6 or not near(frame[0], 2666, 300) or not near(frame[1], 889, 200):
        return None
    levels = half_bits(frame[2:], 444, 3)
    if levels is None:
        return None
    if len(levels) == 43:
        levels.append(0)
    if len(levels) != 44:  # 20 normal bits and the toggle bit, which is twice as long
        return None
    bits, pos = [], 0
    for i in range(21):
        width = 2 if i == 4 else 1
        first, second = levels[pos:pos + width], levels[pos + width:pos + 2 * width]
        pos += 2 * width
        if all(first) and not any(second):
            bits.append(1)
        elif not any(first) and all(second):
            bits.append(0)
        else:
            return None
    if bits[0] != 1 or any(bits[1:4]):  # start bit and mode 0
        return None
    return "RC6", int("".join(map(str, bits[5:13])), 2), int("".join(map(str, bits[13:21])), 2)


DECODERS = (decode_nec, decode_samsung, decode_kaseikyo, decode_rca, decode_pioneer, decode_sirc, decode_rc6, decode_rc5)


def is_nec_repeat(frame):
    return len(frame) == 3 and near(frame[0], 9000, 300) and near(frame[1], 2250, 300)


def decode_timings(durations):
    """(protocol, address, command) when the whole signal is one message of a known protocol, else None."""
    frames = split_frames(durations)
    if not frames:
        return None
    first = None
    for decoder in DECODERS:
        first = decoder(frames[0])
        if first:
            break
    if not first:
        return None
    for frame in frames[1:]:  # only copies of the message or NEC repeat marks may follow
        if first[0].startswith("NEC") and is_nec_repeat(frame):
            continue
        if not any(decoder(frame) == first for decoder in DECODERS):
            return None
    return first


# ---------------------------------------------------------------------------------------------------------------------
# Signals. A signal is ("parsed", protocol, address, command) or ("raw", frequency, [durations]).
# ---------------------------------------------------------------------------------------------------------------------

def make_raw(frequency, durations):
    durations = [min(65535, max(1, int(round(d)))) for d in durations]
    if len(durations) % 2 == 0:
        durations = durations[:-1]  # a trailing space carries nothing
    parsed = decode_timings(durations)
    if parsed:
        return ("parsed",) + parsed
    if len(durations) > MAX_RAW:  # keep the whole frames that fit
        kept, total = [], 0
        for i, frame in enumerate(split_frames(durations)):
            if total + len(frame) + (1 if kept else 0) > MAX_RAW:
                break
            total += len(frame) + (1 if kept else 0)
            kept.append(frame)
        if not kept:
            return None
        flat, pos = [], 0
        for frame in kept:
            if flat:
                flat.append(durations[pos])
                pos += 1
            flat.extend(durations[pos:pos + len(frame)])
            pos += len(frame)
        durations = flat
    return ("raw", int(frequency) if frequency else 38000, durations)


def signal_key(signal):
    """What two copies of the same signal have in common, whoever captured them."""
    if signal[0] == "parsed":
        return signal[1:]
    durations = signal[2]
    shortest = sorted(durations)[:max(1, len(durations) // 4)]
    unit = max(1.0, shortest[len(shortest) // 2])
    return ("raw",) + tuple(min(60, int(round(d / unit))) for d in durations)


# ---------------------------------------------------------------------------------------------------------------------
# Readers. Each returns a list of (remote name, [(button name, signal)]) and appends its complaints to `problems`.
# ---------------------------------------------------------------------------------------------------------------------

def le_bytes(text):
    parts = text.split()
    if not 1 <= len(parts) <= 4:
        raise ValueError(text)
    return sum(int(p, 16) << (8 * i) for i, p in enumerate(parts))


def read_flipper(text, name, problems):
    buttons, cur = [], None

    def flush():
        if not cur or "name" not in cur:
            return
        try:
            if cur.get("type") == "parsed":
                signal = ("parsed", cur["protocol"], le_bytes(cur["address"]), le_bytes(cur["command"]))
            elif cur.get("type") == "raw":
                signal = make_raw(int(cur["frequency"]), [int(x) for x in cur["data"].split()])
            else:
                signal = None
        except (KeyError, ValueError):
            signal = None
        if signal:
            buttons.append((cur["name"], signal))
        else:
            problems.append("%s: button '%s' could not be read" % (name, cur["name"]))

    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        key, value = [p.strip() for p in line.split(":", 1)]
        if key == "name":
            flush()
            cur = {"name": value}
        elif cur is not None:
            cur[key] = value
    flush()
    return [(name, buttons)] if buttons else []


LIRC_KEYS = {"POWER": "Power", "VOLUMEUP": "Vol_up", "VOLUMEDOWN": "Vol_dn", "CHANNELUP": "Ch_next",
             "CHANNELDOWN": "Ch_prev", "MUTE": "Mute"}
LIRC_FLAGS_OK = {"SPACE_ENC", "RC5", "RC6", "SHIFT_ENC", "RAW_CODES", "CONST_LENGTH", "REVERSE", "NO_HEAD_REP",
                 "NO_FOOT_REP", "REPEAT_HEADER"}


def lirc_button_name(name):
    if name.upper().startswith(("KEY_", "BTN_")):
        name = name[4:]
        return LIRC_KEYS.get(name.upper(), name.replace("_", " ").capitalize())
    return name


def reverse_bits(value, bits):
    return int(format(value, "0%db" % bits)[::-1], 2) if bits else 0


def lirc_timings(remote, code):
    """The mark/space list LIRC would send for one code (first frame only; the toggle bit is left as stored)."""
    def pair(key):
        v = remote.get(key, "0 0").split()
        return int(v[0]), (int(v[1]) if len(v) > 1 else 0)

    def number(key):
        return int(remote.get(key, "0").split()[0], 0)

    flags = remote["flags"]
    biphase = bool(flags & {"RC5", "RC6", "SHIFT_ENC"})
    fields = [(number("pre_data"), number("pre_data_bits")), (code, number("bits")), (number("post_data"), number("post_data_bits"))]
    if "REVERSE" in flags:
        fields = [(reverse_bits(v, n), n) for v, n in fields]
    total = sum(n for _, n in fields)
    rc6_mask = number("rc6_mask")
    one, zero = pair("one"), pair("zero")
    out = []  # (is mark, microseconds)

    def emit(mark, us):
        if us > 0:
            out.append((mark, us))

    header = pair("header")
    emit(True, header[0])
    emit(False, header[1])
    emit(True, pair("plead")[0])
    done = 0
    for index, (value, bits) in enumerate(fields):
        if index == 1 and number("pre_data_bits"):
            emit(True, pair("pre")[0])
            emit(False, pair("pre")[1])
        if index == 2 and bits:
            emit(True, pair("post")[0])
            emit(False, pair("post")[1])
        for i in range(bits):
            is_one = (value >> (bits - 1 - i)) & 1
            scale = 2 if rc6_mask & (1 << (total - 1 - done)) else 1
            done += 1
            pulse, space = (one if is_one else zero)
            if biphase and is_one:
                emit(False, space * scale)
                emit(True, pulse * scale)
            else:
                emit(True, pulse * scale)
                emit(False, space * scale)
    emit(True, pair("ptrail")[0])
    foot = pair("foot")
    emit(True, foot[0])
    emit(False, foot[1])
    durations = []
    for mark, us in out:
        if not durations and not mark:
            continue
        if durations and (len(durations) % 2 == 1) == mark:
            durations[-1] += us
        else:
            durations.append(us)
    return durations


def read_lirc(text, name, problems):
    remotes = []
    remote, section, raw_name, raw_values = None, None, None, []
    buttons = []

    def end_raw():
        if raw_name and raw_values:
            signal = make_raw(int(remote.get("frequency", "38000").split()[0] or 38000), raw_values)
            if signal:
                buttons.append((lirc_button_name(raw_name), signal))

    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        words = line.split()
        low = [w.lower() for w in words]
        if low[:2] == ["begin", "remote"]:
            remote, section, buttons = {"flags": set()}, None, []
        elif remote is None:
            continue
        elif low[:2] == ["end", "remote"]:
            label = remote.get("name", name)
            bad = remote["flags"] - LIRC_FLAGS_OK
            if bad:
                problems.append("%s: remote '%s' uses %s, which is not converted" % (name, label, "|".join(sorted(bad))))
            elif buttons:
                remotes.append((label, buttons))
            remote = None
        elif low[:2] in (["begin", "codes"], ["begin", "raw_codes"]):
            section = low[1]
        elif low[:2] in (["end", "codes"], ["end", "raw_codes"]):
            end_raw()
            section, raw_name, raw_values = None, None, []
        elif section == "codes":
            if remote["flags"] - LIRC_FLAGS_OK or len(words) < 2:
                continue
            try:
                durations = lirc_timings(remote, int(words[1], 0))
            except (ValueError, KeyError):
                problems.append("%s: code '%s' could not be read" % (name, words[0]))
                continue
            signal = make_raw(int(remote.get("frequency", "38000").split()[0]), durations) if durations else None
            if signal:
                buttons.append((lirc_button_name(words[0]), signal))
        elif section == "raw_codes":
            if low[0] == "name" and len(words) > 1:
                end_raw()
                raw_name, raw_values = words[1], []
            else:
                try:
                    raw_values.extend(int(w) for w in words)
                except ValueError:
                    pass
        elif low[0] == "flags":
            remote["flags"] = set(f.strip().upper() for f in " ".join(words[1:]).split("|") if f.strip())
        else:
            remote[low[0]] = " ".join(words[1:])
    return remotes


HEX_WORD = re.compile(r"^[0-9A-Fa-f]{4}$")


def pronto_signal(words):
    kind = words[0]
    if kind in (0x5000, 0x6000):  # short forms: system and command follow the four header words
        if len(words) < 6:
            return None
        system, command = words[4], words[5]
        if kind == 0x6000:
            return ("parsed", "RC6", system & 0xFF, command & 0xFF)
        return ("parsed", "RC5" if command < 64 else "RC5X", system & 0x1F, command & 0x3F)
    if kind not in (0x0000, 0x0100) or len(words) < 4 or words[1] == 0:
        return None
    frequency = 1000000.0 / (words[1] * 0.241246)
    once, repeat = words[2] * 2, words[3] * 2
    body = words[4:4 + once] if once else words[4:4 + repeat]  # the repeat part only when nothing is sent once
    if len(body) < 2 or len(words) < 4 + once + repeat:
        return None
    return make_raw(round(frequency), [n * 1000000.0 / frequency for n in body])


def pronto_length(words):
    """How many words the code that starts with these has, or 0 when the header is not complete yet."""
    if len(words) < 4:
        return 0
    if words[0].upper() in ("5000", "6000"):
        return 6
    return 4 + 2 * (int(words[2], 16) + int(words[3], 16))


def read_pronto(text, name, problems):
    buttons = []
    state = {"name": None, "words": []}

    def flush():
        if state["words"]:
            label = state["name"] or "Button %d" % (len(buttons) + 1)
            signal = pronto_signal([int(w, 16) for w in state["words"]])
            if signal:
                buttons.append((label, signal))
            else:
                problems.append("%s: code '%s' is not a Pronto form this tool converts" % (name, label))
            state["name"] = None
        state["words"] = []

    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        words = line.split()
        if all(HEX_WORD.match(w) for w in words):
            expected = pronto_length(state["words"])
            if state["words"] and (expected == 0 or len(state["words"]) < expected):
                state["words"].extend(words)  # a code that wraps over several lines
            else:
                flush()
                state["words"] = list(words)
            continue
        flush()
        m = re.match(r"^(.*?)\s*[:=,\t]\s*((?:[0-9A-Fa-f]{4}\s*)+)$", line)
        if m:
            state["name"], state["words"] = m.group(1).strip(), m.group(2).split()
        else:
            state["name"] = line.rstrip(":=").strip()  # a name on its own line: the code follows
    flush()
    return [(name, buttons)] if buttons else []


def read_text(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def read_any(path, problems):
    try:
        text = read_text(path)
    except OSError as e:
        problems.append("%s: %s" % (path, e))
        return []
    name = os.path.splitext(os.path.basename(path))[0]
    if name.endswith(".lircd"):
        name = name[:-6]
    head = text.lstrip()[:200]
    if head.startswith("Filetype: IR signals file") or head.startswith("Filetype: IR library file"):
        return read_flipper(text, name, problems)
    if re.search(r"^\s*begin\s+remote", text, re.M | re.I):
        return read_lirc(text, name, problems)
    if re.search(r"\b(0000|0100|5000|6000)\s+[0-9A-Fa-f]{4}\s+[0-9A-Fa-f]{4}\s+[0-9A-Fa-f]{4}\b", text):
        return read_pronto(text, name, problems)
    return None  # not a format this tool knows


# ---------------------------------------------------------------------------------------------------------------------
# Flipper-IRDB index and output
# ---------------------------------------------------------------------------------------------------------------------

def index_irdb(root):
    """signal key -> set of file numbers, and the list of file paths (relative to the checkout)."""
    index, files = {}, []
    for folder, dirs, names in os.walk(root):
        dirs[:] = sorted(d for d in dirs if not d.startswith("."))
        for file_name in sorted(names):
            if not file_name.endswith(".ir"):
                continue
            full = os.path.join(folder, file_name)
            try:
                text = read_text(full)
            except OSError:
                continue
            number = len(files)
            files.append(os.path.relpath(full, root).replace(os.sep, "/"))
            for _, buttons in read_flipper(text, file_name, []):
                for _, signal in buttons:
                    index.setdefault(signal_key(signal), set()).add(number)
    return index, files


def compare(buttons, index):
    """How many signals the database has, and the file that holds most of them: (known, file number, hits)."""
    known, hits = 0, {}
    for _, signal in buttons:
        found = index.get(signal_key(signal))
        if found:
            known += 1
            for number in found:
                hits[number] = hits.get(number, 0) + 1
    if not hits:
        return 0, None, 0
    best = max(sorted(hits), key=lambda n: hits[n])
    return known, best, hits[best]


def safe_name(name):
    name = re.sub(r"[^A-Za-z0-9_\-. ()]", "_", name).strip(" .")
    return name or "remote"


def write_flipper(path, buttons, source):
    lines = ["Filetype: IR signals file", "Version: 1", "# Converted by tools/ir_convert.py from " + source]
    for name, signal in buttons:
        lines += ["#", "name: " + name]
        if signal[0] == "parsed":
            _, protocol, address, command = signal

            def le(v):
                return " ".join("%02X" % ((v >> (8 * i)) & 0xFF) for i in range(4))

            lines += ["type: parsed", "protocol: " + protocol, "address: " + le(address), "command: " + le(command)]
        else:
            lines += ["type: raw", "frequency: %d" % signal[1], "duty_cycle: 0.330000",
                      "data: " + " ".join(str(d) for d in signal[2])]
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main(argv):
    inputs, out_dir, irdb, keep_known = [], None, None, False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-o", "--out") and i + 1 < len(argv):
            out_dir = argv[i + 1]
            i += 1
        elif a == "--irdb" and i + 1 < len(argv):
            irdb = argv[i + 1]
            i += 1
        elif a == "--keep-known":
            keep_known = True
        elif a.startswith("-"):
            print(__doc__)
            return 2
        else:
            inputs.append(a)
        i += 1
    if not inputs or not out_dir:
        print(__doc__)
        return 2

    index, irdb_files = {}, []
    if irdb:
        print("Reading Flipper-IRDB at %s ..." % irdb, file=sys.stderr)
        index, irdb_files = index_irdb(irdb)
        print("  %d files, %d different signals" % (len(irdb_files), len(index)), file=sys.stderr)

    problems, sources = [], []
    for item in inputs:
        if os.path.isdir(item):
            for folder, dirs, names in os.walk(item):
                dirs[:] = sorted(d for d in dirs if not d.startswith("."))
                for file_name in sorted(names):
                    rel = os.path.relpath(folder, item)
                    sources.append((os.path.join(folder, file_name), "" if rel == "." else rel, True))
        else:
            sources.append((item, "", False))

    written = skipped = total = 0
    used = set()
    for path, rel, from_folder in sources:
        remotes = read_any(path, problems)
        if remotes is None:
            if not from_folder:
                problems.append("%s: not a Flipper, LIRC or Pronto file" % path)
            continue
        for remote_name, buttons in remotes:
            total += 1
            target = os.path.join(out_dir, rel, safe_name(remote_name) + ".ir")
            n = 2
            while os.path.normcase(target) in used:
                target = os.path.join(out_dir, rel, "%s_%d.ir" % (safe_name(remote_name), n))
                n += 1
            known, best, hits = compare(buttons, index) if index else (0, None, 0)
            label = "%-28s %3d signals" % (safe_name(remote_name)[:28], len(buttons))
            if index and hits == len(buttons) and not keep_known:
                skipped += 1
                print("%s  already in Flipper-IRDB: %s" % (label, irdb_files[best]))
                continue
            used.add(os.path.normcase(target))
            write_flipper(target, buttons, os.path.basename(path))
            written += 1
            note = ""
            if index:
                note = "  all new" if not known else "  %d new, %d in the database (closest: %s, %d)" % (
                    len(buttons) - known, known, irdb_files[best], hits)
            print("%s  -> %s%s" % (label, target.replace(os.sep, "/"), note))

    for p in problems:
        print("warning: " + p, file=sys.stderr)
    print("%d remotes read, %d written, %d already in Flipper-IRDB" % (total, written, skipped))
    return 0 if total else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
