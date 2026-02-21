#!/bin/bash
# build_personas.sh — Gzip persona HTML/JS files for LittleFS upload
# Usage: ./build_personas.sh
# Then upload LittleFS data with: pio run -t uploadfs -e esp32s3

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/personas"
OUT_DIR="$SCRIPT_DIR/data/p"

echo "=== Building persona files for LittleFS ==="

# Create output directory
mkdir -p "$OUT_DIR"

# Gzip HTML files
for f in "$SRC_DIR"/*.html; do
    base=$(basename "$f")
    echo "  Gzipping $base..."
    gzip -9 -c "$f" > "$OUT_DIR/${base}.gz"
done

# Gzip JS files
for f in "$SRC_DIR"/*.js; do
    base=$(basename "$f")
    echo "  Gzipping $base..."
    gzip -9 -c "$f" > "$OUT_DIR/${base}.gz"
done

# Copy manifest JSON files (small enough, no gzip needed)
for f in "$SRC_DIR"/manifest-*.json; do
    if [ -f "$f" ]; then
        base=$(basename "$f")
        echo "  Copying $base..."
        cp "$f" "$OUT_DIR/$base"
    fi
done

# Copy icon files
for f in "$SRC_DIR"/icon-*.png; do
    if [ -f "$f" ]; then
        base=$(basename "$f")
        echo "  Copying $base..."
        cp "$f" "$OUT_DIR/$base"
    fi
done

# Generate minimal icons if they don't exist
if [ ! -f "$OUT_DIR/icon-192.png" ]; then
    echo "  Generating placeholder icon-192.png..."
    python3 -c "
import struct, zlib
def make_png(w, h, r, g, b):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    hdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''
    for y in range(h):
        raw += b'\x00' + bytes([r, g, b]) * w
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', hdr) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
with open('$OUT_DIR/icon-192.png', 'wb') as f:
    f.write(make_png(192, 192, 26, 26, 46))
" 2>/dev/null || echo "    (Python not available, icon skipped)"
fi

if [ ! -f "$OUT_DIR/icon-512.png" ]; then
    echo "  Generating placeholder icon-512.png..."
    python3 -c "
import struct, zlib
def make_png(w, h, r, g, b):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    hdr = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    raw = b''
    for y in range(h):
        raw += b'\x00' + bytes([r, g, b]) * w
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', hdr) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
with open('$OUT_DIR/icon-512.png', 'wb') as f:
    f.write(make_png(512, 512, 26, 26, 46))
" 2>/dev/null || echo "    (Python not available, icon skipped)"
fi

echo ""
echo "=== Output files in $OUT_DIR ==="
ls -la "$OUT_DIR"

echo ""
echo "Total size:"
du -sh "$OUT_DIR"

echo ""
echo "Upload to ESP32 with: pio run -t uploadfs -e esp32s3"
