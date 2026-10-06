"""Generates the building models in assets/buildings as MagicaVoxel (.vox) files.

Run from the repository root:  python tools/building_models/generate.py [model names, e.g. harbor_1]

Every model is built in the building's own frame (u across the front, v from the front at 0 to
the back, y up from the ground) and saved so that MagicaVoxel shows it the right way round: the
door side is MagicaVoxel's y = 0 side, z is up. Palette index = block ID (src/World/BlockTypes.h),
and the palette written into each file holds the in-game colors, so the files can be edited in
MagicaVoxel and stay valid. Sizes must stay footprint x 12 across and at most the type's height
(src/Simulation/BuildingTypes.h); see assets/buildings/README.md.
"""

import os
import struct
import sys

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
WOOL_WHITE, PIG_PINK, SAUSAGE, MUD = 96, 97, 98, 99
# Markers: not drawn; the game reads their positions (src/World/BuildingModel.h)
SMOKE_EMITTER, BOAT_BERTH = 100, 101

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
    96: (235, 232, 219), 97: (230, 158, 153), 98: (148, 56, 41), 99: (71, 51, 33),
    100: (90, 90, 90), 101: (0, 200, 255),  # Markers: smoke emitter, boat berth
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
    """A 3 x 3 brick chimney with a stone cap; smoke rises from its opening."""
    m.box(u, v, y0, u + 2, v + 2, y1, CHIMNEY_BRICK)
    m.box(u, v, y1, u + 2, v + 2, y1, STONE_DARK)
    m.set(u + 1, v + 1, y1, SMOKE_EMITTER)


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


# --- Producers (Milestone 4) -------------------------------------------------------------------

def log_pile(m, u0, u1, v0, rows):
    """Logs lying along u, stacked in a pyramid toward the top; cut ends are lighter."""
    for layer in range(rows):
        for v in range(v0 + layer, v0 + 2 * rows - layer - 1):
            m.box(u0, v, layer, u1, v, layer, TIMBER_LIGHT)
            m.set(u0, v, layer, PLASTER_CREAM)
            m.set(u1, v, layer, PLASTER_CREAM)


def sheep(m, u, v):
    """A sheep facing -v: wool body, dark head and legs."""
    m.box(u, v + 1, 1, u + 2, v + 4, 3, WOOL_WHITE)
    m.box(u, v, 2, u + 2, v, 3, STONE_DARK)
    for leg_u, leg_v in ((u, v + 1), (u + 2, v + 1), (u, v + 4), (u + 2, v + 4)):
        m.set(leg_u, leg_v, 0, STONE_DARK)


def pig(m, u, v):
    """A pig facing -u."""
    m.box(u + 1, v, 1, u + 4, v + 2, 2, PIG_PINK)
    m.box(u, v, 1, u, v + 2, 2, PIG_PINK)
    m.set(u, v + 1, 1, SAUSAGE)  # Snout
    for leg_u, leg_v in ((u + 1, v), (u + 4, v), (u + 1, v + 2), (u + 4, v + 2)):
        m.set(leg_u, leg_v, 0, PIG_PINK)


def fishery():
    """36 x 48 x 32: a fisher's hut on a paved quay (the two land rows) and a plank dock on pilings
    reaching out over the water (the two dock rows at the back). The model's base is 6 voxels below
    the ground (G), so the pilings stand in the sea; the boat berth marker at the dock's end is at
    the waterline (world y 42). Nets with drying fish, crates, barrels and mooring posts."""
    m = Model(36, 48, 32)
    G = 6  # Ground level in the model (world y 46)

    # Stone foundation under the land rows: where they stand over the shore's slope or the beach,
    # it fills the space down to the model's base (only solid voxels below ground are written)
    m.box(0, 0, 0, 35, 23, G - 1, STONE_DARK)
    for v in range(0, 17):
        for u in range(36):
            m.set(u, v, G - 1, 1)  # Grass on top (block 1), the same as the ground it continues
    # Paved quay along the water's edge (replaces the grass layer)
    for u in range(36):
        for v in range(17, 24):
            m.set(u, v, G - 1, COBBLE if (u // 3 + v // 3) % 2 else STONE_LIGHT)

    # Hut
    u0, u1, v0, v1 = 3, 18, 3, 15
    m.ring(u0, v0, u1, v1, G, G + 9, TIMBER_LIGHT)
    for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
        m.box(u, v, G, u, v, G + 9, TIMBER_DARK)
    m.ring(u0, v0, u1, v1, G + 9, G + 9, TIMBER_DARK)
    m.box(9, v0, G, 11, v0, G + 6, DOOR_WOOD)
    window(m, 14, v0, G + 3, 2, 3, SHUTTER_BLUE, None, facing=-1)
    side_window(m, u1, 8, G + 3, 3, 3, None, facing=+1)
    gable_roof(m, u0, u1, v0, v1, G + 9, G + 18, ROOF_TILE_DARK, ROOF_SLATE, overhang=2, gable_wall=TIMBER_LIGHT)
    chimney(m, 5, 10, G + 9, G + 21)

    # Net frames with fish drying
    for u in (22, 33):
        m.box(u, 4, G, u, 4, G + 9, TIMBER_DARK)
        m.box(u, 12, G, u, 12, G + 9, TIMBER_DARK)
    m.box(22, 4, G + 9, 33, 4, G + 9, TIMBER_DARK)
    m.box(22, 12, G + 9, 33, 12, G + 9, TIMBER_DARK)
    for u in range(23, 33):
        for y in range(G + 3, G + 9):
            if (u + y) % 2 == 0:
                m.set(u, 4, y, TIMBER_DARK)
    for u in range(24, 33, 3):
        m.box(u, 12, G + 5, u, 12, G + 7, FISH_SILVER)

    # The dock: plank deck flush with the quay, on pilings down into the sea
    d0, d1 = 11, 24
    for v in range(24, 48):
        for u in range(d0, d1 + 1):
            m.set(u, v, G - 1, TIMBER_LIGHT if (v // 2) % 2 == 0 else FENCE_WOOD)
    for v in (24, 30, 36, 42, 47):
        for u in (d0, d1):
            m.box(u, v, 0, u, v, G - 2, TIMBER_DARK)
    for v in (30, 40, 47):  # Mooring posts
        for u in (d0, d1):
            m.box(u, v, G, u, v, G + 2, TIMBER_DARK)
    crate(m, d0 + 2, 26, G)
    m.box(d0 + 3, 27, G + 3, d0 + 3, 27, G + 3, FISH_SILVER)
    barrel(m, d1 - 4, 28, G)
    for u, v in ((d1 - 3, 41), (d1 - 2, 40), (d1 - 1, 41), (d1 - 2, 42)):
        m.set(u, v, G, FENCE_WOOD)  # A coil of rope
    m.set((d0 + d1) // 2, 47, 2, BOAT_BERTH)  # Waterline at the dock's end: the boat moors here

    # Crates and barrels on the quay
    crate(m, 25, 18, G)
    crate(m, 29, 18, G)
    m.box(29, 18, G + 3, 31, 20, G + 3, FISH_SILVER)
    barrel(m, 3, 18, G)
    barrel(m, 7, 19, G)
    return m


def lumberjack():
    """36 x 36 x 24: a log cabin with log piles, a chopping block with an axe and a saw horse."""
    m = Model(36, 36, 24)
    u0, u1, v0, v1 = 5, 20, 8, 23
    for y in range(0, 10):
        m.ring(u0, v0, u1, v1, y, y, TIMBER_LIGHT if y % 2 == 0 else TIMBER_DARK)
    for u, v in ((u0 - 1, v0), (u1 + 1, v0), (u0 - 1, v1), (u1 + 1, v1), (u0, v0 - 1), (u1, v0 - 1), (u0, v1 + 1), (u1, v1 + 1)):
        for y in range(0, 10, 2):
            m.set(u, v, y, PLASTER_CREAM)  # Log ends at the corners
    m.box(11, v0, 0, 13, v0, 6, DOOR_WOOD)
    window(m, 16, v0, 3, 2, 3, SHUTTER_GREEN, None, facing=-1)
    side_window(m, u0, 14, 3, 3, 3, None, facing=-1)
    gable_roof(m, u0, u1, v0, v1, 9, 19, ROOF_SLATE, STONE_DARK, overhang=2, gable_wall=TIMBER_LIGHT)
    chimney(m, 7, 18, 9, 22)

    log_pile(m, 24, 34, 4, 3)
    log_pile(m, 24, 34, 13, 2)
    m.box(26, 20, 0, 27, 21, 1, BARREL)  # Chopping block
    m.box(27, 20, 2, 27, 20, 3, TIMBER_LIGHT)
    m.set(27, 20, 4, IRON)
    for i in range(3):  # Saw horse
        m.set(30 + i, 24, i, TIMBER_DARK)
        m.set(32 - i, 24, i, TIMBER_DARK)
    m.box(29, 24, 2, 33, 24, 2, TIMBER_LIGHT)
    for u, v in ((3, 28), (12, 30), (20, 27), (29, 31)):
        m.box(u, v, 0, u + 1, v + 1, 0, TIMBER_DARK)  # Stumps
    return m


def sawmill():
    """36 x 36 x 26: an open-sided shed over a saw bench, plank stacks inside, logs waiting at the
    back."""
    m = Model(36, 36, 26)
    u0, u1, v0, v1 = 3, 32, 6, 25
    for u in range(u0, u1 + 1, 6):
        for v in (v0, v1):
            m.box(u, v, 0, u, v, 11, TIMBER_DARK)
    for u in (u0, u1):
        m.box(u, v0, 0, u, v1, 0, TIMBER_DARK)
    m.ring(u0, v0, u1, v1, 11, 11, TIMBER_DARK)
    gable_roof(m, u0, u1, v0, v1, 11, 22, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2)
    m.box(u0, v1, 0, u1, v1, 5, TIMBER_LIGHT)  # Back wall, half height

    # Saw bench with a blade
    m.box(9, 14, 2, 26, 17, 2, TIMBER_LIGHT)
    for u, v in ((9, 14), (26, 14), (9, 17), (26, 17)):
        m.box(u, v, 0, u, v, 1, TIMBER_DARK)
    m.box(17, 15, 3, 18, 16, 5, IRON)
    m.box(10, 15, 3, 15, 16, 3, TIMBER_LIGHT)  # A log on the bench

    # Plank stacks with spacers
    for base_u in (5, 28):
        for layer in range(4):
            m.box(base_u, 8, layer, base_u + 2, 12, layer, TIMBER_LIGHT if layer % 2 == 0 else PLASTER_CREAM)
    log_pile(m, 8, 27, 27, 3)
    return m


def sheep_farm():
    """36 x 36 x 28: a barn with a fenced sheep yard and a hay rack."""
    m = Model(36, 36, 28)
    u0, u1, v0, v1 = 2, 16, 16, 33
    m.ring(u0, v0, u1, v1, 0, 10, TIMBER_LIGHT)
    for u in range(u0, u1 + 1, 2):
        m.box(u, v0, 0, u, v0, 10, TIMBER_DARK)
    m.box(6, v0, 0, 12, v0, 8, DOOR_WOOD)
    m.box(9, v0, 0, 9, v0, 8, TIMBER_DARK)
    gable_roof(m, u0, u1, v0, v1, 10, 22, THATCH, THATCH_DARK, overhang=2, gable_wall=TIMBER_LIGHT)

    fence(m, 19, 2, 34, 33, gate=(24, 27))
    m.box(22, 28, 0, 27, 31, 0, TIMBER_DARK)  # Hay rack
    m.box(22, 28, 1, 27, 31, 3, HAY)
    for u in (22, 27):
        m.box(u, 28, 1, u, 28, 4, TIMBER_DARK)
    for u, v in ((21, 5), (27, 7), (31, 12), (23, 15), (29, 20), (25, 23)):
        sheep(m, u, v)
    sheep(m, 7, 4)
    m.box(4, 10, 0, 7, 12, 1, HAY)
    return m


def framework_knitter():
    """36 x 36 x 34: a two-storey workshop: stone ground floor with big windows onto a loom,
    framed upper floor, wool bales outside."""
    m = Model(36, 36, 34)
    u0, u1, v0, v1 = 3, 32, 8, 31
    m.ring(u0, v0, u1, v1, 0, 10, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    for y in range(0, 11, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.box(16, v0, 0, 19, v0, 7, DOOR_WOOD)
    for left in (6, 22):  # Wide workshop windows
        m.box(left - 1, v0, 2, left + 7, v0, 8, WINDOW_FRAME)
        m.box(left, v0, 3, left + 6, v0, 7, WINDOW_GLASS)
    # The loom behind the left window, with wool on it
    m.box(7, v0 + 2, 0, 12, v0 + 2, 6, TIMBER_DARK)
    m.box(8, v0 + 2, 1, 11, v0 + 2, 5, WOOL_WHITE)
    m.box(7, v0 + 3, 0, 7, v0 + 5, 6, TIMBER_DARK)
    m.box(12, v0 + 3, 0, 12, v0 + 5, 6, TIMBER_DARK)

    m.box(u0, v0 - 1, 11, u1, v1, 11, TIMBER_DARK)
    timber_frame(m, u0, u1, v0 - 1, v1, 12, 21, PLASTER_CREAM, post_spacing=5)
    for u in (6, 12, 21, 27):
        window(m, u, v0 - 1, 14, 3, 4, SHUTTER_RED, None, facing=-1)
    gable_roof(m, u0, u1, v0 - 1, v1, 21, 32, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2, gable_wall=PLASTER_CREAM)

    for u in (4, 8):  # Wool bales tied with timber bands
        m.box(u, 2, 0, u + 2, 4, 2, WOOL_WHITE)
        m.box(u + 1, 2, 0, u + 1, 4, 2, TIMBER_DARK)
    m.box(6, 3, 3, 8, 5, 5, WOOL_WHITE)
    crate(m, 28, 2, 0)
    return m


def pig_farm():
    """36 x 36 x 22: a low stone sty with a thatched roof and a muddy pen with troughs and pigs."""
    m = Model(36, 36, 22)
    u0, u1, v0, v1 = 4, 19, 21, 33
    m.ring(u0, v0, u1, v1, 0, 5, STONE_DARK)
    m.box(9, v0, 0, 13, v0, 4, AIR)  # Open doorway into the pen
    gable_roof(m, u0, u1, v0, v1, 5, 14, THATCH, THATCH_DARK, overhang=2, gable_wall=TIMBER_LIGHT)

    fence(m, 1, 1, 34, 19, gate=(15, 18))
    for v in range(2, 19):
        for u in range(2, 34):
            if (u * 7 + v * 13) % 5 != 0:
                m.set(u, v, 0, MUD)
    for u0_trough in (5, 22):  # Troughs with water
        m.box(u0_trough, 16, 0, u0_trough + 6, 17, 1, TIMBER_DARK)
        m.box(u0_trough + 1, 16, 1, u0_trough + 5, 17, 1, WELL_WATER)
    for u, v in ((6, 4), (14, 7), (24, 4), (27, 10), (9, 11)):
        pig(m, u, v)
    m.box(24, 26, 0, 29, 29, 2, HAY)
    return m


def slaughterhouse():
    """36 x 36 x 32: a stone butcher's house with a smokehouse and its tall chimney, sausages
    hanging under a lean-to at the front, barrels and crates."""
    m = Model(36, 36, 32)
    u0, u1, v0, v1 = 3, 23, 10, 31
    m.ring(u0, v0, u1, v1, 0, 12, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    for y in range(0, 13, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.box(11, v0, 0, 14, v0, 8, DOOR_WOOD)
    for u in (6, 18):
        window(m, u, v0, 4, 2, 4, None, None, facing=-1)
    gable_roof(m, u0, u1, v0, v1, 12, 26, ROOF_TILE_DARK, ROOF_SLATE, overhang=2, gable_wall=STONE_LIGHT)

    # Smokehouse with its chimney
    m.ring(25, 16, 33, 27, 0, 9, TIMBER_DARK)
    m.box(25, 16, 10, 33, 27, 10, ROOF_SLATE)
    m.box(28, 20, 0, 31, 23, 30, CHIMNEY_BRICK)
    m.box(28, 20, 31, 31, 23, 31, STONE_DARK)
    m.box(29, 21, 31, 30, 22, 31, AIR)
    m.set(29, 21, 31, SMOKE_EMITTER)

    # Lean-to with hanging sausages at the front
    for u in (4, 22):
        m.box(u, 4, 0, u, 4, 9, TIMBER_DARK)
    m.box(3, 4, 10, 23, 9, 10, ROOF_TILE_RED)
    m.box(4, 5, 8, 22, 5, 8, TIMBER_DARK)
    for u in range(5, 22, 2):
        m.box(u, 5, 5, u, 5, 7, SAUSAGE)
    barrel(m, 26, 4, 0)
    barrel(m, 30, 6, 0)
    crate(m, 26, 9, 0)
    return m


def harbor():
    """48 x 60 x 40: a harbor office of stone and plastered timber with a red tile roof, a storage
    shed, and a wide pier on pilings (the two dock rows at the back) with bollards, crates, barrels
    and a small crane. As with the fishery the model's base is 6 below the ground (G); the boat
    berth marker at the pier's end is at the waterline, where ships moor."""
    m = Model(48, 60, 40)
    G = 6

    # Stone foundation under the land rows, grass on top, a paved quay along the water
    m.box(0, 0, 0, 47, 35, G - 1, STONE_DARK)
    for v in range(0, 28):
        for u in range(48):
            m.set(u, v, G - 1, 1)
    for u in range(48):
        for v in range(28, 36):
            m.set(u, v, G - 1, COBBLE if (u // 3 + v // 3) % 2 else STONE_LIGHT)

    # Harbor office: stone ground floor with quoins, plastered timber upper floor, red roof
    u0, u1, v0, v1 = 3, 26, 4, 22
    m.ring(u0, v0, u1, v1, G, G + 8, STONE_LIGHT)
    for y in range(G, G + 9, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.ring(u0, v0, u1, v1, G + 9, G + 16, PLASTER_WHITE)
    m.ring(u0, v0, u1, v1, G + 9, G + 9, TIMBER_DARK)
    m.ring(u0, v0, u1, v1, G + 16, G + 16, TIMBER_DARK)
    for u in range(u0, u1 + 1, 4):
        m.box(u, v0, G + 9, u, v0, G + 16, TIMBER_DARK)
        m.box(u, v1, G + 9, u, v1, G + 16, TIMBER_DARK)
    for v in range(v0, v1 + 1, 4):
        m.box(u0, v, G + 9, u0, v, G + 16, TIMBER_DARK)
        m.box(u1, v, G + 9, u1, v, G + 16, TIMBER_DARK)
    m.box(12, v0, G, 16, v0, G + 6, STONE_DARK)  # Arched door frame
    m.box(13, v0, G, 15, v0, G + 5, DOOR_WOOD)
    for u in (6, 20):
        window(m, u, v0, G + 3, 2, 3, SHUTTER_GREEN, None, facing=-1)
    for u in (6, 13, 20):
        window(m, u, v0, G + 11, 2, 3, SHUTTER_GREEN, FLOWER_RED if u != 13 else None, facing=-1)
    side_window(m, u1, 12, G + 11, 3, 3, SHUTTER_GREEN, facing=+1)
    gable_roof(m, u0, u1, v0, v1, G + 16, G + 28, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2, gable_wall=PLASTER_WHITE)
    chimney(m, 22, 18, G + 16, G + 31)
    # A flag pole by the door
    m.box(9, 1, G, 9, 1, G + 18, TIMBER_DARK)
    m.box(10, 1, G + 15, 13, 1, G + 17, AWNING_BLUE)

    # Open storage shed with crates and barrels
    m.box(30, 6, G, 30, 20, G + 9, TIMBER_DARK)
    m.box(44, 6, G, 44, 20, G + 9, TIMBER_DARK)
    m.box(30, 20, G, 44, 20, G + 9, TIMBER_LIGHT)
    m.box(29, 5, G + 10, 45, 21, G + 10, ROOF_SLATE)
    m.box(30, 6, G + 11, 44, 20, G + 11, ROOF_SLATE)
    for u, v in ((32, 9), (36, 9), (32, 13), (40, 15)):
        crate(m, u, v, G)
    crate(m, 34, 11, G + 3)
    for u, v in ((40, 8), (42, 11)):
        barrel(m, u, v, G)

    # The pier: plank deck flush with the quay, on pilings into the sea
    d0, d1 = 6, 41
    for v in range(36, 60):
        for u in range(d0, d1 + 1):
            m.set(u, v, G - 1, TIMBER_LIGHT if (v // 2) % 2 == 0 else FENCE_WOOD)
    for v in (36, 42, 48, 54, 59):
        for u in range(d0, d1 + 1, 7):
            m.box(u, v, 0, u, v, G - 2, TIMBER_DARK)
    for v in (40, 48, 56):  # Bollards along both edges
        for u in (d0, d1):
            m.box(u, v, G, u, v, G + 1, STONE_DARK)
            m.set(u, v, G + 2, IRON)
    # A small crane with a rope and a hanging crate
    m.box(36, 50, G, 37, 51, G + 14, TIMBER_DARK)
    m.box(28, 50, G + 14, 37, 50, G + 14, TIMBER_DARK)
    m.box(29, 50, G + 8, 29, 50, G + 13, FENCE_WOOD)
    crate(m, 28, 49, G + 5)
    # Cargo waiting on the pier
    for u, v in ((10, 39), (14, 39), (10, 43)):
        crate(m, u, v, G)
    crate(m, 12, 41, G + 3)
    for u, v in ((20, 40), (23, 40), (21, 43)):
        barrel(m, u, v, G)
    m.box(28, 38, G, 33, 41, G + 1, HAY)
    m.set((d0 + d1) // 2, 59, 2, BOAT_BERTH)  # Waterline at the pier's end: ships moor here
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
        'fishery_1': fishery(),
        'lumberjack_1': lumberjack(),
        'sawmill_1': sawmill(),
        'sheep_farm_1': sheep_farm(),
        'framework_knitter_1': framework_knitter(),
        'pig_farm_1': pig_farm(),
        'slaughterhouse_1': slaughterhouse(),
        'harbor_1': harbor(),
    }
    # Only the models named on the command line, if any (keeps hand edits to the others)
    wanted = sys.argv[1:]
    for name, model in models.items():
        if wanted and name not in wanted:
            continue
        path = os.path.join(out, name + '.vox')
        model.save(path)
        print('%-20s %2d x %2d x %2d  %6d voxels  -> %s' % (name, model.width, model.depth, model.height, len(model.voxels), path))


if __name__ == '__main__':
    main()
