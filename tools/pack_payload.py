#!/usr/bin/env python3
"""Packs the installer payload.

Reads the source .zip archives (CLEO, SAMPFUNCS, MoonLoader, Silent's ASI Loader,
the UltraFuck script and the fonts), sorts every file into an installer
component, resolves conflicts between archives and writes:

  payload.zip              - one LZMA archive embedded into the exe as RCDATA
  payload_manifest.gen.h   - C++ table of files/dirs/fonts + versions + known hashes
  payload_report.txt       - human readable summary of what was packed / skipped
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import zipfile
import zlib
from dataclasses import dataclass, field
from pathlib import Path

from repack_zip import build_zip, write_file

FONT_EXT = (".ttf", ".otf", ".ttc")
PE_EXT = (".dll", ".asi", ".cleo")
SKIP_NAMES = {"desktop.ini", "thumbs.db"}
REQUIRED_FONTS = {"trebucbd.ttf"}  # imgui.lua / mimgui assert that this file exists in %WINDIR%\Fonts
# Lib files/folders whose stems belong to one logical library ("unit").
UNIT_ALIASES = {
    "socket": "luasocket", "mime": "luasocket", "ltn12": "luasocket", "luasocket": "luasocket",
    "moonimgui": "imgui", "imgui": "imgui",
    "libeffil": "effil", "effil": "effil",
}
ZIP_TIME = (1980, 1, 1, 0, 0, 0)

report: list[str] = []


def note(msg: str) -> None:
    report.append(msg)
    print(f"[pack_payload] {msg}")


def fail(msg: str) -> None:
    print(f"[pack_payload] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


@dataclass
class Archive:
    path: Path
    files: dict[str, bytes] = field(default_factory=dict)   # posix rel path -> data
    empty_dirs: set[str] = field(default_factory=set)
    kind: str = ""


@dataclass
class Item:
    comp: str
    entry: str      # name inside payload.zip
    dest: str       # path relative to the game folder (posix)
    data: bytes
    unit: str = ""


# --------------------------------------------------------------------------- reading

def norm(name: str) -> str:
    name = name.replace("\\", "/")
    while name.startswith("./"):
        name = name[2:]
    return name.lstrip("/")


def read_zip(path: Path) -> Archive:
    arc = Archive(path)
    dirs: set[str] = set()
    with zipfile.ZipFile(path) as zf:
        for info in zf.infolist():
            name = norm(info.filename)
            if not name:
                continue
            if info.is_dir():
                dirs.add(name.rstrip("/"))
                continue
            arc.files[name] = zf.read(info)
    for d in dirs:
        prefix = d + "/"
        if not any(f.startswith(prefix) for f in arc.files) and not any(o != d and o.startswith(prefix) for o in dirs):
            arc.empty_dirs.add(d)
    return arc


# --------------------------------------------------------------------------- validation helpers

def is_i386_pe(data: bytes) -> bool:
    if len(data) < 0x40 or data[:2] != b"MZ":
        return False
    off = struct.unpack_from("<I", data, 0x3C)[0]
    if off + 6 > len(data) or data[off:off + 4] != b"PE\0\0":
        return False
    return struct.unpack_from("<H", data, off + 4)[0] == 0x14C


def ttf_full_name(data: bytes) -> tuple[str, bool] | None:
    """Returns (full font name, is_opentype_cff) or None if data is not a sane sfnt font."""
    if len(data) < 12:
        return None
    tag = data[:4]
    if tag not in (b"\x00\x01\x00\x00", b"true", b"OTTO"):
        return None
    num = struct.unpack_from(">H", data, 4)[0]
    tables = {}
    for i in range(num):
        t, _, off, ln = struct.unpack_from(">4sIII", data, 12 + i * 16)
        if off + ln > len(data):
            return None
        tables[t] = (off, ln)
    if b"name" not in tables:
        return None
    base, _ = tables[b"name"]
    _, count, str_off = struct.unpack_from(">HHH", data, base)
    best = None
    for i in range(count):
        pid, eid, lid, nid, ln, off = struct.unpack_from(">HHHHHH", data, base + 6 + i * 12)
        if nid != 4:
            continue
        raw = data[base + str_off + off: base + str_off + off + ln]
        if pid == 3 and eid in (0, 1) and lid == 0x409:
            return raw.decode("utf-16-be"), tag == b"OTTO"
        if best is None:
            best = raw.decode("utf-16-be" if pid in (0, 3) else "latin-1", "replace")
    return (best, tag == b"OTTO") if best else None


def ascii_after(data: bytes, pattern: bytes) -> str:
    m = re.search(pattern, data)
    return m.group(1).decode("ascii", "replace") if m else ""


def version_resource(data: bytes, key: str = "FileVersion") -> str:
    w = key.encode("utf-16-le")
    i = data.find(w)
    if i < 0:
        return ""
    i += len(w)
    while i + 1 < len(data) and data[i:i + 2] == b"\0\0":
        i += 2
    end = data.find(b"\0\0", i)
    while end > 0 and (end - i) % 2:
        end = data.find(b"\0\0", end + 1)
    return data[i:end].decode("utf-16-le", "replace").strip() if end > i else ""


def unit_of(lib_rel: str) -> str:
    first = lib_rel.split("/")[0]
    stem = first.lower()
    if "/" not in lib_rel:
        stem = os.path.splitext(stem)[0]
    return UNIT_ALIASES.get(stem, stem)


# --------------------------------------------------------------------------- classification

def classify(arc: Archive) -> str:
    names = {n.lower() for n in arc.files}
    if arc.files and all(n.lower().endswith(FONT_EXT) for n in arc.files):
        return "fonts"
    if "moonloader.asi" in names:
        return "ml"
    if "sampfuncs.asi" in names:
        return "sf"
    if "cleo.asi" in names:
        return "cleo"
    if any(re.fullmatch(r"moonloader/ultrafuck[^/]*\.luac?", n) for n in names):
        return "uf"
    if "vorbisfile.dll" in names and "vorbishooked.dll" in names:
        return "asi"
    return ""


def skip_common(name: str) -> bool:
    return os.path.basename(name).lower() in SKIP_NAMES


# --------------------------------------------------------------------------- main packing

def pack(src: Path, out: Path) -> None:
    archives: dict[str, Archive] = {}
    for p in sorted(src.iterdir()):
        if p.suffix.lower() in (".rar", ".7z"):
            fail(f"{p.name}: only .zip archives are supported, convert it: python tools/repack_zip.py <unpacked folder> -o {p.stem}.zip")
        if p.suffix.lower() != ".zip":
            continue
        arc = read_zip(p)
        arc.kind = classify(arc)
        if not arc.kind:
            note(f"WARNING: {p.name}: unknown archive content, ignored")
            continue
        if arc.kind in archives:
            fail(f"two archives of kind '{arc.kind}': {archives[arc.kind].path.name} and {p.name}")
        archives[arc.kind] = arc
        note(f"{p.name}: {arc.kind}, {len(arc.files)} files")

    for need in ("asi", "cleo", "sf", "ml", "uf", "fonts"):
        if need not in archives:
            fail(f"no archive for component '{need}' in {src}")

    items: list[Item] = []
    dirs: list[tuple[str, str]] = []
    lib_candidates: dict[str, list[tuple[str, str, bytes]]] = {}  # unit -> [(source, rel, data)]
    bad_pe: list[str] = []

    def add(comp: str, dest: str, data: bytes, unit: str = "") -> None:
        if dest.lower().endswith(PE_EXT) and not is_i386_pe(data):
            bad_pe.append(dest)
            note(f"SKIPPED broken binary (not an i386 PE): {dest}")
            return
        if not all(ord(c) < 128 for c in dest):
            fail(f"non-ASCII file name: {dest}")
        items.append(Item(comp, f"{comp}/{dest}", dest, data, unit))

    def skipped(arc: Archive, name: str, why: str) -> None:
        note(f"skip {arc.path.name}:{name} ({why})")

    # --- Silent's ASI Loader
    a = archives["asi"]
    for name, data in a.files.items():
        low = name.lower()
        if skip_common(name):
            continue
        if low in ("vorbisfile.dll", "vorbishooked.dll", "scripts/global.ini"):
            add("asi", name, data)
        else:
            skipped(a, name, "documentation/example")

    # --- CLEO
    a = archives["cleo"]
    for name, data in a.files.items():
        low = name.lower()
        if skip_common(name):
            continue
        if low == "cleo.asi" or (low.startswith("cleo/") and low.endswith(".cleo")):
            add("cleo", name, data)
        elif low in ("vorbisfile.dll", "vorbishooked.dll", "scripts/global.ini"):
            skipped(a, name, "ASI loader comes from Silent's ASI Loader archive")
        elif low == "bass.dll":
            skipped(a, name, "older BASS, the MoonLoader archive has a newer one")
        else:
            skipped(a, name, "unexpected")
    for d in a.empty_dirs:
        dirs.append(("cleo", d))

    # --- SAMPFUNCS
    a = archives["sf"]
    for name, data in a.files.items():
        if name.lower() == "sampfuncs.asi":
            add("sf", name, data)
        elif not skip_common(name):
            skipped(a, name, "unexpected")
    for d in a.empty_dirs:
        dirs.append(("sf", d))
    if not any(d.lower() == "sampfuncs" for _, d in dirs):
        dirs.append(("sf", "SAMPFUNCS"))

    # --- MoonLoader
    a = archives["ml"]
    for name, data in a.files.items():
        low = name.lower()
        if skip_common(name):
            continue
        if low in ("moonloader.asi", "lua51.dll"):
            add("ml", name, data)
        elif low == "bass.dll":
            add("bass", name, data)
        elif low.startswith("moonloader/lib/"):
            rel = name[len("moonloader/lib/"):]
            lib_candidates.setdefault(unit_of(rel), []).append(("ml", rel, data))
        elif re.fullmatch(r"moonloader/[^/]+\.lua", low):
            add("mlscripts", name, data)
        elif low in ("vorbisfile.dll", "vorbishooked.dll"):
            skipped(a, name, "ASI loader comes from Silent's ASI Loader archive")
        else:
            skipped(a, name, "unexpected")

    # --- UltraFuck
    a = archives["uf"]
    script_entries = []
    for name, data in a.files.items():
        low = name.lower()
        if skip_common(name):
            continue
        if re.fullmatch(r"moonloader/ultrafuck[^/]*\.luac?", low):
            script_entries.append(name)
            add("script", name, data)
        elif low.startswith("moonloader/lib/"):
            rel = name[len("moonloader/lib/"):]
            lib_candidates.setdefault(unit_of(rel), []).append(("uf", rel, data))
        elif low.startswith("moonloader/"):
            add("config", name, data)
        else:
            skipped(a, name, "outside moonloader/")
    if len(script_entries) != 1:
        fail(f"expected exactly one UltraFuck script in {a.path.name}, got {script_entries}")
    script_dest = script_entries[0]
    m = re.search(r"ultrafuck[ _-]?v?(\d+(?:\.\d+)*)", Path(script_dest).stem, re.I)
    script_version = m.group(1) if m else ""
    m2 = re.search(r"ultrafuck[ _-]?v?(\d+(?:\.\d+)*)", a.path.stem, re.I)
    if m2 and script_version and m2.group(1) != script_version:
        fail(f"version mismatch: archive {a.path.name} vs script {script_dest}")
    script_version = script_version or (m2.group(1) if m2 else "")
    if not script_version:
        fail("cannot determine the UltraFuck version (name the archive UltraFuck_<version>.zip)")

    # --- libraries: one source per unit, MoonLoader's archive wins
    for unit in sorted(lib_candidates):
        cands = lib_candidates[unit]
        sources = {s for s, _, _ in cands}
        chosen = "ml" if "ml" in sources else "uf"
        if len(sources) > 1:
            ml_files = {r: d for s, r, d in cands if s == "ml"}
            diff = [r for s, r, d in cands if s == "uf" and ml_files.get(r) not in (None, d)]
            extra = [r for s, r, d in cands if s == "uf" and r not in ml_files]
            if diff or extra:
                note(f"lib unit '{unit}': using MoonLoader's version (dropped from UF: {sorted(set(diff + extra))})")
        for s, rel, data in cands:
            if s == chosen and not skip_common(rel):
                add("lib", "moonloader/lib/" + rel, data, unit)

    # --- fonts
    fonts = []
    for name, data in sorted(archives["fonts"].files.items()):
        info = ttf_full_name(data)
        if not info:
            fail(f"{name}: not a valid TrueType/OpenType font")
        full, cff = info
        base = os.path.basename(name)
        entry = f"fonts/{base}"
        items.append(Item("fonts", entry, base, data))
        fonts.append((entry, base, f"{full} ({'OpenType' if cff else 'TrueType'})", base.lower() in REQUIRED_FONTS, data))
    if not any(f[3] for f in fonts):
        fail(f"fonts archive does not contain the required {REQUIRED_FONTS}")

    # --- versions and known hashes
    def item_data(comp: str, dest_lower: str) -> bytes:
        for it in items:
            if it.comp == comp and it.dest.lower() == dest_lower:
                return it.data
        fail(f"missing {comp}:{dest_lower}")
        return b""

    ml_ver = ascii_after(item_data("ml", "moonloader.asi"), rb"MoonLoader v\.([0-9][0-9A-Za-z.\-]*)")
    sf_line = ascii_after(item_data("sf", "sampfuncs.asi"), rb"(SAMPFUNCS v[0-9][^\x00]{0,80})")
    sf_ver = ascii_after(sf_line.encode(), rb"SAMPFUNCS v([0-9][0-9A-Za-z.\-]*(?: rel\.\d+)?)")
    sf_target = ascii_after(sf_line.encode(), rb"\(SA-MP ([^)]+)\)")
    cleo_ver = version_resource(item_data("cleo", "cleo.asi"))
    bass_ver = version_resource(item_data("bass", "bass.dll"))
    orig_vorbis = item_data("asi", "vorbishooked.dll")
    silent = item_data("asi", "vorbisfile.dll")
    loaders = {(len(silent), zlib.crc32(silent))}
    for arc_kind in ("cleo", "ml"):
        d = next((v for k, v in archives[arc_kind].files.items() if k.lower() == "vorbisfile.dll"), None)
        if d:
            loaders.add((len(d), zlib.crc32(d)))

    # --- write payload.zip (deterministic, LZMA preset 9e)
    out.mkdir(parents=True, exist_ok=True)
    payload = build_zip([(it.entry, it.data, ZIP_TIME) for it in sorted(items, key=lambda i: i.entry.lower())])
    write_if_changed(out / "payload.zip", payload)

    # --- manifest header
    def cstr(s: str) -> str:
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

    def win(p: str) -> str:
        return p.replace("/", "\\")

    comp_enum = {"asi": "AsiLoader", "bass": "Bass", "cleo": "Cleo", "sf": "Sampfuncs", "ml": "MoonLoader",
                 "mlscripts": "MoonLoaderScripts", "lib": "Lib", "script": "Script", "config": "Config"}
    order = list(comp_enum)
    file_items = sorted((i for i in items if i.comp != "fonts"), key=lambda i: (order.index(i.comp), i.dest.lower()))
    h = ["// Generated by tools/pack_payload.py - do not edit.",
         "#pragma once",
         '#include "core/payload_types.h"',
         "",
         "namespace uf::gen {",
         f"inline constexpr const char kScriptVersion[] = {cstr(script_version)};",
         f"inline constexpr const char kScriptDest[] = {cstr(win(script_dest))};",
         f"inline constexpr const char kMoonLoaderVersion[] = {cstr(ml_ver)};",
         f"inline constexpr const char kSampfuncsVersion[] = {cstr(sf_ver)};",
         f"inline constexpr const char kSampfuncsTarget[] = {cstr(sf_target)};",
         f"inline constexpr const char kCleoVersion[] = {cstr(cleo_ver)};",
         f"inline constexpr const char kBassVersion[] = {cstr(bass_ver)};",
         f"inline constexpr KnownBinary kOriginalVorbisFile = {{{len(orig_vorbis)}u, 0x{zlib.crc32(orig_vorbis):08x}u}};",
         "inline constexpr KnownBinary kSilentLoaders[] = {" +
         ", ".join(f"{{{s}u, 0x{c:08x}u}}" for s, c in sorted(loaders)) + "};",
         "",
         "inline constexpr PayloadFile kFiles[] = {"]
    for it in file_items:
        h.append(f"    {{Comp::{comp_enum[it.comp]}, {cstr(it.entry)}, {cstr(win(it.dest))}, {len(it.data)}u, "
                 f"0x{zlib.crc32(it.data):08x}u, {cstr(it.unit)}}},")
    h.append("};")
    h.append("")
    h.append("inline constexpr PayloadDir kDirs[] = {")
    for comp, d in sorted(dirs):
        h.append(f"    {{Comp::{comp_enum[comp]}, {cstr(win(d))}}},")
    h.append("};")
    h.append("")
    h.append("inline constexpr PayloadFont kFonts[] = {")
    for entry, base, reg, req, data in fonts:
        h.append(f"    {{{cstr(entry)}, {cstr(base)}, {cstr(reg)}, {'true' if req else 'false'}, {len(data)}u, "
                 f"0x{zlib.crc32(data):08x}u}},")
    h.append("};")
    h.append("}  // namespace uf::gen")
    write_if_changed(out / "payload_manifest.gen.h", ("\n".join(h) + "\n").encode())

    total = sum(len(i.data) for i in items)
    note(f"UltraFuck {script_version}, MoonLoader {ml_ver}, SAMPFUNCS {sf_ver} ({sf_target}), CLEO {cleo_ver}, BASS {bass_ver}")
    note(f"{len(items)} files, {total / 1048576:.1f} MiB unpacked -> payload.zip {len(payload) / 1048576:.1f} MiB")
    if bad_pe:
        note(f"broken binaries skipped: {bad_pe}")
    lines = [f"{i.comp:10} {i.dest}  [{i.unit}]" if i.unit else f"{i.comp:10} {i.dest}" for i in sorted(items, key=lambda i: (i.comp, i.dest.lower()))]
    write_if_changed(out / "payload_report.txt", ("\n".join(report) + "\n\n" + "\n".join(lines) + "\n").encode("utf-8"))


def write_if_changed(path: Path, data: bytes) -> None:
    if not path.exists() or path.read_bytes() != data:
        write_file(path, data)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", required=True, type=Path, help="folder with the source archives")
    ap.add_argument("--out", required=True, type=Path, help="output folder for generated files")
    args = ap.parse_args()
    if not args.src.is_dir():
        fail(f"payload folder not found: {args.src}")
    pack(args.src, args.out)


if __name__ == "__main__":
    main()
