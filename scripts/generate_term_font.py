#!/usr/bin/env python3
"""
Generate fixed-width terminal bitmap font headers for X4Term.

Renders ASCII printable characters (0x20-0x7E) and optional extended
Unicode ranges as fixed-width 1-bit bitmap fonts stored in PROGMEM.

Usage:
    # Generate ASCII font only
    python3 generate_term_font.py

    # Generate ASCII + extended Unicode ranges
    python3 generate_term_font.py --ext-ranges 00A0-00FF,2010-2027,2190-2199

    # Custom font
    python3 generate_term_font.py --font /path/to/Font.ttf --width 10 --height 20
"""

import argparse
import sys
import os
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)


FIRST_CHAR = 0x20  # space
LAST_CHAR  = 0x7E  # tilde
NUM_CHARS  = LAST_CHAR - FIRST_CHAR + 1  # 95


def find_system_mono_font():
    """Try to find a monospace font on the system."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    candidates = [
        os.path.join(project_root, "fonts", "DejaVuSansMono.ttf"),
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return None


def load_font_fitting_cell(font_path, cell_w, cell_h):
    """Load font and find the largest pt size that fits the cell."""
    pt_size = max(1, cell_h)
    while pt_size > 0:
        try:
            font = ImageFont.truetype(font_path, pt_size)
        except Exception as e:
            print(f"Error loading font: {e}")
            return None, None
        ascent, descent = font.getmetrics()
        if ascent + descent <= cell_h:
            # Check that widest char fits cell width
            max_w = 0
            for cp in range(FIRST_CHAR, LAST_CHAR + 1):
                bbox = font.getbbox(chr(cp))
                if bbox:
                    max_w = max(max_w, bbox[2] - bbox[0])
            if max_w <= cell_w:
                return font, pt_size
        pt_size -= 1
    return None, None


def render_char(font, char, cell_w, cell_h, baseline, ascent):
    """Render a single character to a bitmap."""
    bytes_per_row = (cell_w + 7) // 8
    bytes_per_glyph = bytes_per_row * cell_h

    img = Image.new('1', (cell_w, cell_h), 0)
    draw = ImageDraw.Draw(img)

    try:
        bbox = font.getbbox(char)
        glyph_w = bbox[2] - bbox[0] if bbox else cell_w
    except Exception:
        glyph_w = cell_w

    x = max(0, (cell_w - glyph_w) // 2)

    try:
        draw.text((x, baseline), char, font=font, fill=1, anchor="ls")
    except TypeError:
        draw.text((x, baseline - ascent), char, font=font, fill=1)

    bitmap = bytearray(bytes_per_glyph)
    for row in range(cell_h):
        for byte_idx in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                px = byte_idx * 8 + bit
                if px < cell_w and img.getpixel((px, row)):
                    byte_val |= (1 << (7 - bit))
            bitmap[row * bytes_per_row + byte_idx] = byte_val

    return bitmap


def is_glyph_blank(bitmap):
    """Check if a rendered bitmap is entirely blank."""
    return all(b == 0 for b in bitmap)


def generate_font(font_path, cell_w, cell_h, output_path):
    font, pt_size = load_font_fitting_cell(font_path, cell_w, cell_h)
    if font is None:
        print("Error: Could not fit font into cell")
        return False

    ascent, descent = font.getmetrics()
    baseline = cell_h - descent

    bytes_per_row = (cell_w + 7) // 8
    bytes_per_glyph = bytes_per_row * cell_h

    print(f"Font: {Path(font_path).name}")
    print(f"  PT size: {pt_size}, ascent={ascent}, descent={descent}")
    print(f"  Cell: {cell_w}x{cell_h}, bytes/glyph: {bytes_per_glyph}")
    print(f"  Characters: {NUM_CHARS} (0x{FIRST_CHAR:02X}-0x{LAST_CHAR:02X})")
    print(f"  Total: {NUM_CHARS * bytes_per_glyph} bytes")

    glyphs = []
    for cp in range(FIRST_CHAR, LAST_CHAR + 1):
        bitmap = render_char(font, chr(cp), cell_w, cell_h, baseline, ascent)
        glyphs.append(bitmap)

    # Write header
    with open(output_path, 'w') as f:
        f.write(f"""/**
 * Auto-generated fixed-width terminal font
 * Source: {Path(font_path).name}
 * PT size: {pt_size}
 * Cell: {cell_w}x{cell_h}
 * Characters: {NUM_CHARS} (ASCII 0x{FIRST_CHAR:02X}-0x{LAST_CHAR:02X})
 * Total bitmap: {NUM_CHARS * bytes_per_glyph} bytes (PROGMEM)
 */
#pragma once

#include <cstdint>
#include <pgmspace.h>

namespace TermFont {{

static constexpr uint8_t FONT_W = {cell_w};
static constexpr uint8_t FONT_H = {cell_h};
static constexpr uint8_t BYTES_PER_ROW = {bytes_per_row};
static constexpr uint8_t BYTES_PER_GLYPH = {bytes_per_glyph};
static constexpr uint8_t FIRST_CHAR = 0x{FIRST_CHAR:02X};
static constexpr uint8_t LAST_CHAR = 0x{LAST_CHAR:02X};
static constexpr uint8_t NUM_CHARS = {NUM_CHARS};

static const uint8_t glyphs[{NUM_CHARS * bytes_per_glyph}] PROGMEM = {{
""")

        for i, bitmap in enumerate(glyphs):
            cp = FIRST_CHAR + i
            char_repr = chr(cp) if cp != 0x5C else '<backslash>'
            f.write(f"    // 0x{cp:02X} '{char_repr}'\n    ")
            for j, b in enumerate(bitmap):
                f.write(f"0x{b:02X},")
                if (j + 1) % 16 == 0 and j < len(bitmap) - 1:
                    f.write("\n    ")
            f.write("\n")

        f.write(f"""}};

inline const uint8_t* getGlyph(uint8_t c) {{
    if (c < FIRST_CHAR || c > LAST_CHAR) c = '?';
    return &glyphs[(c - FIRST_CHAR) * BYTES_PER_GLYPH];
}}

}} // namespace TermFont
""")

    print(f"Output: {output_path}")
    return True


def parse_ranges(ranges_str):
    """Parse range string like '00A0-00FF,2010-2027' into list of (start, end) tuples."""
    ranges = []
    for r in ranges_str.split(','):
        r = r.strip()
        if '-' in r:
            start, end = r.split('-', 1)
            ranges.append((int(start, 16), int(end, 16)))
        else:
            cp = int(r, 16)
            ranges.append((cp, cp))
    return ranges


def generate_extended_font(font_path, cell_w, cell_h, ranges_str, output_path):
    """Generate extended Unicode glyph header with pre-rendered glyphs from font."""
    font, pt_size = load_font_fitting_cell(font_path, cell_w, cell_h)
    if font is None:
        print("Error: Could not fit font into cell")
        return False

    ascent, descent = font.getmetrics()
    baseline = cell_h - descent
    bytes_per_row = (cell_w + 7) // 8
    bytes_per_glyph = bytes_per_row * cell_h

    ranges = parse_ranges(ranges_str)

    total_glyphs = sum(end - start + 1 for start, end in ranges)
    total_bytes = total_glyphs * bytes_per_glyph

    print(f"Extended Unicode font: {Path(font_path).name}, PT {pt_size}")
    print(f"  Ranges: {len(ranges)}")
    for start, end in ranges:
        count = end - start + 1
        print(f"    U+{start:04X}-U+{end:04X}: {count} glyphs ({count * bytes_per_glyph} bytes)")
    print(f"  Total: {total_glyphs} glyphs, {total_bytes} bytes")

    with open(output_path, 'w') as f:
        f.write(f"""/**
 * Auto-generated extended Unicode font glyphs
 * Source: {Path(font_path).name}, PT size: {pt_size}
 * Cell: {cell_w}x{cell_h}
 * Total: {total_glyphs} glyphs, {total_bytes} bytes (PROGMEM)
 *
 * Ranges:
""")
        for start, end in ranges:
            f.write(f" *   U+{start:04X}-U+{end:04X} ({end - start + 1} glyphs)\n")
        f.write(f""" */
#pragma once

#include <cstdint>
#include <pgmspace.h>

namespace TermFontExt {{

static constexpr uint8_t BYTES_PER_GLYPH = {bytes_per_glyph};
""")

        # Generate each range as a separate PROGMEM array
        range_names = []
        for start, end in ranges:
            count = end - start + 1
            name = f"u{start:04X}"
            range_names.append((name, start, end))

            f.write(f"""
// U+{start:04X}-U+{end:04X} ({count} glyphs, {count * bytes_per_glyph} bytes)
static constexpr uint16_t {name}_START = 0x{start:04X};
static constexpr uint16_t {name}_END = 0x{end:04X};
static const uint8_t {name}[{count * bytes_per_glyph}] PROGMEM = {{
""")

            rendered = 0
            blank = 0
            for cp in range(start, end + 1):
                char = chr(cp)
                bitmap = render_char(font, char, cell_w, cell_h, baseline, ascent)

                if is_glyph_blank(bitmap) and cp > 0x20:
                    blank += 1
                else:
                    rendered += 1

                # Comment with character info
                try:
                    char_repr = char if (cp >= 0x20 and cp < 0x7F) else char
                except Exception:
                    char_repr = '?'
                f.write(f"    // U+{cp:04X} '{char_repr}'\n    ")
                for j, b in enumerate(bitmap):
                    f.write(f"0x{b:02X},")
                    if (j + 1) % 16 == 0 and j < len(bitmap) - 1:
                        f.write("\n    ")
                f.write("\n")

            f.write("};\n")
            print(f"    {name}: {rendered} rendered, {blank} blank")

        # Generate lookup function
        f.write("""
/**
 * Look up a Unicode codepoint in the extended font tables.
 * Returns pointer to PROGMEM glyph data, or nullptr if not found.
 */
inline const uint8_t* lookup(uint16_t cp) {
""")
        for name, start, end in range_names:
            f.write(f"    if (cp >= 0x{start:04X} && cp <= 0x{end:04X})\n")
            f.write(f"        return &{name}[(cp - 0x{start:04X}) * BYTES_PER_GLYPH];\n")
        f.write("""    return nullptr;
}

} // namespace TermFontExt
""")

    print(f"Output: {output_path}")
    return True


def main():
    parser = argparse.ArgumentParser(description='Generate terminal bitmap font header')
    parser.add_argument('--font', type=str, help='Path to monospace TTF/OTF font')
    parser.add_argument('--width', type=int, default=10, help='Cell width in pixels (default: 10)')
    parser.add_argument('--height', type=int, default=20, help='Cell height in pixels (default: 20)')
    parser.add_argument('--output', type=str, help='Output header path (ASCII font)')
    parser.add_argument('--ext-ranges', type=str,
                        help='Extended Unicode ranges to render, e.g. "00A0-00FF,2010-2027,2190-2199"')
    parser.add_argument('--ext-output', type=str, help='Output path for extended Unicode header')
    args = parser.parse_args()

    font_path = args.font
    if not font_path:
        font_path = find_system_mono_font()
        if not font_path:
            print("Error: No monospace font found. Use --font to specify one.")
            sys.exit(1)
        print(f"Using system font: {font_path}")

    if not os.path.exists(font_path):
        print(f"Error: Font file not found: {font_path}")
        sys.exit(1)

    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # Generate ASCII font
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = project_root / 'lib' / 'TermFont' / f'term_font_{args.width}x{args.height}.h'

    output_path.parent.mkdir(parents=True, exist_ok=True)

    if not generate_font(font_path, args.width, args.height, str(output_path)):
        print("ASCII font generation failed!")
        sys.exit(1)

    # Generate extended Unicode font if requested
    if args.ext_ranges:
        if args.ext_output:
            ext_output_path = Path(args.ext_output)
        else:
            ext_output_path = project_root / 'lib' / 'TermFont' / 'term_font_ext.h'

        ext_output_path.parent.mkdir(parents=True, exist_ok=True)

        if not generate_extended_font(font_path, args.width, args.height,
                                       args.ext_ranges, str(ext_output_path)):
            print("Extended font generation failed!")
            sys.exit(1)

    print("Done!")


if __name__ == '__main__':
    main()
