#!/usr/bin/env python3
"""
convert.py — Pretvori index.html v web_ui_gz.h (gzip PROGMEM header za ESP32)

Uporaba:
  python3 convert.py
  python3 convert.py [vhod.html] [izhod.h]

Format izhoda:
  #ifndef HTML_PAGE_GZ_H  / #define HTML_PAGE_GZ_H
  const uint8_t HTML_PAGE_GZ[] PROGMEM = { ... };   ← 16 bajtov/vrstica, hex
  const size_t  HTML_PAGE_GZ_LEN = <stevilo_bajtov>;
  #endif
"""

import gzip
import os
import sys
import time

# ---------------------------------------------------------------------------
#  Poti
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SCRIPT_DIR, 'index.html')
dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(SCRIPT_DIR, 'web_ui_gz.h')

# ---------------------------------------------------------------------------
#  Branje
# ---------------------------------------------------------------------------
if not os.path.isfile(src):
    print(f'NAPAKA: vhodna datoteka ni najdena: {src}', file=sys.stderr)
    sys.exit(1)

with open(src, 'rb') as f:
    raw = f.read()

# ---------------------------------------------------------------------------
#  Kompresija (gzip level 9 = maksimalna kompresija)
# ---------------------------------------------------------------------------
t0 = time.perf_counter()
data = gzip.compress(raw, compresslevel=9, mtime=0)  # mtime=0 → deterministični output (za primerjavo)
elapsed_ms = (time.perf_counter() - t0) * 1000

# ---------------------------------------------------------------------------
#  Gradnja vsebine .h datoteke
# ---------------------------------------------------------------------------
lines = [
    '#ifndef HTML_PAGE_GZ_H',
    '#define HTML_PAGE_GZ_H',
    '',
    '#include <pgmspace.h>',
    '',
    'const uint8_t HTML_PAGE_GZ[] PROGMEM = {',
]

BYTES_PER_LINE = 16
for i in range(0, len(data), BYTES_PER_LINE):
    chunk = data[i : i + BYTES_PER_LINE]
    hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
    trailing = ',' if i + BYTES_PER_LINE < len(data) else ''
    lines.append('  ' + hex_str + trailing)

lines += [
    '};',
    '',
    f'const size_t HTML_PAGE_GZ_LEN = {len(data)};',
    '',
    '#endif',
    '',  # končna prazna vrstica
]

output = '\n'.join(lines)

# ---------------------------------------------------------------------------
#  Zapis (samo če se vsebina dejansko razlikuje)
# ---------------------------------------------------------------------------
existing = None
if os.path.isfile(dst):
    with open(dst, 'r', encoding='utf-8') as f:
        existing = f.read()

if output == existing:
    print(f'Ni sprememb — {dst} je že ažuren.')
    sys.exit(0)

with open(dst, 'w', encoding='utf-8', newline='\n') as f:
    f.write(output)

# ---------------------------------------------------------------------------
#  Poročilo
# ---------------------------------------------------------------------------
ratio = 100.0 * (1.0 - len(data) / len(raw))
print(f'Vhod:        {src}')
print(f'             {len(raw):>10,} B (originalna velikost)')
print(f'Izhod:       {dst}')
print(f'             {len(data):>10,} B (gzip level 9, {ratio:.1f}% kompresija, {elapsed_ms:.0f} ms)')
print(f'Spremenljivki: HTML_PAGE_GZ  /  HTML_PAGE_GZ_LEN = {len(data)}')
