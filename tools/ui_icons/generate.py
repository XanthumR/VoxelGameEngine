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



# --- Milestone 8.1 goods, the Artisans and the service buildings ---

def grain(d):
    # Three ears of wheat with long stalks
    gold, dark = (226, 186, 80), (170, 130, 50)
    for x in (4, 8, 12):
        d.line((x, 8, x - 1, 15), fill=(160, 150, 60))
        d.ellipse((x - 2, 1, x + 1, 9), fill=gold, outline=INK)
        d.line((x - 1, 3, x - 1, 7), fill=dark)


def flour(d):
    sack, tie = (236, 230, 214), (160, 130, 90)
    d.polygon([(3, 5), (12, 5), (14, 14), (1, 14)], fill=sack, outline=INK)
    d.polygon([(5, 1), (10, 1), (12, 5), (3, 5)], fill=sack, outline=INK)
    d.line((4, 5, 11, 5), fill=tie)
    d.line((6, 9, 9, 9), fill=(200, 190, 170))
    d.line((5, 11, 10, 11), fill=(200, 190, 170))


def bread(d):
    crust, light = (190, 120, 50), (230, 170, 90)
    d.ellipse((1, 5, 15, 14), fill=crust, outline=INK)
    for x in (4, 7, 10):
        d.line((x, 7, x + 2, 10), fill=light)


def tallow(d):
    # A barrel of fat
    d.rectangle((3, 3, 12, 14), fill=WOOD, outline=INK)
    d.line((3, 6, 12, 6), fill=INK)
    d.line((3, 11, 12, 11), fill=INK)
    d.ellipse((3, 1, 12, 5), fill=(240, 226, 170), outline=INK)


def soap(d):
    bar, shine = (200, 222, 205), (240, 250, 240)
    d.rounded_rectangle((1, 6, 14, 13), radius=2, fill=bar, outline=INK)
    d.line((3, 8, 9, 8), fill=shine)
    for x, y in ((11, 2), (6, 1), (13, 4)):
        d.ellipse((x - 1, y, x + 1, y + 2), outline=(140, 180, 210))


def clay(d):
    lump, light = (176, 100, 60), (210, 140, 95)
    d.polygon([(1, 13), (4, 6), (9, 4), (14, 8), (15, 13)], fill=lump, outline=INK)
    d.line((5, 8, 9, 6), fill=light)
    d.line((0, 14, 15, 14), fill=INK)


def beef(d):
    meat, fat = (180, 50, 45), (240, 220, 200)
    d.ellipse((1, 3, 14, 13), fill=meat, outline=INK)
    d.arc((3, 5, 12, 11), 200, 340, fill=fat)
    d.ellipse((10, 9, 15, 15), fill=fat, outline=INK)


def iron(d):
    # A heap of rusty ore lumps
    ore, rust = (110, 80, 70), (170, 90, 50)
    for x, y in ((1, 8), (8, 8), (4, 3)):
        d.polygon([(x, y + 3), (x + 2, y), (x + 6, y + 1), (x + 7, y + 5), (x + 3, y + 7)], fill=ore, outline=INK)
        d.point((x + 3, y + 3), fill=rust)


def coal(d):
    lump, shine = (40, 40, 44), (110, 110, 120)
    for x, y in ((1, 8), (8, 9), (4, 3)):
        d.polygon([(x, y + 3), (x + 2, y), (x + 6, y + 1), (x + 7, y + 5), (x + 3, y + 6)], fill=lump, outline=INK)
        d.point((x + 2, y + 2), fill=shine)


def steel(d):
    # An ingot
    face, top = (140, 150, 162), (190, 198, 210)
    d.polygon([(1, 9), (5, 5), (15, 5), (11, 9)], fill=top, outline=INK)
    d.rectangle((1, 9, 11, 13), fill=face, outline=INK)
    d.polygon([(11, 9), (15, 5), (15, 9), (11, 13)], fill=(110, 118, 130), outline=INK)


def steel_beams(d):
    # Two I-beams end on
    steel = (120, 130, 145)
    for x in (1, 9):
        d.rectangle((x, 2, x + 5, 4), fill=steel, outline=INK)
        d.rectangle((x + 2, 4, x + 3, 11), fill=steel, outline=INK)
        d.rectangle((x, 11, x + 5, 13), fill=steel, outline=INK)


def canned_food(d):
    tin, label = (190, 196, 205), (180, 60, 50)
    d.rectangle((3, 3, 12, 14), fill=tin, outline=INK)
    d.ellipse((3, 1, 12, 5), fill=(220, 225, 232), outline=INK)
    d.rectangle((4, 7, 11, 11), fill=label)
    d.point((7, 9), fill=(240, 220, 120))


def sewing_machines(d):
    black, brass = (36, 36, 40), (214, 170, 60)
    d.rectangle((0, 12, 15, 14), fill=WOOD, outline=INK)
    d.rectangle((2, 3, 4, 12), fill=black, outline=INK)
    d.rectangle((2, 3, 13, 6), fill=black, outline=INK)
    d.rectangle((11, 6, 12, 10), fill=black, outline=INK)
    d.line((5, 4, 10, 4), fill=brass)
    d.ellipse((5, 7, 9, 11), outline=brass)


def artisan(d):
    hat = (36, 30, 26)
    d.ellipse((4, 6, 11, 14), fill=SKIN, outline=INK)
    d.rectangle((2, 5, 13, 6), fill=hat, outline=INK)
    d.chord((4, 0, 11, 8), 180, 360, fill=hat, outline=INK)
    d.point((6, 10), fill=INK)
    d.point((9, 10), fill=INK)
    d.line((6, 12, 9, 12), fill=(120, 80, 60))


def marketplace(d):
    red, white = (190, 50, 45), (236, 228, 205)
    for i, x in enumerate(range(1, 15, 3)):
        d.rectangle((x, 2, x + 2, 6), fill=red if i % 2 == 0 else white, outline=INK)
    d.line((2, 6, 2, 14), fill=INK)
    d.line((13, 6, 13, 14), fill=INK)
    d.rectangle((3, 10, 12, 12), fill=WOOD, outline=INK)
    d.point((5, 9), fill=(90, 160, 60))
    d.point((9, 9), fill=(220, 170, 60))


def school(d):
    brick = (170, 80, 55)
    d.rectangle((2, 7, 13, 14), fill=brick, outline=INK)
    d.polygon([(1, 7), (8, 2), (14, 7)], fill=(80, 88, 100), outline=INK)
    d.rectangle((7, 0, 8, 2), fill=(220, 180, 70))
    d.rectangle((7, 10, 8, 14), fill=WOOD_DARK)
    d.rectangle((3, 9, 5, 11), fill=(60, 90, 130))
    d.rectangle((10, 9, 12, 11), fill=(60, 90, 130))


def construction(d):
    # A hammer crossed with a trowel
    steel, handle = (150, 158, 170), WOOD
    d.line((3, 14, 11, 6), fill=handle, width=2)
    d.polygon([(9, 2), (13, 6), (11, 8), (7, 4)], fill=steel, outline=INK)
    d.line((13, 14, 9, 10), fill=handle, width=2)
    d.polygon([(2, 6), (7, 6), (9, 9), (4, 11)], fill=(200, 205, 212), outline=INK)


def theatre(d):
    # Comedy and tragedy masks
    light, dark = (240, 230, 200), (200, 170, 90)
    d.ellipse((1, 2, 9, 11), fill=light, outline=INK)
    d.ellipse((7, 5, 15, 14), fill=dark, outline=INK)
    d.point((3, 5), fill=INK)
    d.point((6, 5), fill=INK)
    d.arc((3, 6, 7, 9), 20, 160, fill=INK)
    d.point((9, 8), fill=INK)
    d.point((12, 8), fill=INK)
    d.arc((9, 10, 13, 13), 200, 340, fill=INK)


ICONS = {
    "wood": wood, "planks": planks, "fish": fish, "wool": wool, "work_clothes": work_clothes,
    "bricks": bricks, "sausages": sausages, "pigs": pigs,
    "coin": coin, "farmer": farmer, "worker": worker, "residents": residents,
    "upgrade": upgrade, "warning": warning, "ship": ship,
    "grain": grain, "flour": flour, "bread": bread, "tallow": tallow, "soap": soap, "clay": clay, "beef": beef,
    "iron": iron, "coal": coal, "steel": steel, "steel_beams": steel_beams, "canned_food": canned_food,
    "sewing_machines": sewing_machines, "artisan": artisan, "marketplace": marketplace, "school": school, "theatre": theatre,
    "construction": construction,
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
