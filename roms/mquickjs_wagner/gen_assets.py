#!/usr/bin/env python3
"""
gen_assets.py — Process images/videos in assets/ using FFmpeg into Raw RGBA32 format.
- Photos & Static Images (.png, .jpg, .jpeg, .bmp, etc.) -> WRAWI (width, height, raw RGBA bytes)
- Animations & Videos (.gif, .mp4, .avi, .webm, .mov, etc.) -> WRAWV (width, height, frame_count, fps, raw RGBA bytes)
- Scripts/Text/Other files -> raw byte arrays with null-terminator.
"""

import os
import sys
import subprocess
import struct
import tempfile

ASSETS_DIR = "assets"
OUTPUT     = "assets.h"

IMAGE_EXTS = {'.png', '.jpg', '.jpeg', '.bmp', '.tga', '.webp'}
VIDEO_EXTS = {'.gif', '.mp4', '.avi', '.webm', '.mov', '.mkv'}

def convert_media_with_ffmpeg(filepath):
    ext = os.path.splitext(filepath)[1].lower()
    
    if ext in IMAGE_EXTS:
        # Use FFmpeg to decode single frame to raw rgba
        cmd = [
            'ffmpeg', '-y', '-loglevel', 'error',
            '-i', filepath,
            '-f', 'image2pipe',
            '-pix_fmt', 'rgba',
            '-vcodec', 'rawvideo', '-'
        ]
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        raw_bytes, err = p.communicate()
        if p.returncode != 0:
            print(f"[gen_assets] FFmpeg error processing image {filepath}: {err.decode()}")
            return None
        
        # Probe dimensions via ffprobe
        probe_cmd = [
            'ffprobe', '-v', 'error',
            '-select_streams', 'v:0',
            '-show_entries', 'stream=width,height',
            '-of', 'csv=s=x:p=0', filepath
        ]
        dim_str = subprocess.check_output(probe_cmd).decode().strip()
        width, height = map(int, dim_str.split('x'))
        
        # Header: Magic "WRAWI" (5 bytes) + width (uint32_le) + height (uint32_le)
        header = b'WRAWI' + struct.pack('<II', width, height)
        return header + raw_bytes

    elif ext in VIDEO_EXTS:
        # Probe dimensions and framerate via ffprobe
        probe_cmd = [
            'ffprobe', '-v', 'error',
            '-select_streams', 'v:0',
            '-show_entries', 'stream=width,height,r_frame_rate,nb_read_frames',
            '-count_frames',
            '-of', 'csv=p=0', filepath
        ]
        probe_out = subprocess.check_output(probe_cmd).decode().strip().split('\n')
        # Expecting width,height / r_frame_rate / nb_read_frames or similar
        # Fallback ffprobe parsing
        probe_cmd2 = [
            'ffprobe', '-v', 'error',
            '-select_streams', 'v:0',
            '-show_entries', 'stream=width,height,r_frame_rate',
            '-of', 'csv=s=,:p=0', filepath
        ]
        parts = subprocess.check_output(probe_cmd2).decode().strip().split(',')
        width = int(parts[0])
        height = int(parts[1])
        fps_num, fps_den = map(int, parts[2].split('/')) if '/' in parts[2] else (int(parts[2]), 1)
        fps = int(fps_num / fps_den) if fps_den > 0 else 30

        # Decode all frames to rawvideo pipe
        cmd = [
            'ffmpeg', '-y', '-loglevel', 'error',
            '-i', filepath,
            '-f', 'image2pipe',
            '-pix_fmt', 'rgba',
            '-vcodec', 'rawvideo', '-'
        ]
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        raw_bytes, err = p.communicate()
        if p.returncode != 0:
            print(f"[gen_assets] FFmpeg error processing video/gif {filepath}: {err.decode()}")
            return None
        
        frame_size = width * height * 4
        frame_count = len(raw_bytes) // frame_size
        
        # Header: Magic "WRAWV" (5 bytes) + width (uint32) + height (uint32) + frame_count (uint32) + fps (uint32)
        header = b'WRAWV' + struct.pack('<IIII', width, height, frame_count, fps)
        return header + raw_bytes

    return None

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
        
        # Check if file needs FFmpeg RAW conversion
        raw_data = convert_media_with_ffmpeg(full)
        if raw_data is None:
            # Regular text or script asset
            with open(full, "rb") as f:
                raw_data = f.read()
        
        entries.append((rel, raw_data))

lines = []
lines.append("#pragma once")
lines.append("typedef struct { const char* path; const unsigned char* data; unsigned int size; } WagnosticAsset;")

for idx, (path, data) in enumerate(entries):
    var = f"asset_{idx}"
    rows = []
    data_with_null = data + b'\x00'
    flat = [f"0x{b:02x}" for b in data_with_null]
    for i in range(0, len(flat), 12):
        rows.append("  " + ", ".join(flat[i:i+12]))
    lines.append(f"static const unsigned char {var}[] = {{")
    lines.append(",\n".join(rows))
    lines.append("};")

lines.append("static const WagnosticAsset WAGNOSTIC_ASSETS[] = {")
for idx, (path, data) in enumerate(entries):
    safe = path.replace("\\", "/")
    lines.append(f'    {{"{safe}", asset_{idx}, {len(data)}}},')
lines.append("};")
lines.append(f"static const int WAGNOSTIC_ASSET_COUNT = {len(entries)};")

with open(OUTPUT, "w") as f:
    f.write("\n".join(lines) + "\n")

print(f"[gen_assets] Generated {OUTPUT} with {len(entries)} asset(s) via FFmpeg RAW Pipeline:")
for path, data in entries:
    print(f"  {path}  ({len(data)} bytes)")
