"""Generates the game UI's pixel-art icons (assets/ui/icons/*.tga: RmlUi's OpenGL backend reads TGA).

Each icon is drawn on a 16x16 canvas with plain (unsmoothed) shapes, then scaled up 4x with
nearest-neighbour so the pixels stay crisp, like the voxel world. Run from the project root:
    python tools/ui_icons/generate.py
"""

import os

from PIL import Image, ImageDraw

SIZE = 16
SCALE = 4
OUT = os.path.join("assets", "ui", "icons")

INK = (43, 29, 18)          # Outline
WOOD = (150, 98, 52)
WOOD_LIGHT = (205, 156, 96)
WOOD_DARK = (102, 64, 32)
SKIN = (233, 186, 140)


def icon(draw_fn):
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(image))
    return image.resize((SIZE * SCALE, SIZE * SCALE), Image.NEAREST)


def log_end(d, x, y):
    d.ellipse((x, y, x + 6, y + 6), fill=WOOD_LIGHT, outline=INK)
    d.ellipse((x + 2, y + 2, x + 4, y + 4), outline=WOOD)


def wood(d):
    # Three logs stacked, seen from the end
    log_end(d, 1, 8)
    log_end(d, 8, 8)
    log_end(d, 4, 2)


def planks(d):
    for i, y in enumerate((3, 7, 11)):
        x = 1 + (i % 2)
        d.rectangle((x, y, x + 13, y + 3), fill=WOOD_LIGHT if i % 2 == 0 else WOOD, outline=INK)
        d.line((x + 4, y + 1, x + 7, y + 1), fill=WOOD_DARK)


def fish(d):
    body = (95, 140, 170)
    d.polygon([(2, 8), (6, 4), (11, 5), (13, 8), (11, 11), (6, 12)], fill=body, outline=INK)
    d.polygon([(13, 8), (15, 5), (15, 11)], fill=body, outline=INK)
    d.line((5, 9, 10, 9), fill=(170, 205, 220))
    d.point((5, 7), fill=INK)


def wool(d):
    puff = (236, 230, 214)
    for x, y in ((2, 6), (6, 3), (9, 6), (5, 8)):
        d.ellipse((x, y, x + 6, y + 6), fill=puff, outline=INK)
    d.ellipse((6, 6, 9, 9), fill=puff)
    d.line((12, 13, 14, 15), fill=WOOD_DARK)


def work_clothes(d):
    cloth = (70, 98, 140)
    d.polygon([(4, 2), (6, 2), (8, 4), (10, 2), (12, 2), (15, 5), (13, 7), (12, 6), (12, 14), (4, 14), (4, 6), (3, 7), (1, 5)],
              fill=cloth, outline=INK)
    d.line((8, 5, 8, 13), fill=(50, 72, 105))
    d.point((7, 7), fill=WOOD_LIGHT)
    d.point((7, 10), fill=WOOD_LIGHT)


def bricks(d):
    red, mortar = (165, 72, 50), (214, 196, 170)
    d.rectangle((1, 3, 14, 13), fill=mortar, outline=INK)
    for row, y in enumerate((4, 7, 10)):
        offset = 0 if row % 2 == 0 else -3
        for x in range(2 + offset, 14, 6):
            d.rectangle((max(2, x), y, min(13, x + 4), y + 1), fill=red)


def sausages(d):
    # A chain of three links, tied off between them
    meat, shine = (170, 80, 60), (215, 130, 105)
    for x, y in ((0, 9), (5, 5), (10, 1)):
        d.ellipse((x, y, x + 6, y + 6), fill=meat, outline=INK)
        d.point((x + 2, y + 2), fill=shine)
    for x, y in ((5, 10), (10, 6)):
        d.point((x, y), fill=(236, 228, 205))


def pigs(d):
    pink, dark = (232, 160, 160), (190, 110, 115)
    d.ellipse((1, 5, 12, 14), fill=pink, outline=INK)
    d.ellipse((9, 4, 15, 11), fill=pink, outline=INK)
    d.rectangle((13, 7, 15, 9), fill=dark, outline=INK)
    d.point((11, 6), fill=INK)
    d.polygon([(10, 4), (11, 2), (12, 4)], fill=dark, outline=INK)
    d.line((3, 14, 3, 15), fill=INK)
    d.line((9, 14, 9, 15), fill=INK)


def coin(d):
    gold, light = (214, 166, 52), (250, 214, 110)
    d.ellipse((1, 1, 14, 14), fill=gold, outline=INK)
    d.ellipse((3, 3, 12, 12), outline=(170, 122, 30))
    d.line((7, 5, 7, 10), fill=light)
    d.line((8, 5, 8, 10), fill=(170, 122, 30))


def farmer(d):
    straw = (226, 190, 92)
    d.ellipse((4, 6, 11, 14), fill=SKIN, outline=INK)
    d.rectangle((0, 6, 15, 7), fill=straw, outline=INK)
    d.rectangle((4, 2, 11, 6), fill=straw, outline=INK)
    d.line((5, 5, 10, 5), fill=(160, 60, 40))
    d.point((6, 10), fill=INK)
    d.point((9, 10), fill=INK)


def worker(d):
    cap = (80, 80, 92)
    d.ellipse((4, 5, 11, 14), fill=SKIN, outline=INK)
    d.chord((3, 2, 12, 10), 180, 360, fill=cap, outline=INK)
    d.rectangle((9, 5, 14, 6), fill=cap, outline=INK)
    d.point((6, 10), fill=INK)
    d.point((9, 10), fill=INK)


def residents(d):
    for x, shade in ((1, (150, 120, 90)), (7, (110, 85, 60))):
        d.ellipse((x + 1, 2, x + 6, 7), fill=SKIN, outline=INK)
        d.rounded_rectangle((x, 8, x + 7, 15), radius=2, fill=shade, outline=INK)


def upgrade(d):
    green = (70, 160, 80)
    d.polygon([(8, 1), (15, 8), (11, 8), (11, 14), (5, 14), (5, 8), (1, 8)], fill=green, outline=INK)
    d.line((7, 4, 7, 12), fill=(140, 215, 140))


def warning(d):
    amber = (238, 160, 30)
    d.polygon([(8, 1), (15, 14), (1, 14)], fill=amber, outline=INK)
    d.rectangle((7, 5, 8, 10), fill=INK)
    d.rectangle((7, 12, 8, 12), fill=INK)


def ship(d):
    d.polygon([(1, 10), (15, 10), (13, 14), (3, 14)], fill=WOOD, outline=INK)
    d.line((8, 1, 8, 10), fill=INK)
    d.polygon([(9, 2), (14, 8), (9, 8)], fill=(236, 228, 205), outline=INK)
    d.polygon([(7, 3), (3, 8), (7, 8)], fill=(236, 228, 205), outline=INK)


ICONS = {
    "wood": wood, "planks": planks, "fish": fish, "wool": wool, "work_clothes": work_clothes,
    "bricks": bricks, "sausages": sausages, "pigs": pigs,
    "coin": coin, "farmer": farmer, "worker": worker, "residents": residents,
    "upgrade": upgrade, "warning": warning, "ship": ship,
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, draw_fn in ICONS.items():
        icon(draw_fn).save(os.path.join(OUT, name + ".tga"), compression=None)
    # A contact sheet to look them over (not used by the game)
    sheet = Image.new("RGBA", (len(ICONS) * (SIZE * SCALE + 8), SIZE * SCALE + 8), (239, 226, 196, 255))
    for i, draw_fn in enumerate(ICONS.values()):
        sheet.alpha_composite(icon(draw_fn), (4 + i * (SIZE * SCALE + 8), 4))
    sheet.save(os.path.join(os.environ.get("TEMP", "."), "ui_icons_sheet.png"))
    print(f"Wrote {len(ICONS)} icons to {OUT}")
