"""Renders the build menu's building pictures (assets/ui/buildings/<model name>.tga).

Each building's first model (assets/buildings/<model name>_1.vox) is drawn in isometric view from
the front (the door side) and the right, on a patch of grass, with the colors saved in the file
(the in-game colors), and its moving parts (assets/buildings/parts/parts.txt) where they rest. It is drawn large and shrunk smoothly to fill the same canvas as the others. Run from the project root:
    python tools/ui_icons/buildings.py
"""

import glob
import os
import struct

from PIL import Image, ImageDraw

MODELS = os.path.join("assets", "buildings")
OUT = os.path.join("assets", "ui", "buildings")
CANVAS = (240, 180)  # Shown at half size on the build menu's cards
MARKERS = (100, 101)  # Smoke emitter and boat berth: not drawn
GRASS = 1
SHADE = {"top": 1.0, "front": 0.8, "side": 0.62}


def read_vox(path):
    """Returns (width, depth, height, {(x, y, z): index}, palette) of the first model in a .vox file."""
    data = open(path, "rb").read()
    size, voxels, palette = None, {}, None
    pos = 8 + 12  # "VOX " + version, then MAIN's header
    while pos < len(data):
        name = data[pos:pos + 4]
        content, children = struct.unpack_from("<ii", data, pos + 4)
        body = pos + 12
        if name == b"SIZE" and size is None:
            size = struct.unpack_from("<iii", data, body)
        elif name == b"XYZI" and not voxels:
            count = struct.unpack_from("<i", data, body)[0]
            for i in range(count):
                x, y, z, index = data[body + 4 + i * 4: body + 8 + i * 4]
                if index not in MARKERS:
                    voxels[(x, y, z)] = index
        elif name == b"RGBA":
            palette = [tuple(data[body + i * 4: body + i * 4 + 3]) for i in range(256)]
        pos = body + content + children
    return size[0], size[1], size[2], voxels, palette


def render(width, depth, voxels, color_of, scale):
    """Isometric picture of voxels {(p, q, z): rgb}: p to the right, q toward the viewer, z up."""
    def project(x, y, z):
        return ((x - y) * 2 * scale, (x + y) * scale - z * 2 * scale)

    xs, ys = [], []
    for p, q, z in voxels:
        for corner in ((p, q + 1, z), (p + 1, q, z), (p + 1, q + 1, z), (p, q, z + 1)):
            sx, sy = project(*corner)
            xs.append(sx)
            ys.append(sy)
    left, top = min(xs), min(ys)
    image = Image.new("RGBA", (max(xs) - left + 1, max(ys) - top + 1), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    def face(corners, rgb):
        draw.polygon([(sx - left, sy - top) for sx, sy in (project(*c) for c in corners)], fill=rgb)

    # Back to front, bottom to top
    for (p, q, z) in sorted(voxels, key=lambda v: (v[0] + v[1] + v[2], v[2])):
        rgb = color_of(voxels[(p, q, z)])
        shade = lambda name: tuple(int(c * SHADE[name]) for c in rgb)
        if (p, q, z + 1) not in voxels:
            face([(p, q, z + 1), (p + 1, q, z + 1), (p + 1, q + 1, z + 1), (p, q + 1, z + 1)], shade("top"))
        if (p, q + 1, z) not in voxels:
            face([(p, q + 1, z), (p + 1, q + 1, z), (p + 1, q + 1, z + 1), (p, q + 1, z + 1)], shade("front"))
        if (p + 1, q, z) not in voxels:
            face([(p + 1, q, z), (p + 1, q + 1, z), (p + 1, q + 1, z + 1), (p + 1, q, z + 1)], shade("side"))
    return image


def fit(width, depth, voxels, color_of):
    """Rendered large, then shrunk smoothly to fill the canvas (less a margin), centered on it."""
    image = render(width, depth, voxels, color_of, 4)
    factor = min((CANVAS[0] - 8) / image.width, (CANVAS[1] - 8) / image.height)
    image = image.resize((round(image.width * factor), round(image.height * factor)), Image.LANCZOS)
    canvas = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    canvas.alpha_composite(image, ((CANVAS[0] - image.width) // 2, (CANVAS[1] - image.height) // 2))
    return canvas


# Where the animals of a pen stand in a picture, as fractions of how far they stray
ANIMAL_SPOTS = ((-1.0, -1.0), (1.0, 1.0), (1.0, -1.0), (-1.0, 1.0), (0.0, 0.0))


def parts_of(name):
    """(u, v, y, block) of the moving parts of a building model at rest, in its frame."""
    out = []
    try:
        lines = open(os.path.join(MODELS, "parts", "parts.txt")).read().splitlines()
    except OSError:
        return out
    animals = 0
    for line in lines:
        fields = line.split()
        if not fields or fields[0].startswith("#") or fields[0] != name:
            continue
        part_file, u, v, y, motion, a, b = fields[1], int(fields[2]), int(fields[3]), int(fields[4]), fields[5], float(fields[7]), float(fields[8])
        pw, pd, ph, part_voxels, _ = read_vox(os.path.join(MODELS, "parts", part_file + ".vox"))
        if motion == "wander":  # Standing on the ground, spread over the pen
            du, dv = ANIMAL_SPOTS[animals % len(ANIMAL_SPOTS)]
            animals += 1
            u0, v0, y0 = round(u + du * a) - pw // 2, round(v + dv * b) - pd // 2, y
        else:  # Centered on its pivot
            u0, v0, y0 = u - pw // 2, v - pd // 2, y - ph // 2
        for (x, q, z), index in part_voxels.items():
            out.append((u0 + pw - 1 - x, v0 + q, y0 + z, index))
    return out


def building(path, name):
    width, depth, height, file_voxels, palette = read_vox(path)
    # The game's building frame: u = width - 1 - x, v = y (v = 0 is the door side, nearest the viewer)
    voxels = {(width - 1 - x, depth - 1 - y, z + 1): index for (x, y, z), index in file_voxels.items()}
    for u, v, y, index in parts_of(name):
        voxels[(u, depth - 1 - v, y + 1)] = index
    for p in range(width):
        for q in range(depth):
            voxels.setdefault((p, q, 0), GRASS)
    def color_of(index):
        if index == GRASS:
            return (96, 150, 64)
        return palette[index - 1] if palette else (128, 128, 128)
    return fit(width, depth, voxels, color_of)


def road():
    """One road tile: worn stones on packed earth."""
    voxels = {}
    for p in range(12):
        for q in range(12):
            voxels[(p, q, 0)] = 1 if (p * 7 + q * 13) % 5 else 2
    return fit(12, 12, voxels, lambda index: (128, 118, 102) if index == 1 else (150, 140, 122))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    count = 0
    for path in sorted(glob.glob(os.path.join(MODELS, "*_1.vox"))):
        name = os.path.basename(path)[: -len("_1.vox")]
        building(path, name).save(os.path.join(OUT, name + ".tga"), compression=None)
        count += 1
    road().save(os.path.join(OUT, "road.tga"), compression=None)
    print(f"Wrote {count} building pictures and the road to {OUT}")
