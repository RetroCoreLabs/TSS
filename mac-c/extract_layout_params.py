#!/usr/bin/env python3
"""Extract the distribution layout-parameter block from SINTRAN-L-1:DATA.

The MACM generation stream opens with a plain-text header that defines each
SINTRAN system area's SEGFIL page number, plus the macro-name -> area-name
legend. That block is an authoritative, non-OCR witness for the segment
page map, independent of both the live LIST-SEGMENT dump and the OCR'd
release-manual section 8.3.

Writes:  distribution-layout-params.txt   verbatim header text
         distribution-layout-params.json  parsed parameters + legend
"""
import json
import re
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 \
    else '/mnt/d/ND/extract/VSXL1/SINTRAN-L-1.DATA'
OUTDIR = sys.argv[2] if len(sys.argv) > 2 else '.'

raw = open(SRC, 'rb').read()
txt = bytes(b & 0x7F for b in raw).decode('latin-1')   # strip ND parity

# The header runs from the start to the first control/binary byte; the agent
# located the first control byte at offset 7897.
end = 0
while end < len(txt) and (32 <= ord(txt[end]) < 127 or txt[end] in '\r\n\t'):
    end += 1
header = txt[:end]

with open(f'{OUTDIR}/distribution-layout-params.txt', 'w',
          newline='\n') as f:
    f.write(header.replace('\r\n', '\n').replace('\r', '\n'))

# --- parse "NAME=expr" layout parameters -------------------------------
params = {}
for m in re.finditer(r'(?m)^\s*([A-Z][A-Z0-9]{1,5})\s*=\s*([0-9+\-]+)\s*$',
                     header):
    name, expr = m.group(1), m.group(2)
    # leading additive terms are the page address; a trailing "-N" is a
    # length / in-page offset and is NOT part of the address
    terms = re.findall(r'([+-])?(\d+)', expr)
    addr, first = 0, True
    for sign, digits in terms:
        if sign == '-' and not first:
            break                      # stop at the trailing subtraction
        v = int(digits, 8)
        addr = v if first else addr + v
        first = False
    params[name] = {
        'raw': expr,
        'page_oct': f'{addr:o}',
        'page_dec': addr,
        'madr_oct': f'{addr - 0o200:o}' if addr >= 0o200 else None,
        'madr_dec': addr - 0o200 if addr >= 0o200 else None,
    }

# --- parse the macro-name -> area-name legend from the %% comments -----
legend = {}
for m in re.finditer(r'%%\s*-\s*([A-Z0-9]{3,6})\s{2,}(.+?)\s*$',
                     header, re.M):
    legend[m.group(1)] = m.group(2).strip()

out = {
    'source_image': 'D:/ND/S/VSXL1.IMG',
    'source_volume': '250305L07-XX-01D',
    'source_file': '(SYSTEM)SINTRAN-L-1:DATA',
    'note': ('Page numbers are SEGFIL page addresses, the same quantity as '
             '"madr" in segment-facts.json. The image base is 0o200: '
             'madr = page - 0o200. A trailing "-N" in the raw expression is '
             'a length/offset, not part of the address.'),
    'header_bytes': end,
    'layout_params': params,
    'macro_legend': legend,
}
with open(f'{OUTDIR}/distribution-layout-params.json', 'w',
          newline='\n') as f:
    json.dump(out, f, indent=2)
    f.write('\n')

print(f'header: {end} bytes')
print(f'layout parameters: {len(params)}')
print(f'legend entries: {len(legend)}')
for k, v in list(params.items())[:8]:
    print(f"  {k:<6} {v['raw']:<12} page {v['page_oct']:>6}  "
          f"madr {v['madr_oct']}")
