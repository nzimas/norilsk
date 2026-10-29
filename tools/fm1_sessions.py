#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""List, save and restore NORILSK's sessions on an FM-1 over USB-MIDI.

  fm1_sessions.py                 the sessions on the FM-1: number, bytes
  fm1_sessions.py save N [DIR]    session N to DIR/session_N.bin (default: build/)
  fm1_sessions.py restore FILE [N]  a saved session back onto the FM-1, under its own number or as session N
                                  (replacing the one there; the FM-1 gives the record that number as it commits it)
  fm1_sessions.py diag            what the firmware sees of its flash (build 23): each sector's state, the last write

The protocol is the firmware's backup.c (Tonitor's backup protocol, reading only): the installer's framing,
F0 pack7(00 59 cmd len24 body ~sum) F7; 0x50 the list, 0x51 a piece of a record; 0x52 .. 0x54 a record written (build 21). Only an FM-1 that answers the
handshake as NORILSK (FM-1_7xx) is asked. The saved file is the record as the flash holds it: build/host replay
plays it. Needs mido (as fm1_install.py).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fm1_install import MidoBackend, Updater, pack7, unpack7   # noqa: E402

PIECE = 256
SET_NUM = 1000                                          # GLOBAL SETUP's settings (build 43): a record, not a session
PRE_NUM = 1001                                          # the presets (build 47): a record, not a session


def frame(cmd, body=b""):
    n = len(body)
    m = bytearray([0x00, 0x59, cmd, n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF]) + body
    m.append(~sum(body) & 0xFF)
    return b"\xF0" + pack7(bytes(m)) + b"\xF7"


def ask(link, cmd, body=b"", tries=3):
    """-> the answer's body, or None"""
    for _ in range(tries):
        link.drain()
        if not link.send(frame(cmd, body)):
            return None
        while True:
            p = link.read(2.0)
            if p is None:
                break
            d = unpack7(p[1:-1])
            if len(d) < 7 or d[:3] != bytes([0x00, 0x59, cmd]):
                continue
            n = d[3] | d[4] << 8 | d[5] << 16
            if len(d) < 7 + n or ~sum(d[6:6 + n]) & 0xFF != d[6 + n]:
                continue
            return d[6:6 + n]
    return None


def listing(link):
    b = ask(link, 0x50)
    if b is None:
        sys.exit("the FM-1 did not answer the list")
    return [(b[1 + k * 6] | b[2 + k * 6] << 8, int.from_bytes(b[3 + k * 6:7 + k * 6], "little")) for k in range(b[0])]


def read(link, num, length):
    out = bytearray()
    while len(out) < length:
        q = bytes([num & 0xFF, num >> 8]) + len(out).to_bytes(4, "little") + bytes([PIECE & 0xFF, PIECE >> 8])
        b = ask(link, 0x51, q)
        if b is None or len(b) < 8 or b[:6] != q[:6]:
            sys.exit(f"session {num}: no answer at byte {len(out)}")
        n = b[6] | b[7] << 8
        if not n or len(b) != 8 + n:
            sys.exit(f"session {num}: a short piece at byte {len(out)}")
        out += b[8:]
    return bytes(out)


STATUS = ["done", "the FM-1 has no flash store", "not as asked", "no room left on the FM-1",
          "not a whole record (a damaged file?)", "writing the flash failed"]


def restore(link, data, num=None):
    if data[:4] != b"MOL1" or len(data) < 32:
        sys.exit("not a NORILSK session file")
    if num is None:
        num = data[4] | data[5] << 8
    if not (1 <= num <= 64 or num in (SET_NUM, PRE_NUM)):
        sys.exit(f"no session slot {num} (1 .. 64; {SET_NUM}: the global setup; {PRE_NUM}: the presets)")
    if num != (data[4] | data[5] << 8):                 # another number: the head renumbered, its CRC made again (the
        data = bytearray(data)                          # FM-1 takes a record only under the number its head says)
        data[4], data[5] = num & 0xFF, num >> 8
        crc = 0
        for byte in data[:24]:                          # (sessions.c ses_crc: CRC16, poly 0x1021, from 0)
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
                crc &= 0xFFFF
        data[24], data[25] = crc & 0xFF, crc >> 8
        data = bytes(data)
    for cmd, body in [(0x52, bytes([num & 0xFF, num >> 8]) + len(data).to_bytes(4, "little"))] + \
            [(0x53, off.to_bytes(4, "little") + data[off:off + PIECE]) for off in range(0, len(data), PIECE)] + [(0x54, b"")]:
        b = ask(link, cmd, body)
        if b is None or len(b) != 1:
            sys.exit(f"session {num}: no answer (an older firmware answers 2: update it)")
        if b[0]:
            step = {0x52: "making room", 0x53: f"writing at byte {int.from_bytes(body[:4], 'little')}", 0x54: "checking"}[cmd]
            sys.exit(f"session {num}: {STATUS[b[0]] if b[0] < len(STATUS) else b[0]} ({step})")
    print(f"session {num}: {len(data)} B restored")


def main():
    args = sys.argv[1:]
    dev = Updater(MidoBackend()).find(want=lambda i: i.model == "FM-1" and 700 <= i.version <= 799 and not i.loader)
    if dev and not re.search(r"norilsk|molecular", dev.name, re.I):   # (Tonitor and Bigorna answer FM-1_700 too)
        dev.link.close()
        dev = None
    if not dev:
        sys.exit("no FM-1 running NORILSK found (its USB name: FM-1 Norilsk)")
    try:
        print(f"{dev.id.text} on {dev.name}")
        recs = listing(dev.link)
        if not args:
            for num, n in recs:
                print(f"  {'global setup' if num == SET_NUM else 'presets' if num == PRE_NUM else 'session'} {num}: {n} B")
            if not recs:
                print("  no sessions")
            return
        if args[0] == "diag":
            b = ask(dev.link, 0x55)
            if b is None or len(b) < 73 + 8 + 64:
                sys.exit("no diagnosis (a firmware older than build 23)")
            names = "?BUMO"                           # SC_UNKNOWN BLANK USED MINE OTHER
            print("sectors (0x97000 + n x 4 KiB): " + "".join(names[x] if x < 5 else "!" for x in b[:73]))
            s0, ns = (b[73] if b[73] < 128 else b[73] - 256), b[74]
            print(f"last placing: sector {s0}, {ns} sectors, states before {[names[x] if x < 5 else x for x in b[75:75 + ns]]}, "
                  f"erased mask {b[79]:#04x}; sessions {b[80]}")
            want, got = b[81:113], b[113:145]
            print("last commit head meant:", want.hex())
            print("              read back:", got.hex(), "(same)" if want == got else "(DIFFERENT)")
            return
        if args[0] == "restore" and len(args) in (2, 3):
            with open(args[1], "rb") as f:
                restore(dev.link, f.read(), int(args[2]) if len(args) == 3 else None)
            return
        if args[0] != "save" or len(args) < 2:
            sys.exit(__doc__)
        num = int(args[1])
        d = args[2] if len(args) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
        length = dict(recs).get(num)
        if length is None:
            sys.exit(f"no session {num} on the FM-1")
        data = read(dev.link, num, length)
        if data[:4] != b"MOL1":
            sys.exit(f"session {num}: not a NORILSK record")
        os.makedirs(d, exist_ok=True)
        path = os.path.join(d, f"session_{num}.bin")
        with open(path, "wb") as f:
            f.write(data)
        print(f"session {num}: {len(data)} B -> {os.path.normpath(path)}")
    finally:
        dev.link.close()


if __name__ == "__main__":
    main()
