#!/usr/bin/env python3
"""
Tiered badge icon generator
Usage:   badge-icon-gen.py <tier:1|2|3> <badge_name>
Output:  ~/.local/share/muhhwm/badges/<badge_name>.<tier>.bmp
"""

import sys, os, hashlib, struct, random

# ── constants ─────────────────────────────────────
SIZE = 16  # output size (pixels)
OUTDIR = os.path.expanduser("~/.local/share/muhhwm/badges")

# colour palettes
FG1 = [(0x5C, 0x6A, 0x72), (0x8D, 0xA1, 0x01), (0xDF, 0xA0, 0x00), (0x3A, 0x94, 0xC5)]
FG2 = [(0xA6, 0xB0, 0xA0), (0xF8, 0x55, 0x52), (0x35, 0xA7, 0x7C), (0xDF, 0x69, 0xBA)]
FG3 = [(0xFF, 0xFF, 0xFF), (0x00, 0x00, 0x00), (0xFF, 0xD7, 0x00), (0x81, 0xA1, 0xC1)]
BG = [(0xF2, 0xEF, 0xDF), (0xE0, 0xDC, 0xC8), (0xA6, 0xB0, 0xA0), (0xF8, 0x55, 0x52)]


# ── helper: bits from seed ────────────────────────
def bits_from_seed(seed, n):
    bits = []
    val = seed
    for _ in range((n + 31) // 32):
        for _ in range(32):
            bits.append(val & 1)
            val >>= 1
            if val == 0:
                val = int.from_bytes(
                    hashlib.sha256(str(seed).encode()).digest()[:4], "big"
                )
    return bits[:n]


# ── grid generation ───────────────────────────────
def make_grid(seed, tier):
    rng = random.Random(seed)

    # grid dimensions
    if tier == 3:
        grid_sz = 7
        cell_sz = 2  # 7*2=14, border=1 each → 16
    else:
        grid_sz = 5
        cell_sz = 3  # 5*3=15, border=1 each → 16 (asymmetric, but fine)

    # symmetry modes
    if tier == 1:
        sym = 3  # mirror both (clean, simple)
    elif tier == 2:
        sym = rng.randint(0, 3)  # random mirror type
    else:
        sym = rng.randint(3, 4)  # rotational 90° or 180°

    # number of foreground colours
    if tier == 1:
        fg_count = 1
    elif tier == 2:
        fg_count = 2
    else:
        fg_count = 3

    # generate the top‑left quadrant
    fill = [[0] * grid_sz for _ in range(grid_sz)]
    bits = bits_from_seed(seed, grid_sz * grid_sz * 2)  # enough bits for 0-3 values

    for y in range(grid_sz):
        for x in range(grid_sz):
            # only fill cells that will be used (different for each sym mode)
            if sym == 0:  # no symmetry
                active = True
            elif sym == 1:  # horizontal
                active = x < (grid_sz + 1) // 2
            elif sym == 2:  # vertical
                active = y < (grid_sz + 1) // 2
            elif sym == 3:  # mirror both
                active = x < (grid_sz + 1) // 2 and y < (grid_sz + 1) // 2
            else:  # rotational
                active = x < (grid_sz + 1) // 2 and y < (grid_sz + 1) // 2

            if active:
                idx = (y * grid_sz + x) * 2
                val = bits[idx] | (bits[idx + 1] << 1)
                if val >= fg_count:
                    val = rng.randint(1, fg_count)  # fallback
                fill[y][x] = val

    # apply symmetry
    if sym == 1:  # horizontal
        for y in range(grid_sz):
            for x in range(grid_sz // 2):
                fill[y][grid_sz - 1 - x] = fill[y][x]
    elif sym == 2:  # vertical
        for y in range(grid_sz // 2):
            for x in range(grid_sz):
                fill[grid_sz - 1 - y][x] = fill[y][x]
    elif sym == 3:  # mirror both
        for y in range((grid_sz + 1) // 2):
            for x in range((grid_sz + 1) // 2):
                fill[y][grid_sz - 1 - x] = fill[y][x]
                fill[grid_sz - 1 - y][x] = fill[y][x]
                fill[grid_sz - 1 - y][grid_sz - 1 - x] = fill[y][x]
    else:  # rotational 90° or 180°
        if sym == 4:  # 90° rotation
            for y in range(grid_sz):
                for x in range(grid_sz):
                    fill[grid_sz - 1 - x][y] = fill[y][x]
        else:  # 180° rotation
            for y in range(grid_sz):
                for x in range(grid_sz):
                    fill[grid_sz - 1 - y][grid_sz - 1 - x] = fill[y][x]

    return fill, grid_sz, cell_sz, fg_count


# ── convert grid to pixel map ─────────────────────
def grid_to_pixels(grid, grid_sz, cell_sz, tier):
    border = 1 if (SIZE - grid_sz * cell_sz) >= 2 else 0
    off = (SIZE - grid_sz * cell_sz) // 2

    pixels = [[0] * SIZE for _ in range(SIZE)]
    for cy in range(grid_sz):
        for cx in range(grid_sz):
            val = grid[cy][cx]
            if val == 0:
                continue
            y0 = off + cy * cell_sz
            x0 = off + cx * cell_sz
            for dy in range(cell_sz):
                y = y0 + dy
                if 0 <= y < SIZE:
                    for dx in range(cell_sz):
                        x = x0 + dx
                        if 0 <= x < SIZE:
                            pixels[y][x] = val

    # tier‑specific extras
    rng = random.Random(
        int.from_bytes(hashlib.sha256(str(id(grid)).encode()).digest()[:4], "big")
    )
    if tier == 2:
        # add 1‑pixel border of a contrasting colour (fg2 if available)
        col = 2 if any(2 in row for row in grid) else 1
        for y in range(SIZE):
            pixels[y][0] = pixels[y][SIZE - 1] = col
        for x in range(1, SIZE - 1):
            pixels[0][x] = pixels[SIZE - 1][x] = col
    elif tier == 3:
        # central diamond (3x3)
        cx, cy = SIZE // 2, SIZE // 2
        for dy in (-1, 0, 1):
            y = cy + dy
            for dx in (-1, 0, 1):
                x = cx + dx
                if 0 <= y < SIZE and 0 <= x < SIZE and abs(dx) + abs(dy) <= 2:
                    pixels[y][x] = 3  # fg3 for legendary detail
        # corner accents
        for cx, cy in [(0, 0), (0, SIZE - 1), (SIZE - 1, 0), (SIZE - 1, SIZE - 1)]:
            for dy in (0, 1):
                y = cy + dy if cy == 0 else cy - dy
                for dx in (0, 1):
                    x = cx + dx if cx == 0 else cx - dx
                    if 0 <= y < SIZE and 0 <= x < SIZE:
                        pixels[y][x] = 2
    return pixels


# ── BMP writer ────────────────────────────────────
def save_bmp(pixels, fg_palette, bg_color, filepath):
    rowsize = SIZE * 3
    pad = (4 - rowsize % 4) % 4
    rowsize += pad
    filesize = 14 + 40 + rowsize * SIZE

    with open(filepath, "wb") as f:
        f.write(b"BM")
        f.write(struct.pack("<I", filesize))
        f.write(b"\x00\x00\x00\x00")
        f.write(struct.pack("<I", 14 + 40))
        f.write(
            struct.pack(
                "<IiiHHIIiiII", 40, SIZE, SIZE, 1, 24, 0, rowsize * SIZE, 0, 0, 0, 0
            )
        )

        for row in reversed(pixels):
            data = bytearray()
            for val in row:
                if val == 0:
                    r, g, b = bg_color
                elif val <= len(fg_palette):
                    r, g, b = fg_palette[val - 1]
                else:
                    r, g, b = bg_color
                data.extend([b, g, r])
            data.extend(b"\x00" * pad)
            f.write(data)


# ── main ──────────────────────────────────────────
def main():
    if len(sys.argv) < 3:
        sys.exit(f"Usage: {sys.argv[0]} <tier:1|2|3> <badge_name>")

    tier = int(sys.argv[1])
    if tier not in (1, 2, 3):
        sys.exit("Tier must be 1, 2, or 3")
    name = sys.argv[2]

    sha = hashlib.sha256(name.encode()).hexdigest()
    seed = int(sha, 16) & 0xFFFFFFFF
    rng = random.Random(seed)

    fg1_idx = (seed >> 2) & 3
    fg2_idx = (seed >> 4) & 3
    fg3_idx = (seed >> 6) & 3
    bg_idx = seed & 3

    fg_palette = [FG1[fg1_idx]]
    if tier >= 2:
        fg_palette.append(FG2[fg2_idx])
    if tier >= 3:
        fg_palette.append(FG3[fg3_idx])

    bg = BG[bg_idx]

    grid, grid_sz, cell_sz, fg_count = make_grid(seed, tier)
    pixels = grid_to_pixels(grid, grid_sz, cell_sz, tier)

    os.makedirs(OUTDIR, exist_ok=True)
    fname = f"{name}.{tier}.bmp"
    outpath = os.path.join(OUTDIR, fname)

    save_bmp(pixels, fg_palette, bg, outpath)
    print(f"Saved {outpath}")


if __name__ == "__main__":
    main()
