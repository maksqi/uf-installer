#!/usr/bin/env python3
"""Rewrites zip archives with the strongest compression the installer reads: LZMA (zip method 14),
preset 9 + extreme. Also packs a folder into such a zip. Only the standard library is needed.

  python tools/repack_zip.py payload/*.zip                     # recompress in place
  python tools/repack_zip.py D:/fonts -o payload/fonts.zip      # pack a folder

Python's zipfile, 7-Zip and WinRAR open these archives; Windows Explorer does not (it only knows deflate).
pack_payload.py uses build_zip() for payload.zip as well.
"""
from __future__ import annotations

import argparse
import lzma
import os
import struct
import sys
import time
import zipfile
import zlib
from pathlib import Path

METHOD_STORED = 0
METHOD_LZMA = 14
FLAG_LZMA_EOS = 0x0002    # the LZMA stream ends with an end marker
FLAG_UTF8 = 0x0800
LC, LP, PB = 3, 0, 2

# (name, data or None for a directory, zip date_time)
Entry = tuple[str, bytes | None, tuple[int, int, int, int, int, int]]


def lzma_entry(data: bytes) -> bytes:
    """Zip LZMA data: [SDK version:2][props size:2][props:5][raw LZMA1 stream with an end marker]."""
    dict_size = 1 << 12
    while dict_size < len(data) and dict_size < (1 << 26):
        dict_size <<= 1
    filt = {"id": lzma.FILTER_LZMA1, "preset": 9 | lzma.PRESET_EXTREME,
            "dict_size": dict_size, "lc": LC, "lp": LP, "pb": PB}
    props = struct.pack("<BI", (PB * 5 + LP) * 9 + LC, dict_size)
    return struct.pack("<BBH", 9, 20, len(props)) + props + lzma.compress(data, format=lzma.FORMAT_RAW, filters=[filt])


def dos_time(dt: tuple[int, int, int, int, int, int]) -> tuple[int, int]:
    y, mo, d, h, mi, s = dt
    y = min(max(y, 1980), 2107)
    return (h << 11) | (mi << 5) | (s // 2), ((y - 1980) << 9) | (mo << 5) | d


def build_zip(entries: list[Entry]) -> bytes:
    """Builds the archive in memory, entries in the given order. The same input always gives the same bytes."""
    out = bytearray()
    central = bytearray()
    for name, data, dt in entries:
        is_dir = data is None
        raw = b"" if is_dir else data
        packed = b"" if is_dir else lzma_entry(raw)
        if is_dir or len(packed) >= len(raw):
            method, flags, version, packed = METHOD_STORED, 0, 20 if is_dir else 10, raw
        else:
            method, flags, version = METHOD_LZMA, FLAG_LZMA_EOS, 63
        try:
            fname = name.encode("ascii")
        except UnicodeEncodeError:
            fname, flags = name.encode("utf-8"), flags | FLAG_UTF8
        crc = zlib.crc32(raw)
        tm, dm = dos_time(dt)
        offset = len(out)
        out += struct.pack("<IHHHHHIIIHH", 0x04034B50, version, flags, method, tm, dm, crc, len(packed), len(raw),
                           len(fname), 0) + fname + packed
        central += struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, version, version, flags, method, tm, dm, crc,
                               len(packed), len(raw), len(fname), 0, 0, 0, 0, 0x10 if is_dir else 0, offset) + fname
    cd_offset = len(out)
    out += central
    out += struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, len(entries), len(entries), len(central), cd_offset, 0)
    return bytes(out)


def write_file(path: Path, data: bytes) -> None:
    """Atomic write; retries because an antivirus may hold a freshly written file for a moment."""
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_bytes(data)
    for attempt in range(10):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            if attempt == 9:
                raise
            time.sleep(0.3)


def read_entries(src: Path) -> list[Entry]:
    if src.is_dir():
        entries: list[Entry] = []
        for root, dirs, files in os.walk(src):
            dirs.sort()
            rel_root = Path(root).relative_to(src).as_posix()
            prefix = "" if rel_root == "." else rel_root + "/"
            if prefix and not files and not dirs:
                entries.append((prefix, None, time.localtime(os.path.getmtime(root))[:6]))
            for f in sorted(files):
                full = Path(root) / f
                entries.append((prefix + f, full.read_bytes(), time.localtime(full.stat().st_mtime)[:6]))
        return entries
    with zipfile.ZipFile(src) as zf:
        return [(i.filename, None if i.is_dir() else zf.read(i), i.date_time) for i in zf.infolist()]


def verify(data: bytes, entries: list[Entry]) -> None:
    import io
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        got = {i.filename: (None if i.is_dir() else zf.read(i)) for i in zf.infolist()}
    if got != {n: d for n, d, _ in entries}:
        raise SystemExit("verification failed: the new archive differs from the source")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", type=Path, help="zip archives (rewritten in place) or one folder with -o")
    ap.add_argument("-o", "--output", type=Path, help="output zip (for a single input)")
    args = ap.parse_args()
    if args.output and len(args.inputs) != 1:
        ap.error("-o needs exactly one input")
    for src in args.inputs:
        dst = args.output or src
        if src.is_dir() and not args.output:
            ap.error(f"{src} is a folder: pass -o <archive.zip>")
        entries = read_entries(src)
        before = src.stat().st_size if src.is_file() else sum(len(d or b"") for _, d, _ in entries)
        data = build_zip(entries)
        verify(data, entries)
        write_file(dst, data)
        print(f"{dst.name}: {before / 1024:.0f} KiB -> {len(data) / 1024:.0f} KiB ({len(entries)} entries)")


if __name__ == "__main__":
    sys.exit(main())
