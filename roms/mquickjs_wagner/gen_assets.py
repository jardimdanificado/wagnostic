#!/usr/bin/env python3
"""
gen_assets.py — Embeds every file under assets/ into assets.h as C byte arrays.
"""

import os
import sys

ASSETS_DIR = "assets"
OUTPUT     = "assets.h"

entries = []

if not os.path.isdir(ASSETS_DIR):
    print(f"[gen_assets] WARNING: '{ASSETS_DIR}' directory not found, creating empty assets.h")
    with open(OUTPUT, "w") as f:
        f.write("#pragma once\n")
        f.write("typedef struct { const char* path; const unsigned char* data; unsigned int size; } WagnosticAsset;\n")
        f.write("static const WagnosticAsset WAGNOSTIC_ASSETS[] = { {0,0,0} };\n")
        f.write("static const int WAGNOSTIC_ASSET_COUNT = 0;\n")
    sys.exit(0)

for root, dirs, files in os.walk(ASSETS_DIR):
    dirs.sort()
    for name in sorted(files):
        full = os.path.join(root, name)
        rel  = os.path.relpath(full, ASSETS_DIR)
        with open(full, "rb") as f:
            data = f.read()
        entries.append((rel, data))

lines = []
lines.append("#pragma once")
lines.append("typedef struct { const char* path; const unsigned char* data; unsigned int size; } WagnosticAsset;")

for idx, (path, data) in enumerate(entries):
    var = f"asset_{idx}"
    rows = []
    flat = [f"0x{b:02x}" for b in data]
    for i in range(0, len(flat), 12):
        rows.append("  " + ", ".join(flat[i:i+12]))
    lines.append(f"static const unsigned char {var}[] = {{")
    lines.append(",\n".join(rows) if rows else "  0x00")
    lines.append("};")

lines.append("static const WagnosticAsset WAGNOSTIC_ASSETS[] = {")
for idx, (path, _) in enumerate(entries):
    safe = path.replace("\\", "/")
    lines.append(f'    {{"{safe}", asset_{idx}, sizeof(asset_{idx})}},')
lines.append("};")
lines.append(f"static const int WAGNOSTIC_ASSET_COUNT = {len(entries)};")

with open(OUTPUT, "w") as f:
    f.write("\n".join(lines) + "\n")

print(f"[gen_assets] Generated {OUTPUT} with {len(entries)} asset(s):")
for path, data in entries:
    print(f"  {path}  ({len(data)} bytes)")
