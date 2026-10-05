"""Generates the building models in assets/buildings as MagicaVoxel (.vox) files.

Run from the repository root:  python tools/building_models/generate.py

Every model is built in the building's own frame (u across the front, v from the front at 0 to
the back, y up from the ground) and saved so that MagicaVoxel shows it the right way round: the
door side is MagicaVoxel's y = 0 side, z is up. Palette index = block ID (src/World/BlockTypes.h),
and the palette written into each file holds the in-game colors, so the files can be edited in
MagicaVoxel and stay valid. Sizes must stay footprint x 12 across and at most the type's height
(src/Simulation/BuildingTypes.h); see assets/buildings/README.md.
"""

import os
import struct

# --- Block IDs (src/World/BlockTypes.h) ------------------------------------------------------
AIR = 0
PLASTER_WHITE, PLASTER_CREAM, PLASTER_OCHRE = 60, 61, 62
TIMBER_DARK, TIMBER_LIGHT = 63, 64
THATCH, THATCH_DARK = 65, 66
ROOF_TILE_RED, ROOF_TILE_DARK, ROOF_SLATE = 67, 68, 69
STONE_LIGHT, STONE_DARK, COBBLE = 70, 71, 72
WINDOW_GLASS, WINDOW_FRAME, DOOR_WOOD = 73, 74, 75
SHUTTER_GREEN, SHUTTER_BLUE, SHUTTER_RED = 76, 77, 78
CHIMNEY_BRICK, FLOWER_RED, FLOWER_YELLOW, LEAVES = 79, 80, 81, 82
FENCE_WOOD, HAY, CRATE, BARREL, IRON = 83, 84, 85, 86, 87
AWNING_RED, AWNING_WHITE, AWNING_BLUE, AWNING_YELLOW = 88, 89, 90, 91
WELL_WATER, GARDEN_SOIL, VEGETABLES, FISH_SILVER = 92, 93, 94, 95

# In-game colors (shaders/render/shade.comp), for the .vox palette
COLORS = {
    1: (9, 186, 0), 2: (46, 28, 13), 3: (107, 107, 102), 4: (204, 184, 122), 34: (60, 160, 25), 35: (10, 56, 82),
    40: (77, 46, 20), 41: (158, 115, 66), 42: (140, 41, 26), 43: (179, 168, 148), 44: (92, 69, 43), 45: (128, 122, 112),
    46: (179, 31, 26), 47: (219, 163, 122), 48: (56, 46, 38), 49: (158, 107, 51), 50: (46, 92, 184),
    60: (224, 217, 199), 61: (219, 196, 148), 62: (204, 153, 87), 63: (61, 38, 23), 64: (133, 92, 51),
    65: (184, 153, 82), 66: (140, 110, 54), 67: (158, 51, 31), 68: (112, 36, 26), 69: (69, 77, 89),
    70: (168, 163, 148), 71: (112, 110, 102), 72: (133, 128, 117), 73: (41, 61, 82), 74: (235, 230, 214),
    75: (97, 56, 26), 76: (51, 107, 64), 77: (51, 82, 143), 78: (153, 46, 36), 79: (143, 66, 46),
    80: (219, 36, 41), 81: (245, 204, 41), 82: (51, 122, 36), 83: (148, 112, 71), 84: (219, 184, 89),
    85: (158, 115, 64), 86: (107, 66, 33), 87: (56, 56, 61), 88: (194, 36, 31), 89: (237, 227, 204),
    90: (46, 89, 179), 91: (230, 184, 46), 92: (26, 77, 115), 93: (77, 51, 31), 94: (82, 158, 46), 95: (179, 189, 199),
}


class Model:
    """A sparse voxel model in the building frame (u, v, y)."""

    def __init__(self, width, depth, height):
        self.width, self.depth, self.height = width, depth, height
        self.voxels = {}

    def set(self, u, v, y, block):
        if 0 <= u < self.width and 0 <= v < self.depth and 0 <= y < self.height:
            if block == AIR:
                self.voxels.pop((u, v, y), None)
            else:
                self.voxels[(u, v, y)] = block

    def get(self, u, v, y):
        return self.voxels.get((u, v, y), AIR)

    def box(self, u0, v0, y0, u1, v1, y1, block):
        """Fills an inclusive box."""
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for v in range(min(v0, v1), max(v0, v1) + 1):
                for u in range(min(u0, u1), max(u0, u1) + 1):
                    self.set(u, v, y, block)

    def ring(self, u0, v0, u1, v1, y0, y1, block):
        """The walls of a rectangle (inclusive), from y0 to y1."""
        self.box(u0, v0, y0, u1, v0, y1, block)
        self.box(u0, v1, y0, u1, v1, y1, block)
        self.box(u0, v0, y0, u0, v1, y1, block)
        self.box(u1, v0, y0, u1, v1, y1, block)

    def save(self, path):
        """Writes a MagicaVoxel file: SIZE, XYZI and RGBA chunks in MAIN."""
        def chunk(name, content, children=b''):
            return name + struct.pack('<ii', len(content), len(children)) + content + children

        size = chunk(b'SIZE', struct.pack('<iii', self.width, self.depth, self.height))
        xyzi = struct.pack('<i', len(self.voxels))
        for (u, v, y), block in sorted(self.voxels.items()):
            xyzi += struct.pack('<BBBB', self.width - 1 - u, v, y, block)
        palette = b''
        for index in range(1, 257):
            r, g, b = COLORS.get(index, (128, 128, 128))
            palette += struct.pack('<BBBB', r, g, b, 255)
        main = chunk(b'MAIN', b'', size + chunk(b'XYZI', xyzi) + chunk(b'RGBA', palette))
        with open(path, 'wb') as file:
            file.write(b'VOX ' + struct.pack('<i', 150) + main)


# --- Shared parts ------------------------------------------------------------------------------

def gable_roof(m, u0, u1, v0, v1, eave_y, ridge_y, block, ridge_block, overhang=2, gable_wall=None):
    """A roof over the box u0..u1 x v0..v1 (the walls), ridge along u, two voxels thick, with
    gable end walls filled up to its underside when gable_wall is given."""
    vs, ve = v0 - overhang, v1 + overhang
    half = (ve - vs) / 2.0
    rise = ridge_y - eave_y
    for v in range(vs, ve + 1):
        distance = min(v - vs, ve - v)
        y = eave_y + int(round(distance * rise / half))
        for u in range(u0 - overhang, u1 + overhang + 1):
            m.set(u, v, y, block)
            if vs < v < ve:
                m.set(u, v, y - 1, block)
        if gable_wall is not None and v0 < v < v1:
            for wall_y in range(eave_y, y - 1):
                m.set(u0, v, wall_y, gable_wall)
                m.set(u1, v, wall_y, gable_wall)
    for u in range(u0 - overhang, u1 + overhang + 1):  # Ridge cap
        m.set(u, (vs + ve) // 2, ridge_y + 1, ridge_block)
        m.set(u, (vs + ve + 1) // 2, ridge_y + 1, ridge_block)


def window(m, u, v, y, width, height, shutter=None, flowers=None, facing=-1):
    """A window in the wall plane v (front wall faces -v: facing = -1), with a frame, optional
    shutters standing out of the wall and an optional flower box under it."""
    m.box(u - 1, v, y - 1, u + width, v, y + height, WINDOW_FRAME)
    m.box(u, v, y, u + width - 1, v, y + height - 1, WINDOW_GLASS)
    out = v + facing
    if shutter is not None:
        m.box(u - 2, out, y, u - 2, out, y + height - 1, shutter)
        m.box(u + width + 1, out, y, u + width + 1, out, y + height - 1, shutter)
    if flowers is not None:
        m.box(u - 1, out, y - 2, u + width, out, y - 2, TIMBER_LIGHT)
        for i in range(u - 1, u + width + 1):
            m.set(i, out, y - 1, flowers if i % 2 == 0 else LEAVES)


def side_window(m, u, v, y, width, height, shutter=None, facing=-1):
    """A window in the wall plane u (a side wall), spanning v .. v + width - 1."""
    m.box(u, v - 1, y - 1, u, v + width, y + height, WINDOW_FRAME)
    m.box(u, v, y, u, v + width - 1, y + height - 1, WINDOW_GLASS)
    if shutter is not None:
        out = u + facing
        m.box(out, v - 2, y, out, v - 2, y + height - 1, shutter)
        m.box(out, v + width + 1, y, out, v + width + 1, y + height - 1, shutter)


def timber_frame(m, u0, u1, v0, v1, y0, y1, infill, post_spacing=5):
    """Plaster walls with dark timber posts, sill, middle and top beams, and corner braces."""
    m.ring(u0, v0, u1, v1, y0, y1, infill)
    for y in (y0, (y0 + y1) // 2, y1):
        m.ring(u0, v0, u1, v1, y, y, TIMBER_DARK)
    for u in list(range(u0, u1 + 1, post_spacing)) + [u1]:
        m.box(u, v0, y0, u, v0, y1, TIMBER_DARK)
        m.box(u, v1, y0, u, v1, y1, TIMBER_DARK)
    for v in list(range(v0, v1 + 1, post_spacing)) + [v1]:
        m.box(u0, v, y0, u0, v, y1, TIMBER_DARK)
        m.box(u1, v, y0, u1, v, y1, TIMBER_DARK)
    # Braces in the corner panels of the front and back
    mid = (y0 + y1) // 2
    for i in range(0, min(post_spacing, mid - y0) + 1):
        for v in (v0, v1):
            m.set(u0 + i, v, y0 + i, TIMBER_DARK)
            m.set(u1 - i, v, y0 + i, TIMBER_DARK)


def fence(m, u0, v0, u1, v1, gate=None):
    """A fence round a rectangle (posts every 3, two rails), with an optional gate gap (u range)
    in its front side v0."""
    def gap(u, v):
        return gate is not None and v == v0 and gate[0] <= u <= gate[1]
    for u in range(u0, u1 + 1):
        for v in (v0, v1):
            if gap(u, v):
                continue
            post = (u - u0) % 3 == 0
            for y in ((0, 1, 2, 3) if post else (1, 3)):
                m.set(u, v, y, FENCE_WOOD)
    for v in range(v0, v1 + 1):
        for u in (u0, u1):
            post = (v - v0) % 3 == 0
            for y in ((0, 1, 2, 3) if post else (1, 3)):
                m.set(u, v, y, FENCE_WOOD)


def barrel(m, u, v, y=0):
    m.box(u, v, y, u + 2, v + 2, y + 3, BARREL)
    for corner_u, corner_v in ((u, v), (u + 2, v), (u, v + 2), (u + 2, v + 2)):
        m.box(corner_u, corner_v, y, corner_u, corner_v, y + 3, AIR)  # Rounded
    for ring_y in (y + 1, y + 3):
        for i in range(3):
            for edge_u, edge_v in ((u + i, v), (u + i, v + 2), (u, v + i), (u + 2, v + i)):
                if m.get(edge_u, edge_v, ring_y) == BARREL:
                    m.set(edge_u, edge_v, ring_y, IRON)


def crate(m, u, v, y=0, size=3):
    m.box(u, v, y, u + size - 1, v + size - 1, y + size - 1, CRATE)
    for corner in ((u, v), (u + size - 1, v), (u, v + size - 1), (u + size - 1, v + size - 1)):
        m.box(corner[0], corner[1], y, corner[0], corner[1], y + size - 1, TIMBER_DARK)


def chimney(m, u, v, y0, y1):
    m.box(u, v, y0, u + 2, v + 2, y1, CHIMNEY_BRICK)
    m.box(u, v, y1, u + 2, v + 2, y1, STONE_DARK)
    m.set(u + 1, v + 1, y1, AIR)


# --- Buildings ---------------------------------------------------------------------------------

def farmer_house(variant):
    """36 x 36 x 30: a timber-framed cottage with a thatched roof, a fenced vegetable garden in
    front and a cobbled path to the door."""
    plaster = [PLASTER_WHITE, PLASTER_CREAM, PLASTER_OCHRE][variant]
    shutter = [SHUTTER_GREEN, SHUTTER_BLUE, SHUTTER_RED][variant]
    flowers = [FLOWER_RED, FLOWER_YELLOW, FLOWER_RED][variant]
    m = Model(36, 36, 30)
    u0, u1, v0, v1 = 5, 30, 12, 33

    # Plinth, walls, door, step
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    timber_frame(m, u0, u1, v0, v1, 2, 11, plaster, post_spacing=5)
    m.box(16, v0, 2, 19, v0, 9, DOOR_WOOD)
    m.box(15, v0, 2, 15, v0, 10, TIMBER_DARK)
    m.box(20, v0, 2, 20, v0, 10, TIMBER_DARK)
    m.box(15, v0, 10, 20, v0, 10, TIMBER_DARK)
    m.set(19, v0 - 1, 5, IRON)
    m.box(15, v0 - 2, 0, 20, v0 - 1, 0, COBBLE)
    m.box(15, v0 - 1, 1, 20, v0 - 1, 1, COBBLE)

    # Windows: front, back and sides
    for u in (8, 24):
        window(m, u, v0, 4, 3, 4, shutter, flowers, facing=-1)
        window(m, u, v1, 4, 3, 4, shutter, None, facing=+1)
    for v in (17, 26):
        side_window(m, u0, v, 4, 3, 4, shutter, facing=-1)
        side_window(m, u1, v, 4, 3, 4, shutter, facing=+1)

    # Thatched roof with plastered gables and a small gable window
    gable_roof(m, u0, u1, v0, v1, 11, 26, THATCH, THATCH_DARK, overhang=2, gable_wall=plaster)
    for u in (u0, u1):
        m.box(u, 21, 15, u, 24, 18, WINDOW_FRAME)
        m.box(u, 22, 16, u, 23, 17, WINDOW_GLASS)
    chimney(m, [9, 23, 16][variant], 27, 2, 29)

    # Front garden: fence with a gate, a cobbled path, vegetable beds and a yard corner
    fence(m, 1, 1, 34, v0 - 1, gate=(15, 20))
    m.box(16, 2, 0, 19, v0 - 3, 0, COBBLE)
    for v in range(3, 9):
        m.box(3, v, 0, 13, v, 0, GARDEN_SOIL)
        if v % 2 == 1:
            m.box(3, v, 1, 13, v, 1, VEGETABLES)
    if variant == 0:
        m.box(23, 4, 0, 26, 6, 1, HAY)
        m.box(28, 4, 0, 31, 6, 1, HAY)
        m.box(25, 4, 2, 29, 6, 2, HAY)
    elif variant == 1:
        barrel(m, 24, 4)
        barrel(m, 28, 4)
        crate(m, 26, 7)
    else:
        for v in range(3, 9):
            m.box(22, v, 0, 32, v, 0, GARDEN_SOIL)
            m.box(22, v, 1, 32, v, 1, FLOWER_YELLOW if v % 2 else LEAVES)

    # The third variant has a lean-to shed on its right side
    if variant == 2:
        m.ring(u1 + 1, 20, 34, 31, 0, 6, TIMBER_LIGHT)
        for u in range(u1, 35):
            m.box(u, 19, 7 + (34 - u) // 2, u, 32, 7 + (34 - u) // 2, THATCH)
        crate(m, 32, 22, 0, 2)
    return m


def worker_house(variant):
    """36 x 36 x 42: a two-storey town house: masonry ground floor with corner quoins, a framed
    and plastered upper floor jettied out over the street, tiled roof with a dormer, two chimneys."""
    plaster = [PLASTER_WHITE, PLASTER_CREAM][variant]
    roof = [ROOF_TILE_RED, ROOF_SLATE][variant]
    ridge = [ROOF_TILE_DARK, STONE_DARK][variant]
    shutter = [SHUTTER_GREEN, SHUTTER_BLUE][variant]
    m = Model(36, 36, 42)
    u0, u1, v0, v1 = 3, 32, 7, 33

    # Ground floor: stone with dark quoins, a plinth and an arched door
    m.ring(u0, v0, u1, v1, 0, 11, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    for y in range(0, 12, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.box(15, v0, 1, 19, v0, 9, DOOR_WOOD)
    m.box(14, v0, 1, 14, v0, 10, STONE_DARK)
    m.box(20, v0, 1, 20, v0, 10, STONE_DARK)
    m.box(15, v0, 10, 19, v0, 10, STONE_DARK)
    m.box(16, v0, 11, 18, v0, 11, STONE_DARK)
    m.set(22, v0 - 1, 8, IRON)  # Lantern
    m.set(22, v0 - 1, 7, WINDOW_GLASS)
    for u in (6, 10, 24, 28):
        window(m, u, v0, 3, 2, 5, None, None, facing=-1)
        window(m, u, v1, 3, 2, 5, None, None, facing=+1)

    # Upper floor, one voxel out over the street at the front
    uv0 = v0 - 1
    m.box(u0, uv0, 12, u1, v1, 12, TIMBER_DARK)
    timber_frame(m, u0, u1, uv0, v1, 13, 23, plaster, post_spacing=5)
    for u in (6, 11, 16, 22, 27):
        window(m, u, uv0, 16, 3, 5, shutter, FLOWER_RED if u in (6, 27) else None, facing=-1)
        window(m, u, v1, 16, 3, 5, shutter, None, facing=+1)
    for v in (12, 20, 27):
        side_window(m, u0, v, 16, 3, 5, None, facing=-1)
        side_window(m, u1, v, 16, 3, 5, None, facing=+1)

    # Tiled roof, dormer on the front slope, two chimneys
    gable_roof(m, u0, u1, uv0, v1, 23, 39, roof, ridge, overhang=2, gable_wall=plaster)
    m.box(15, 11, 27, 19, 14, 32, plaster)
    m.box(16, 10, 28, 18, 10, 30, WINDOW_GLASS)
    m.box(15, 10, 27, 19, 10, 27, WINDOW_FRAME)
    m.box(15, 10, 31, 19, 10, 31, WINDOW_FRAME)
    for step in range(3):
        m.box(14 + step, 9, 33 + step, 20 - step, 15, 33 + step, roof)
    chimney(m, 7, 24, 12, 41)
    chimney(m, 26, 24, 12, 41)

    # Cobbled forecourt with flower tubs
    m.box(0, 0, 0, 35, v0 - 2, 0, COBBLE)
    for u in (2, 31):
        barrel(m, u, 1, 1)
        m.box(u, 1, 5, u + 2, 3, 5, LEAVES)
        m.set(u + 1, 2, 6, FLOWER_RED if variant == 0 else FLOWER_YELLOW)
    return m


def warehouse():
    """48 x 48 x 40: a stone and timber storehouse with big double doors, a hoist over a loft
    door, crates and barrels on a cobbled apron."""
    m = Model(48, 48, 40)
    u0, u1, v0, v1 = 4, 43, 9, 43

    # Stone base with quoins, plank upper walls with posts
    m.ring(u0, v0, u1, v1, 0, 9, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    for y in range(0, 10, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.ring(u0, v0, u1, v1, 10, 19, TIMBER_LIGHT)
    for y in (10, 19):
        m.ring(u0, v0, u1, v1, y, y, TIMBER_DARK)
    for u in range(u0, u1 + 1, 6):
        m.box(u, v0, 10, u, v0, 19, TIMBER_DARK)
        m.box(u, v1, 10, u, v1, 19, TIMBER_DARK)
    for v in range(v0, v1 + 1, 6):
        m.box(u0, v, 10, u0, v, 19, TIMBER_DARK)
        m.box(u1, v, 10, u1, v, 19, TIMBER_DARK)

    # Double doors with Z-braces and a stone frame
    m.box(17, v0, 1, 30, v0, 15, STONE_DARK)
    m.box(18, v0, 1, 29, v0, 14, DOOR_WOOD)
    m.box(23, v0, 1, 24, v0, 14, TIMBER_DARK)
    for half in (18, 25):
        for y in (2, 8, 13):
            m.box(half, v0 - 1, y, half + 4, v0 - 1, y, TIMBER_DARK)
        for i in range(5):
            m.set(half + i, v0 - 1, 3 + i, TIMBER_DARK)
    for u in (8, 14, 34, 40):
        window(m, u, v0, 13, 2, 3, None, None, facing=-1)

    # Slate roof, loft door and hoist beam with a rope and hook
    gable_roof(m, u0, u1, v0, v1, 19, 37, ROOF_SLATE, STONE_DARK, overhang=2, gable_wall=TIMBER_LIGHT)
    m.box(20, v0 - 1, 20, 27, v0 + 2, 26, TIMBER_LIGHT)
    m.box(21, v0 - 1, 20, 26, v0 - 1, 25, DOOR_WOOD)
    m.box(19, v0 - 2, 27, 28, v0 + 2, 28, ROOF_SLATE)
    m.box(23, v0 - 6, 26, 24, v0, 26, TIMBER_DARK)
    m.box(23, v0 - 6, 19, 23, v0 - 6, 25, IRON)
    m.set(23, v0 - 6, 18, IRON)
    m.set(24, v0 - 6, 18, IRON)

    # Cobbled apron, crates and barrels
    m.box(0, 0, 0, 47, v0 - 1, 0, COBBLE)
    crate(m, 3, 2, 1)
    crate(m, 7, 2, 1)
    crate(m, 5, 2, 4)
    crate(m, 11, 3, 1, 2)
    for u in (36, 40):
        barrel(m, u, 2, 1)
    barrel(m, 38, 5, 1)
    m.box(1, 12, 0, 3, 18, 2, HAY)
    return m


def marketplace():
    """48 x 36 x 24: a paved square with four striped market stalls round a roofed well."""
    m = Model(48, 36, 24)

    # Paving: checked stone and cobble
    for v in range(36):
        for u in range(48):
            m.set(u, v, 0, STONE_LIGHT if (u // 3 + v // 3) % 2 == 0 else COBBLE)

    # The well: stone ring, water, posts, beam, little tiled roof and a bucket
    m.box(20, 14, 1, 27, 21, 3, STONE_LIGHT)
    m.box(21, 15, 1, 26, 20, 3, AIR)
    m.box(21, 15, 1, 26, 20, 1, WELL_WATER)
    for u in (20, 27):
        m.box(u, 17, 4, u, 18, 11, TIMBER_DARK)
    m.box(20, 17, 11, 27, 18, 11, TIMBER_DARK)
    for step in range(3):
        m.box(19, 15 + step, 12 + step, 28, 20 - step, 12 + step, ROOF_TILE_RED)
    m.box(23, 17, 7, 24, 18, 8, IRON)

    # Four stalls facing the well, each with its own awning colors and goods
    stalls = [
        (2, 2, (AWNING_RED, AWNING_WHITE), VEGETABLES, +1),
        (34, 2, (AWNING_BLUE, AWNING_WHITE), FISH_SILVER, +1),
        (2, 24, (AWNING_YELLOW, AWNING_WHITE), FLOWER_RED, -1),
        (34, 24, (AWNING_RED, AWNING_YELLOW), HAY, -1),
    ]
    for u0, v0, (stripe_a, stripe_b), goods, toward in stalls:
        u1, v1 = u0 + 11, v0 + 9
        front, back = (v1, v0) if toward > 0 else (v0, v1)
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.box(u, v, 1, u, v, 12, TIMBER_DARK)
        # Counter with goods on it, crates and a barrel behind
        m.box(u0 + 1, front, 1, u1 - 1, front, 3, CRATE)
        m.box(u0 + 1, front, 4, u1 - 1, front, 4, goods)
        m.box(u0 + 1, front - toward, 4, u1 - 1, front - toward, 4, goods if goods != HAY else VEGETABLES)
        crate(m, u0 + 2, back - (2 if toward > 0 else 0), 1)
        barrel(m, u0 + 7, back - (2 if toward > 0 else 0), 1)
        # Sloping striped awning, high at the back, low over the counter
        for v in range(min(v0, v1) - 1, max(v0, v1) + 2):
            along = (v - back) * toward  # 0 at the back, growing toward the front
            y = 14 - along // 3
            for u in range(u0 - 1, u1 + 2):
                m.set(u, v, y, stripe_a if (u // 2) % 2 == 0 else stripe_b)
    return m


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out = os.path.join(root, 'assets', 'buildings')
    os.makedirs(out, exist_ok=True)
    models = {
        'farmer_house_1': farmer_house(0),
        'farmer_house_2': farmer_house(1),
        'farmer_house_3': farmer_house(2),
        'worker_house_1': worker_house(0),
        'worker_house_2': worker_house(1),
        'warehouse_1': warehouse(),
        'marketplace_1': marketplace(),
    }
    for name, model in models.items():
        path = os.path.join(out, name + '.vox')
        model.save(path)
        print('%-16s %2d x %2d x %2d  %6d voxels  -> %s' % (name, model.width, model.depth, model.height, len(model.voxels), path))


if __name__ == '__main__':
    main()
