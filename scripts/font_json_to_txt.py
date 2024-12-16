#!/usr/bin/env python3

"""
Convert json output from msdf-atlas-gen (https://github.com/Chlumsky/msdf-atlas-gen) to custom format for parsing in C++.
Output goes to asset/atlas.txt.

Example:
```
msdf-atlas-gen-font /usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf -type msdf -fontname ubuntu_mono -uniformcols 10 -imageout atlas.bmp -json atlas.json --pxrange 8
./font_json_to_txt.py atlas.json > atlas.txt
```
"""

import json
import sys

if len(sys.argv) == 1:
    sys.exit("missing json")

with open(sys.argv[1], "r") as fp:
    data = json.load(fp)

a = data["atlas"]
print(f"distance_range {a['distanceRange']}")
print(f"em_size {a['size']}")
print(f"grid_width {a['grid']['cellWidth']}")
print(f"grid_height {a['grid']['cellHeight']}")
print(f"grid_cols {a['grid']['columns']}")
print(f"grid_rows {a['grid']['rows']}")

advance = [0]*256

for a in data["glyphs"]:
    ch = a["unicode"]
    advance[ch] = a["advance"]

print("advance ", end="")

for a in advance:
    print(f"{a} ", end="")
