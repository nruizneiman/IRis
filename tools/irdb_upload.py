#!/usr/bin/env python3
"""Copy Flipper-IRDB `.ir` files to the SD card through the bench firmware (serial), with CRC32 checks.

Usage:  python tools/irdb_upload.py COM5 <root folder> <file> [<file> ...]
        python tools/irdb_upload.py COM5 <root folder> --list files.txt [--skip-existing]
(--skip-existing leaves files alone that are already on the card with the same size and CRC32: use it to resume.)
Each file is stored on the card as /irdb/<path relative to the root folder, with forward slashes>.
Needs the `bench` firmware running (commands sdput / sdcrc). Does not reset the board.
"""
import os
import re
import sys
import time
import zlib

import serial

CHUNK = 256
MAX_SIZE = 65536


def read_until(ser, wanted, timeout):
    """Read lines until one starts with a word in `wanted`; return (word, full line)."""
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        # read(n) blocks for the whole serial timeout unless n bytes arrive: take what is there, or one byte
        buf += ser.read(ser.in_waiting or 1)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", "replace").strip()
            for w in wanted:
                if text.startswith(w):
                    return w, text
    return None, buf.decode("utf-8", "replace")


def already_there(ser, local, remote):
    """True when the card already holds this exact file (same size and CRC32)."""
    data = open(local, "rb").read()
    ser.reset_input_buffer()
    ser.write(("sdcrc %s\n" % remote).encode())
    word, text = read_until(ser, ["CRC", "ERR"], 5)
    m = re.search(r"size=(\d+) crc=([0-9A-Fa-f]{8})", text)
    return word == "CRC" and bool(m) and int(m.group(1)) == len(data) and int(m.group(2), 16) == (zlib.crc32(data) & 0xFFFFFFFF)


def upload(ser, local, remote):
    data = open(local, "rb").read()
    if not data or len(data) > MAX_SIZE:
        return False, "size %d outside 1..%d" % (len(data), MAX_SIZE)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    ser.reset_input_buffer()
    ser.write(("sdput %d %08x %s\n" % (len(data), crc, remote)).encode())
    word, text = read_until(ser, ["READY", "ERR"], 10)
    if word != "READY":
        return False, text or "no answer"
    for off in range(0, len(data), CHUNK):
        ser.write(data[off:off + CHUNK])
        ser.flush()
        got = ser.read(1)
        if got != b"A":
            return False, "no acknowledgement for the chunk at offset %d (got %r)" % (off, got)
    word, text = read_until(ser, ["OK", "ERR"], 15)
    if word != "OK":
        return False, text
    ser.write(("sdcrc %s\n" % remote).encode())
    word, text = read_until(ser, ["CRC", "ERR"], 10)
    m = re.search(r"crc=([0-9A-Fa-f]{8})", text)
    if word != "CRC" or not m or int(m.group(1), 16) != crc:
        return False, "verification failed: %s (expected crc=%08X)" % (text, crc)
    return True, text


def main():
    argv = [a for a in sys.argv[1:] if a != "--skip-existing"]
    skip_existing = "--skip-existing" in sys.argv[1:]
    if len(argv) < 3:
        print(__doc__)
        return 2
    port, root, rest = argv[0], os.path.abspath(argv[1]), argv[2:]
    if rest[0] == "--list":  # a text file with one path per line (too many files for the command line)
        with open(rest[1], encoding="utf-8") as fh:
            files = [ln.strip() for ln in fh if ln.strip()]
    else:
        files = rest
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout, ser.write_timeout = port, 115200, 2, 5
    ser.dtr = False
    ser.rts = False
    ser.open()
    ok = bad = 0
    for f in files:
        path = os.path.abspath(f)
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        if rel.startswith(".."):
            print("SKIP (outside the root folder):", f)
            bad += 1
            continue
        remote = "/irdb/" + rel
        if skip_existing and already_there(ser, path, remote):
            print("SAME " + rel, flush=True)
            ok += 1
            continue
        good, msg = upload(ser, path, remote)
        print(("OK   " if good else "FAIL ") + rel + "  " + msg, flush=True)
        ok += good
        bad += not good
    ser.close()
    print("done: %d uploaded and verified, %d failed" % (ok, bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
