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
# More materials (Milestone 8)
WHEAT, WHEAT_STALK, CLAY, COAL, IRON_ORE, STEEL, FIRE = 160, 161, 162, 163, 164, 165, 166
CATTLE_BROWN, CATTLE_WHITE, BREAD, SOAP, BRASS, VELVET, BRICK_RED = 167, 168, 169, 170, 171, 172, 173
PLASTER_BLUE, PLASTER_PINK, ROOF_GREEN, SACK, CANVAS = 174, 175, 176, 177, 178

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
    160: (224, 184, 77), 161: (158, 163, 61), 162: (168, 97, 56), 163: (26, 26, 28), 164: (115, 71, 51),
    165: (140, 148, 158), 166: (255, 140, 38), 167: (107, 61, 31), 168: (230, 224, 214), 169: (194, 128, 56),
    170: (219, 224, 204), 171: (204, 163, 61), 172: (128, 20, 31), 173: (168, 77, 51), 174: (140, 168, 194),
    175: (219, 168, 158), 176: (92, 148, 128), 177: (204, 184, 140), 178: (230, 222, 199),
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
    """36 x 36 x 28: the shepherd's farmstead: a timber-framed farmhouse with a thatched roof and a
    chimney, an open shearing shed with wool bales and a shearing bench, hay ricks and a water
    trough. The sheep themselves live in the sheepfolds placed around it."""
    m = Model(36, 36, 28)
    # Farmhouse at the back left
    u0, u1, v0, v1 = 2, 19, 14, 33
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    timber_frame(m, u0, u1, v0, v1, 2, 11, PLASTER_CREAM, post_spacing=4)
    m.box(9, v0, 2, 12, v0, 9, DOOR_WOOD)
    m.box(9, v0 - 2, 0, 12, v0 - 1, 0, COBBLE)
    window(m, 4, v0, 5, 3, 3, SHUTTER_GREEN, FLOWER_YELLOW, facing=-1)
    window(m, 15, v0, 5, 2, 3, SHUTTER_GREEN, None, facing=-1)
    side_window(m, u0, 22, 5, 3, 3, SHUTTER_GREEN, facing=-1)
    gable_roof(m, u0, u1, v0, v1, 11, 24, THATCH, THATCH_DARK, overhang=2, gable_wall=PLASTER_CREAM)
    chimney(m, 14, 27, 2, 27)

    # Open shearing shed at the right: posts, a slate lean-to roof, wool inside
    s0, s1, t0, t1 = 23, 34, 14, 33
    for u, v in ((s0, t0), (s1, t0), (s0, t1), (s1, t1), (s0, 23), (s1, 23)):
        m.box(u, v, 0, u, v, 10 - (u - s0) // 3, TIMBER_DARK)
    for u in range(s0 - 1, s1 + 2):
        m.box(u, t0 - 1, 11 - (u - s0) // 3, u, t1 + 1, 11 - (u - s0) // 3, ROOF_SLATE)
    m.box(s1, t0, 0, s1, t1, 4, TIMBER_LIGHT)  # Back wall, half height
    m.box(25, 17, 0, 30, 19, 2, TIMBER_LIGHT)  # Shearing bench
    m.box(26, 17, 3, 29, 19, 4, WOOL_WHITE)    # A fleece on it
    m.set(30, 18, 3, IRON)                     # Shears
    for u, v in ((25, 26), (29, 26), (25, 30)):  # Wool bales tied with bands
        m.box(u, v, 0, u + 2, v + 2, 2, WOOL_WHITE)
        m.box(u + 1, v, 0, u + 1, v + 2, 2, TIMBER_DARK)
    m.box(27, 28, 3, 29, 30, 5, WOOL_WHITE)

    # Yard in front: hay ricks, a trough, a cart of hay, a fence along the front
    fence(m, 0, 0, 35, 10, gate=(8, 14))
    for u in (24, 30):
        m.box(u, 3, 0, u + 3, 6, 2, HAY)
        m.box(u + 1, 4, 3, u + 2, 5, 4, HAY)
    m.box(3, 3, 0, 4, 8, 1, TIMBER_DARK)  # Trough with water
    m.box(4, 4, 1, 4, 7, 1, WELL_WATER)
    m.box(16, 3, 1, 21, 6, 1, TIMBER_LIGHT)  # Hay cart
    m.box(16, 3, 2, 21, 6, 3, HAY)
    for u, v in ((16, 2), (21, 2), (16, 7), (21, 7)):
        m.box(u, v, 0, u, v, 1, TIMBER_DARK)
    m.box(14, 4, 1, 15, 4, 1, TIMBER_DARK)  # Shafts
    return m


def sheepfold():
    """36 x 36 x 14: a fenced sheep pasture (a farm module): grass inside a post-and-rail fence, a
    small thatched shelter, a hay rack, a trough and a flock of sheep."""
    m = Model(36, 36, 14)
    fence(m, 0, 0, 35, 35, gate=(15, 19))
    # Shelter in the back corner: posts and a thatched lean-to
    for u, v in ((24, 25), (33, 25), (24, 33), (33, 33)):
        m.box(u, v, 0, u, v, 7 - (33 - v) // 3, TIMBER_DARK)
    for v in range(24, 35):
        m.box(23, v, 8 - (34 - v) // 3, 34, v, 8 - (34 - v) // 3, THATCH)
    m.box(24, 33, 0, 33, 33, 4, TIMBER_LIGHT)
    m.box(26, 29, 0, 30, 31, 1, HAY)  # Straw bedding
    # Hay rack and trough
    m.box(4, 28, 0, 9, 31, 0, TIMBER_DARK)
    m.box(4, 28, 1, 9, 31, 3, HAY)
    for u in (4, 9):
        m.box(u, 28, 1, u, 28, 4, TIMBER_DARK)
    m.box(3, 8, 0, 4, 15, 1, TIMBER_DARK)
    m.box(4, 9, 1, 4, 14, 1, WELL_WATER)
    for u, v in ((8, 5), (14, 10), (21, 6), (27, 13), (10, 18), (19, 21), (28, 4)):
        sheep(m, u, v)
    for u, v in ((6, 22), (31, 19), (16, 28)):  # Flowers in the grass
        m.set(u, v, 0, FLOWER_YELLOW)
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
    """36 x 48 x 26: the swineherd's farmstead: a stone farmhouse with a tiled roof and a chimney,
    a feed barn with grain sacks and swill barrels, a muck heap and a covered feed store. The pigs
    live in the pigsties placed around it."""
    m = Model(36, 48, 26)
    # Farmhouse at the back
    u0, u1, v0, v1 = 3, 22, 28, 45
    m.ring(u0, v0, u1, v1, 0, 9, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    for y in range(0, 10, 2):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, STONE_DARK)
    m.box(11, v0, 0, 14, v0, 7, DOOR_WOOD)
    m.box(11, v0 - 3, 0, 14, v0 - 1, 0, COBBLE)
    for u in (5, 18):
        window(m, u, v0, 3, 2, 3, SHUTTER_RED, FLOWER_RED, facing=-1)
    side_window(m, u0, 35, 3, 3, 3, SHUTTER_RED, facing=-1)
    gable_roof(m, u0, u1, v0, v1, 9, 21, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2, gable_wall=STONE_LIGHT)
    chimney(m, 6, 40, 2, 24)

    # Feed barn at the front right: timber walls, a wide door, thatch
    b0, b1, c0, c1 = 18, 33, 4, 20
    for y in range(0, 9):
        m.ring(b0, c0, b1, c1, y, y, TIMBER_LIGHT if y % 3 else TIMBER_DARK)
    m.box(22, c0, 0, 28, c0, 7, AIR)  # Open double door
    m.box(21, c0, 0, 21, c0, 8, TIMBER_DARK)
    m.box(29, c0, 0, 29, c0, 8, TIMBER_DARK)
    gable_roof(m, b0, b1, c0, c1, 8, 17, THATCH, THATCH_DARK, overhang=2, gable_wall=TIMBER_LIGHT)
    for u, v in ((20, 8), (23, 8), (20, 12)):  # Grain sacks inside
        m.box(u, v, 0, u + 1, v + 2, 2, PLASTER_CREAM)
    m.box(26, 14, 0, 31, 18, 2, HAY)
    barrel(m, 29, 7)

    # Yard: swill barrels and a trough, a muck heap, a feed store on posts, a cart path
    for u, v in ((3, 3), (7, 3)):
        barrel(m, u, v)
    m.box(3, 8, 0, 11, 9, 1, TIMBER_DARK)
    m.box(4, 8, 1, 10, 9, 1, MUD)  # Swill in the trough
    for v in range(14, 22):  # Muck heap
        for u in range(3, 10):
            h = 2 - (abs(u - 6) + abs(v - 18)) // 3
            if h >= 0:
                m.box(u, v, 0, u, v, h, MUD)
    m.set(5, 17, 3, HAY)
    m.box(12, 0, 0, 16, v0 - 4, 0, COBBLE)  # Path from the road to the house
    fence(m, 0, 0, 35, 47, gate=(11, 17))
    return m


def pigsty():
    """24 x 36 x 12: a pigsty (a farm module): a muddy pen inside a low fence, a stone hut with a
    thatched roof at the back, troughs and pigs."""
    m = Model(24, 36, 12)
    for v in range(1, 35):
        for u in range(1, 23):
            if (u * 7 + v * 13) % 6 != 0:
                m.set(u, v, 0, MUD)
    fence(m, 0, 0, 23, 35, gate=(9, 13))
    # Hut at the back: stone walls, open front, thatch
    h0, h1, k0, k1 = 3, 20, 24, 33
    m.ring(h0, k0, h1, k1, 0, 4, STONE_DARK)
    m.box(8, k0, 0, 14, k0, 3, AIR)
    gable_roof(m, h0, h1, k0, k1, 4, 9, THATCH, THATCH_DARK, overhang=1, gable_wall=TIMBER_LIGHT)
    m.box(h0 + 1, k0 + 1, 0, h1 - 1, k1 - 1, 0, HAY)  # Straw inside
    # Troughs
    m.box(2, 3, 0, 3, 10, 1, TIMBER_DARK)
    m.box(3, 4, 1, 3, 9, 1, WELL_WATER)
    m.box(15, 3, 0, 21, 4, 1, TIMBER_DARK)
    m.box(16, 3, 1, 20, 3, 1, MUD)
    for u, v in ((6, 6), (12, 12), (16, 8), (5, 17), (14, 19)):
        pig(m, u, v)
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



# --- Milestone 8.1: the rest of the Workers, and the Artisans ---------------------------------

def quoins(m, u0, v0, u1, v1, y0, y1, block=STONE_DARK, step=2):
    """Darker corner stones every step layers."""
    for y in range(y0, y1 + 1, step):
        for u, v in ((u0, v0), (u1, v0), (u0, v1), (u1, v1)):
            m.set(u, v, y, block)


def brick_walls(m, u0, v0, u1, v1, y0, y1):
    """Red brick walls, a darker course every fourth layer."""
    m.ring(u0, v0, u1, v1, y0, y1, BRICK_RED)
    for y in range(y0 + 3, y1 + 1, 4):
        m.ring(u0, v0, u1, v1, y, y, CHIMNEY_BRICK)


def arched_window(m, u, v, y, width, height, facing=-1, frame=STONE_LIGHT):
    """A tall window with a stone sill and a stepped arch over it, in the wall plane v."""
    m.box(u, v, y, u + width - 1, v, y + height - 1, WINDOW_GLASS)
    m.box(u - 1, v + facing, y - 1, u + width, v + facing, y - 1, frame)  # Sill
    m.box(u - 1, v, y + height, u + width, v, y + height, frame)
    m.box(u, v, y + height + 1, u + width - 1, v, y + height + 1, frame)
    if width > 2:
        m.box(u + 1, v, y + height - 1, u + width - 2, v, y + height - 1, WINDOW_FRAME)  # Glazing bar
        m.box(u + width // 2, v, y, u + width // 2, v, y + height - 1, WINDOW_FRAME)


def factory_chimney(m, u, v, y0, y1, size=4):
    """A tall square brick chimney with stone bands and a smoking opening at the top."""
    m.box(u, v, y0, u + size - 1, v + size - 1, y1, CHIMNEY_BRICK)
    for y in range(y0 + 6, y1, 7):
        m.box(u, v, y, u + size - 1, v + size - 1, y, STONE_DARK)
    m.box(u - 1, v - 1, y1 - 1, u + size, v + size, y1 - 1, STONE_DARK)  # Cap
    m.box(u + 1, v + 1, y1, u + size - 2, v + size - 2, y1, AIR)
    m.set(u + size // 2, v + size // 2, y1, SMOKE_EMITTER)


def disc_cells(cu, cv, r):
    """Columns (u, v) inside a circle of radius r around (cu, cv)."""
    for v in range(int(cv - r) - 1, int(cv + r) + 2):
        for u in range(int(cu - r) - 1, int(cu + r) + 2):
            if (u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2 <= r * r:
                yield u, v


def cylinder(m, cu, cv, r, y0, y1, block, hollow=False):
    for u, v in disc_cells(cu, cv, r):
        if hollow and (u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2 < (r - 1.2) ** 2:
            continue
        m.box(u, v, y0, u, v, y1, block)


def dome(m, cu, cv, r, y0, block):
    """A half sphere on the layer y0."""
    for dy in range(int(r) + 1):
        radius = (r * r - dy * dy) ** 0.5
        for u, v in disc_cells(cu, cv, radius):
            m.set(u, v, y0 + dy, block)


def sacks(m, u, v, y, count, block=SACK):
    """Sacks side by side along u, 2 x 2 x 2, tied at the top."""
    for i in range(count):
        m.box(u + 3 * i, v, y, u + 3 * i + 1, v + 1, y + 1, block)
        m.set(u + 3 * i, v, y + 2, block)


def tree(m, u, v, y=0, height=9, crown=4):
    """A small leafy tree."""
    m.box(u, v, y, u, v, y + height - 2, TIMBER_DARK)
    for dy in range(-crown, crown + 1):
        radius = (crown * crown - dy * dy) ** 0.5
        for cu, cv in disc_cells(u + 0.5, v + 0.5, radius):
            if (cu * 3 + cv * 5 + dy) % 7 != 0:
                m.set(cu, cv, y + height + dy, LEAVES)


def cow(m, u, v):
    """A cow facing -u: brown with white patches, horns."""
    for x in range(u + 2, u + 8):
        for z in range(v, v + 3):
            for y in (2, 3, 4):
                patch = (x * 5 + z * 3 + y * 7) % 9 < 3
                m.set(x, z, y, CATTLE_WHITE if patch else CATTLE_BROWN)
    for leg_u, leg_v in ((u + 2, v), (u + 7, v), (u + 2, v + 2), (u + 7, v + 2)):
        m.box(leg_u, leg_v, 0, leg_u, leg_v, 1, CATTLE_BROWN)
    m.box(u, v + 1, 3, u + 1, v + 1, 5, CATTLE_BROWN)  # Head
    m.set(u, v + 1, 3, CATTLE_WHITE)                   # Muzzle
    m.set(u + 1, v, 6, PLASTER_WHITE)                  # Horns
    m.set(u + 1, v + 2, 6, PLASTER_WHITE)
    m.set(u + 8, v + 1, 3, CATTLE_BROWN)               # Tail


def lamp_post(m, u, v, height=9):
    m.box(u, v, 0, u, v, height, IRON)
    m.set(u, v, height + 1, WINDOW_GLASS)
    m.set(u, v, height + 2, IRON)


def school():
    """36 x 36 x 38: a red brick schoolhouse with tall arched windows, a slate roof and a bell
    turret on the ridge; a fenced schoolyard with a tree, benches and a flag pole in front."""
    m = Model(36, 36, 38)
    u0, u1, v0, v1 = 3, 32, 12, 32
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    brick_walls(m, u0, v0, u1, v1, 2, 15)
    quoins(m, u0, v0, u1, v1, 2, 15, STONE_LIGHT)
    # Front: the door under a stone porch, two arched windows each side
    m.box(15, v0, 2, 20, v0, 10, STONE_LIGHT)
    m.box(16, v0, 2, 19, v0, 9, DOOR_WOOD)
    m.box(14, v0 - 3, 11, 21, v0, 11, ROOF_SLATE)  # Porch roof on two posts
    for u in (14, 21):
        m.box(u, v0 - 3, 0, u, v0 - 3, 10, TIMBER_DARK)
    m.box(15, v0 - 3, 0, 20, v0 - 1, 0, STONE_LIGHT)
    for u in (5, 10, 24, 29):
        arched_window(m, u, v0, 5, 3, 7, facing=-1)
        arched_window(m, u, v1, 5, 3, 7, facing=+1)
    for v in (17, 22, 27):
        m.box(u0, v, 5, u0, v + 1, 11, WINDOW_GLASS)
        m.box(u1, v, 5, u1, v + 1, 11, WINDOW_GLASS)
    gable_roof(m, u0, u1, v0, v1, 15, 28, ROOF_SLATE, STONE_DARK, overhang=2, gable_wall=BRICK_RED)
    # Round window in each gable
    for u in (u0, u1):
        m.box(u, 21, 19, u, 23, 21, WINDOW_FRAME)
        m.set(u, 22, 20, WINDOW_GLASS)
    # Bell turret: white posts, a brass bell, a little pointed roof with a finial
    t0, t1, w0, w1 = 15, 20, 20, 25
    m.box(t0, w0, 27, t1, w1, 28, PLASTER_WHITE)
    for u, v in ((t0, w0), (t1, w0), (t0, w1), (t1, w1)):
        m.box(u, v, 29, u, v, 32, PLASTER_WHITE)
    m.box(17, 22, 30, 18, 23, 31, BRASS)
    m.box(17, 22, 29, 18, 23, 29, BRASS)
    for step in range(3):
        m.box(t0 - 1 + step, w0 - 1 + step, 33 + step, t1 + 1 - step, w1 + 1 - step, 33 + step, ROOF_SLATE)
    m.box(17, 22, 36, 18, 23, 37, IRON)
    chimney(m, 6, 26, 14, 30)
    # Schoolyard: fence, gravel, a tree, benches and a flag
    fence(m, 0, 0, 35, v0 - 1, gate=(15, 20))
    m.box(1, 1, 0, 34, v0 - 2, 0, COBBLE)
    tree(m, 5, 5, 1, height=9, crown=4)
    for u in (24, 29):
        m.box(u, 3, 1, u + 3, 4, 1, TIMBER_LIGHT)
        m.set(u, 3, 0, TIMBER_DARK)
        m.set(u + 3, 3, 0, TIMBER_DARK)
    m.box(33, 8, 1, 33, 8, 16, IRON)
    m.box(29, 8, 13, 32, 8, 15, AWNING_BLUE)
    m.box(29, 8, 14, 32, 8, 14, AWNING_WHITE)
    return m


def grain_farm():
    """36 x 36 x 28: the farmstead of the grain fields: a big barn with open doors and sacks of
    grain on the threshing floor, a granary on staddle stones, wheat sheaves and a cart."""
    m = Model(36, 36, 28)
    # Barn
    u0, u1, v0, v1 = 2, 21, 12, 33
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    for y in range(2, 12):
        m.ring(u0, v0, u1, v1, y, y, TIMBER_LIGHT if y % 3 else TIMBER_DARK)
    for u in range(u0, u1 + 1, 5):
        m.box(u, v0, 2, u, v0, 11, TIMBER_DARK)
        m.box(u, v1, 2, u, v1, 11, TIMBER_DARK)
    m.box(8, v0, 0, 15, v0, 9, AIR)  # Open double doors, folded back
    m.box(6, v0 - 1, 0, 7, v0 - 1, 9, DOOR_WOOD)
    m.box(16, v0 - 1, 0, 17, v0 - 1, 9, DOOR_WOOD)
    m.box(8, v0 - 3, 0, 15, v0 + 6, 0, STONE_LIGHT)  # Threshing floor
    gable_roof(m, u0, u1, v0, v1, 11, 25, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2, gable_wall=TIMBER_LIGHT)
    m.box(9, v0, 14, 14, v0, 18, DOOR_WOOD)  # Loft door
    sacks(m, 9, v0 + 2, 1, 2)
    m.box(4, v0 + 3, 2, 7, v1 - 3, 5, WHEAT)  # Sheaves stored inside
    # Granary on staddle stones
    g0, g1, h0, h1 = 25, 33, 18, 30
    for u, v in ((g0, h0), (g1, h0), (g0, h1), (g1, h1), (29, h0), (29, h1)):
        m.box(u, v, 0, u, v, 1, STONE_LIGHT)
        m.set(u, v, 2, STONE_DARK)
    m.box(g0, h0, 3, g1, h1, 3, TIMBER_DARK)
    m.ring(g0, h0, g1, h1, 4, 10, TIMBER_LIGHT)
    m.box(28, h0, 4, 30, h0, 8, DOOR_WOOD)
    gable_roof(m, g0, g1, h0, h1, 10, 17, THATCH, THATCH_DARK, overhang=1, gable_wall=TIMBER_LIGHT)
    for i in range(3):
        m.box(28, h0 - 1 - i, 3 - i, 30, h0 - 1 - i, 3 - i, TIMBER_LIGHT)  # Steps
    # Yard: wheat sheaves (stooks), a cart with sacks, a well
    for u, v in ((25, 3), (30, 4), (27, 9)):
        m.box(u, v, 0, u + 2, v + 2, 3, WHEAT)
        m.box(u + 1, v + 1, 4, u + 1, v + 1, 5, WHEAT)
        m.box(u, v + 1, 1, u + 2, v + 1, 1, WHEAT_STALK)
    m.box(2, 3, 1, 9, 7, 1, TIMBER_LIGHT)
    m.ring(2, 3, 9, 7, 2, 2, TIMBER_LIGHT)
    sacks(m, 3, 4, 2, 2)
    for u, v in ((2, 2), (9, 2), (2, 8), (9, 8)):
        m.box(u, v, 0, u, v, 1, TIMBER_DARK)
    m.box(10, 5, 1, 12, 5, 1, TIMBER_DARK)
    return m


def wheat_field():
    """36 x 36 x 10: a grain field (a farm module): rows of ripe wheat over furrows, a few sheaves
    and a scarecrow."""
    m = Model(36, 36, 10)
    for v in range(36):
        for u in range(36):
            if v % 4 == 3:
                m.set(u, v, 0, GARDEN_SOIL)  # Furrow
                continue
            top = 3 + (u * 7 + v * 11) % 3 // 2
            m.box(u, v, 0, u, v, top - 1, WHEAT_STALK)
            m.set(u, v, top, WHEAT)
            if (u * 13 + v * 5) % 4 == 0:
                m.set(u, v, top + 1, WHEAT)
    # A scarecrow in the middle
    m.box(17, 17, 0, 17, 17, 8, TIMBER_DARK)
    m.box(14, 17, 6, 20, 17, 6, TIMBER_DARK)
    m.box(16, 17, 4, 18, 17, 6, SHUTTER_BLUE)
    m.box(16, 17, 7, 18, 17, 8, SACK)
    m.box(15, 16, 9, 19, 18, 9, HAY)
    return m


def flour_mill():
    """36 x 36 x 64: a tower windmill: a tapering round tower of white plaster on a stone base,
    a balcony round it, a thatched cap and four lattice sails with canvas; sacks of flour by the
    door."""
    m = Model(36, 36, 64)
    cu, cv = 18.0, 21.0
    for y in range(0, 41):
        r = 10.5 - y * 0.09
        block = STONE_DARK if y < 2 else (STONE_LIGHT if y < 8 else PLASTER_WHITE)
        cylinder(m, cu, cv, r, y, y, block, hollow=True)
    # Door, windows, the balcony at y 16
    m.box(16, 10, 0, 19, 12, 7, DOOR_WOOD)
    m.box(16, 9, 0, 19, 9, 0, STONE_LIGHT)
    for y, u in ((12, 13), (22, 21), (30, 15)):
        m.box(u, 11, y, u + 1, 13, y + 2, WINDOW_GLASS)
    for u, v in disc_cells(cu, cv, 13.5):
        if (u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2 > 9.4 ** 2:
            m.set(u, v, 16, TIMBER_LIGHT)
            if (u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2 > 12.4 ** 2 and (u + v) % 2 == 0:
                m.box(u, v, 17, u, v, 18, FENCE_WOOD)
    # The cap: a thatched dome, and the shaft out of the front
    dome(m, cu, cv, 7.5, 41, THATCH)
    m.box(17, 21, 48, 18, 22, 49, THATCH_DARK)
    m.box(17, 7, 43, 18, 14, 44, TIMBER_DARK)
    hub_u, hub_y, sail_v = 18.0, 44.0, 6
    m.box(16, sail_v, 42, 19, sail_v + 1, 45, TIMBER_DARK)
    # Four sails in an X: a spar along each diagonal, canvas on one side with a lattice
    length = 19.0
    for u in range(36):
        for y in range(20, 64):
            du, dy = u + 0.5 - hub_u, y + 0.5 - hub_y
            dist = (du * du + dy * dy) ** 0.5
            if dist < 2.0 or dist > length:
                continue
            for a, b in ((du, dy), (-du, dy)):
                along = (a + b) / 2 ** 0.5    # Along this diagonal
                across = (a - b) / 2 ** 0.5   # Across it
                if abs(across) < 0.8:
                    m.set(u, sail_v, y, TIMBER_DARK)
                elif 0.8 <= across * (1 if along > 0 else -1) < 6.2 and abs(along) > 4:
                    lattice = int(abs(along)) % 4 == 0 or abs(across) > 5.6
                    m.set(u, sail_v, y, TIMBER_LIGHT if lattice else CANVAS)
    # Flour sacks and a cart by the door
    sacks(m, 4, 4, 0, 3, PLASTER_WHITE)
    sacks(m, 5, 4, 3, 2, PLASTER_WHITE)
    m.box(25, 3, 1, 32, 7, 1, TIMBER_LIGHT)
    sacks(m, 26, 4, 2, 2, PLASTER_WHITE)
    for u, v in ((25, 2), (32, 2), (25, 8), (32, 8)):
        m.box(u, v, 0, u, v, 1, TIMBER_DARK)
    return m


def bakery():
    """36 x 36 x 34: a half-timbered bakery with a shop window under a yellow awning, bread on
    the counter, a brick oven built onto its side with a smoking chimney, flour sacks and a
    wood pile."""
    m = Model(36, 36, 34)
    u0, u1, v0, v1 = 3, 25, 10, 31
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    timber_frame(m, u0, u1, v0, v1, 2, 12, PLASTER_OCHRE, post_spacing=4)
    m.box(u0, v0 - 1, 13, u1, v1, 13, TIMBER_DARK)
    timber_frame(m, u0, u1, v0 - 1, v1, 14, 21, PLASTER_CREAM, post_spacing=4)
    m.box(17, v0, 2, 20, v0, 10, DOOR_WOOD)
    # Shop window with bread on the sill, an awning over it
    m.box(5, v0, 4, 14, v0, 10, WINDOW_FRAME)
    m.box(6, v0, 5, 13, v0, 9, WINDOW_GLASS)
    m.box(5, v0 - 1, 3, 14, v0 - 2, 3, TIMBER_LIGHT)
    for u in range(5, 15, 2):
        m.set(u, v0 - 2, 4, BREAD)
        m.set(u + 1, v0 - 1, 4, BREAD)
    for v in range(v0 - 4, v0):
        y = 12 - (v0 - v) // 2
        m.box(4, v, y, 15, v, y, AWNING_YELLOW if (v % 2) else AWNING_WHITE)
    # A sign: a pretzel on an iron bracket
    m.box(21, v0 - 3, 12, 21, v0 - 1, 12, IRON)
    m.box(20, v0 - 3, 9, 22, v0 - 3, 11, BREAD)
    m.set(21, v0 - 3, 10, AIR)
    for u in (6, 12, 18):
        window(m, u, v0 - 1, 16, 3, 3, SHUTTER_GREEN, FLOWER_RED if u == 12 else None, facing=-1)
    gable_roof(m, u0, u1, v0 - 1, v1, 21, 31, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=2, gable_wall=PLASTER_CREAM)
    # Brick oven on the right side: a vaulted bulge and its chimney
    for v in range(14, 28):
        for u in range(u1 + 1, 34):
            h = int(9 - ((u - u1) * 0.5) ** 2 / 3)
            m.box(u, v, 0, u, v, max(2, h), BRICK_RED)
    m.box(31, 14, 2, 32, 15, 4, FIRE)  # The fire door
    m.box(30, 14, 5, 33, 14, 5, CHIMNEY_BRICK)
    chimney(m, 27, 20, 9, 33)
    # Yard: flour sacks, a wood pile for the oven
    sacks(m, 3, 3, 0, 2, PLASTER_WHITE)
    log_pile(m, 26, 34, 2, 2)
    return m


def rendering_works():
    """36 x 36 x 34: a grimy stone boiling house where pig fat is rendered: big iron vats on a
    platform, a tall brick chimney, barrels of tallow stacked outside."""
    m = Model(36, 36, 34)
    u0, u1, v0, v1 = 3, 22, 13, 33
    m.ring(u0, v0, u1, v1, 0, 11, STONE_DARK)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_LIGHT)
    m.box(9, v0, 0, 13, v0, 8, DOOR_WOOD)
    for u in (5, 17):
        m.box(u, v0, 4, u + 2, v0, 8, WINDOW_GLASS)
    gable_roof(m, u0, u1, v0, v1, 11, 21, ROOF_TILE_DARK, ROOF_SLATE, overhang=2, gable_wall=STONE_DARK)
    factory_chimney(m, 25, 26, 0, 33, size=4)
    # Two vats on a timber platform, with steps and fire under them
    m.box(23, 8, 0, 34, 20, 2, TIMBER_DARK)
    for cu in (26.5, 31.5):
        cylinder(m, cu, 14.5, 3.0, 3, 9, STEEL, hollow=True)
        cylinder(m, cu, 14.5, 2.0, 8, 8, SOAP)  # Melted fat
        for y in (4, 7):
            for u, v in disc_cells(cu, 14.5, 3.0):
                if (u + 0.5 - cu) ** 2 + (v + 0.5 - 14.5) ** 2 > 1.9 ** 2:
                    m.set(u, v, y, IRON)
        m.box(int(cu) - 1, 11, 1, int(cu), 11, 2, FIRE)
    for i in range(3):
        m.box(23 - i - 1, 12, 2 - i, 23 - i - 1, 16, 2 - i, TIMBER_LIGHT)
    # Tallow barrels and a pig pen gate
    for u, v in ((2, 3), (6, 3), (10, 3), (4, 6), (8, 6)):
        barrel(m, u, v)
    barrel(m, 4, 3, 4)
    barrel(m, 8, 3, 4)
    for u in range(14, 21, 3):
        crate(m, u, 4)
    return m


def soap_factory():
    """36 x 36 x 40: a two-storey brick workshop with tall arched windows, a blue plastered upper
    floor, a factory chimney, soap bars drying on racks and crates of soap."""
    m = Model(36, 36, 40)
    u0, u1, v0, v1 = 3, 28, 12, 33
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    brick_walls(m, u0, v0, u1, v1, 1, 12)
    quoins(m, u0, v0, u1, v1, 1, 12, STONE_LIGHT)
    m.box(14, v0, 1, 17, v0, 9, DOOR_WOOD)
    m.box(13, v0, 10, 18, v0, 10, STONE_LIGHT)
    for u in (5, 9, 21, 25):
        arched_window(m, u, v0, 3, 2, 6, facing=-1)
    m.box(u0, v0, 13, u1, v1, 13, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 14, 22, PLASTER_BLUE)
    quoins(m, u0, v0, u1, v1, 14, 22, STONE_LIGHT, step=1)
    for u in (6, 11, 16, 21):
        window(m, u, v0, 16, 3, 4, None, None, facing=-1)
        window(m, u, v1, 16, 3, 4, None, None, facing=+1)
    gable_roof(m, u0, u1, v0, v1, 22, 34, ROOF_SLATE, STONE_DARK, overhang=2, gable_wall=PLASTER_BLUE)
    factory_chimney(m, 30, 26, 0, 39, size=4)
    # Drying racks with rows of soap bars
    for rack_v in (3, 7):
        for u in (19, 33):
            m.box(u, rack_v, 0, u, rack_v, 6, TIMBER_DARK)
        for y in (2, 4, 6):
            m.box(19, rack_v, y, 33, rack_v, y, TIMBER_LIGHT)
            for u in range(20, 33, 2):
                m.set(u, rack_v, y + 1 if y < 6 else y, SOAP)
    for u in (3, 7, 11):
        crate(m, u, 3)
    m.box(4, 4, 3, 5, 5, 3, SOAP)
    crate(m, 5, 3, 3)
    return m


def clay_pit():
    """36 x 36 x 24: a clay pit: wet clay dug out in steps with puddles, a hand winch over the
    hole, clay blocks drying in stacks, a wheelbarrow and the diggers' shed."""
    m = Model(36, 36, 24)
    # The pit: clay ground with mounds round it and water in the hollow
    for v in range(36):
        for u in range(36):
            d = ((u - 15) ** 2 / 1.4 + (v - 20) ** 2) ** 0.5
            if d < 11:
                m.set(u, v, 0, WELL_WATER if d < 4 and (u + v) % 3 else CLAY)
            elif d < 15:
                h = 1 + int((15 - d) * 0.6) + (u * 7 + v * 3) % 2
                m.box(u, v, 0, u, v, h, CLAY)
    # A winch on a timber frame over the water
    for u in (10, 20):
        m.box(u, 20, 1, u, 20, 9, TIMBER_DARK)
    m.box(10, 20, 9, 20, 20, 9, TIMBER_LIGHT)
    m.box(15, 20, 4, 15, 20, 8, FENCE_WOOD)
    m.box(14, 19, 2, 16, 21, 3, BARREL)
    # Stacks of drying clay blocks
    for u0, v0 in ((27, 4), (27, 12), (2, 2)):
        for layer in range(4):
            for i in range(0, 6, 2):
                m.box(u0 + i, v0, layer, u0 + i, v0 + 4, layer, CLAY if layer % 2 == 0 else BRICK_RED)
        m.box(u0 - 1, v0 - 1, 5, u0 + 6, v0 + 5, 5, THATCH)
        for u, v in ((u0 - 1, v0 - 1), (u0 + 6, v0 - 1), (u0 - 1, v0 + 5), (u0 + 6, v0 + 5)):
            m.box(u, v, 0, u, v, 4, TIMBER_DARK)
    # Wheelbarrow
    m.box(20, 4, 1, 22, 6, 2, TIMBER_LIGHT)
    m.box(21, 5, 2, 21, 5, 2, CLAY)
    m.set(23, 5, 1, IRON)
    m.box(18, 4, 1, 19, 4, 1, TIMBER_DARK)
    m.box(18, 6, 1, 19, 6, 1, TIMBER_DARK)
    # The diggers' shed at the back right
    m.ring(26, 25, 34, 34, 0, 7, TIMBER_LIGHT)
    m.box(29, 25, 0, 31, 25, 5, DOOR_WOOD)
    gable_roof(m, 26, 34, 25, 34, 7, 12, THATCH, THATCH_DARK, overhang=1, gable_wall=TIMBER_LIGHT)
    m.box(24, 28, 0, 24, 28, 6, TIMBER_DARK)  # A spade leaning on it
    m.set(24, 28, 0, IRON)
    return m


def brick_factory():
    """36 x 36 x 44: a brickworks: a long kiln of brick with glowing fire holes along its side, a
    tall chimney, rows of bricks drying under an open shed and a stack of fired bricks."""
    m = Model(36, 36, 44)
    # The kiln: a long low vault
    k0, k1, w0, w1 = 3, 26, 18, 32
    for u in range(k0, k1 + 1):
        for v in range(w0, w1 + 1):
            h = int(10 - ((v - (w0 + w1) / 2) ** 2) / 6)
            m.box(u, v, 0, u, v, max(3, h), BRICK_RED)
    for u in range(k0, k1 + 1, 3):
        for v in range(w0, w1 + 1):
            h = int(10 - ((v - (w0 + w1) / 2) ** 2) / 6)
            m.set(u, v, max(3, h), CHIMNEY_BRICK)
    for u in range(k0 + 2, k1, 5):
        m.box(u, w0, 1, u + 1, w0, 3, FIRE)
        m.box(u - 1, w0, 4, u + 2, w0, 4, STONE_DARK)
    factory_chimney(m, 28, 23, 0, 43, size=5)
    # The drying shed: posts, a long tiled roof, bricks in open rows
    s0, s1, t0, t1 = 2, 33, 3, 13
    for u in range(s0, s1 + 1, 6):
        for v in (t0, t1):
            m.box(u, v, 0, u, v, 7, TIMBER_DARK)
    for v in range(t0 - 1, t1 + 2):
        y = 8 + (v - t0 + 1) // 3
        m.box(s0 - 1, v, y, s1 + 1, v, y, ROOF_TILE_RED)
    for v in range(t0 + 2, t1 - 1, 3):
        for layer in range(4):
            for u in range(s0 + 2, s1 - 1):
                if (u + layer) % 2 == 0:
                    m.set(u, v, layer, CLAY if layer > 1 else BRICK_RED)
    # Fired bricks on pallets
    for u in (28, 32):
        m.box(u, 15, 0, u + 2, 16, 0, TIMBER_DARK)
        m.box(u, 15, 1, u + 2, 16, 4, BRICK_RED)
    return m


def artisan_house(variant):
    """36 x 36 x 54: a three-storey town house: a shop on the ground floor with big windows and
    an awning, plastered floors above with a balcony, a cornice and a mansard roof with dormers."""
    plaster = [PLASTER_BLUE, PLASTER_PINK, PLASTER_OCHRE][variant]
    awning = [(AWNING_RED, AWNING_WHITE), (AWNING_BLUE, AWNING_WHITE), (AWNING_YELLOW, AWNING_RED)][variant]
    shutter = [SHUTTER_GREEN, SHUTTER_BLUE, SHUTTER_RED][variant]
    m = Model(36, 36, 54)
    u0, u1, v0, v1 = 3, 32, 6, 33
    # Ground floor: stone, the shop front with two windows and a door
    m.ring(u0, v0, u1, v1, 0, 11, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    quoins(m, u0, v0, u1, v1, 0, 11)
    for left in (5, 21):
        m.box(left, v0, 2, left + 9, v0, 9, WINDOW_FRAME)
        m.box(left + 1, v0, 3, left + 8, v0, 8, WINDOW_GLASS)
        m.box(left + 1, v0 + 1, 2, left + 8, v0 + 1, 3, CRATE)  # Wares behind the glass
    m.box(16, v0, 1, 19, v0, 9, DOOR_WOOD)
    m.set(18, v0 - 1, 5, BRASS)
    for v in range(v0 - 4, v0):
        y = 12 - (v0 - v) // 2
        for u in range(4, 32):
            m.set(u, v, y, awning[0] if (u // 2) % 2 == 0 else awning[1])
    m.box(4, v0, 11, 31, v0, 11, TIMBER_DARK)  # Sign board
    # Two plastered floors with tall windows and a balcony
    m.box(u0, v0, 12, u1, v1, 12, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 13, 34, plaster)
    quoins(m, u0, v0, u1, v1, 13, 34, STONE_LIGHT, step=1)
    m.box(u0, v0, 23, u1, v1, 23, STONE_LIGHT)
    for floor_y in (15, 25):
        for u in (6, 12, 21, 27):
            window(m, u, v0, floor_y, 3, 6, shutter, None, facing=-1)
            window(m, u, v1, floor_y, 3, 6, None, None, facing=+1)
        for v in (13, 20, 27):
            side_window(m, u0, v, floor_y, 3, 6, None, facing=-1)
            side_window(m, u1, v, floor_y, 3, 6, None, facing=+1)
    m.box(16, v0, 13, 19, v0, 21, WINDOW_GLASS)  # Balcony door
    m.box(14, v0 - 3, 13, 21, v0 - 1, 13, STONE_LIGHT)
    for u in range(14, 22):
        m.set(u, v0 - 3, 14, IRON)
        m.set(u, v0 - 3, 15, IRON if u % 2 == 0 else AIR)
        m.set(u, v0 - 3, 16, IRON)
    m.box(14, v0 - 2, 14, 14, v0 - 1, 16, IRON)
    m.box(21, v0 - 2, 14, 21, v0 - 1, 16, IRON)
    m.box(14, v0 - 3, 17, 14, v0 - 3, 17, FLOWER_RED)
    m.box(21, v0 - 3, 17, 21, v0 - 3, 17, FLOWER_YELLOW)
    # Cornice, then a mansard roof (steep, then flat) with dormers
    m.box(u0 - 1, v0 - 1, 35, u1 + 1, v1 + 1, 35, STONE_LIGHT)
    for step in range(10):
        m.ring(u0 + step // 2, v0 + step // 2, u1 - step // 2, v1 - step // 2, 36 + step, 36 + step, ROOF_SLATE)
    m.box(u0 + 5, v0 + 5, 46, u1 - 5, v1 - 5, 46, ROOF_SLATE)
    m.box(u0 + 6, v0 + 6, 47, u1 - 6, v1 - 6, 47, STONE_DARK)
    for u in (8, 16, 24):
        m.box(u, v0 + 1, 37, u + 4, v0 + 3, 42, plaster)
        m.box(u + 1, v0 + 1, 38, u + 3, v0 + 1, 41, WINDOW_GLASS)
        m.box(u - 1, v0, 43, u + 5, v0 + 4, 43, ROOF_SLATE)
    chimney(m, 7, 26, 35, 52)
    chimney(m, 26, 26, 35, 52)
    # Pavement in front
    m.box(0, 0, 0, 35, v0 - 1, 0, STONE_LIGHT)
    for u in (1, 33):
        lamp_post(m, u, 1, 9)
    return m


def variety_theatre():
    """48 x 48 x 50: a grand theatre: a stone front with a columned portico and a pediment,
    velvet doors and brass lamps, playbills, ochre walls and a green copper dome with a lantern,
    and a paved square in front."""
    m = Model(48, 48, 50)
    u0, u1, v0, v1 = 4, 43, 13, 44
    # Hall
    m.ring(u0, v0, u1, v1, 0, 1, STONE_DARK)
    m.ring(u0, v0, u1, v1, 2, 24, PLASTER_OCHRE)
    quoins(m, u0, v0, u1, v1, 2, 24, STONE_LIGHT, step=1)
    for y in (12, 24):
        m.ring(u0, v0, u1, v1, y, y, STONE_LIGHT)
    for v in range(v0 + 4, v1 - 3, 6):
        for u in (u0, u1):
            m.box(u, v, 4, u, v + 2, 9, WINDOW_GLASS)
            m.box(u, v, 15, u, v + 2, 21, WINDOW_GLASS)
    m.box(u0 - 1, v0 - 1, 25, u1 + 1, v1 + 1, 25, STONE_LIGHT)  # Cornice
    m.box(u0, v0, 26, u1, v1, 26, ROOF_SLATE)
    m.box(u0 + 1, v0 + 1, 27, u1 - 1, v1 - 1, 27, ROOF_SLATE)
    # Front: doors, playbills, a balcony of windows
    for u in (14, 22, 30):
        m.box(u, v0, 2, u + 3, v0, 9, VELVET)
        m.box(u - 1, v0, 10, u + 4, v0, 10, BRASS)
    for u in (8, 37):
        m.box(u, v0 - 1, 4, u + 2, v0 - 1, 9, PLASTER_WHITE)  # Playbills
        m.box(u, v0 - 1, 7, u + 2, v0 - 1, 7, VELVET)
        m.set(u + 1, v0 - 1, 5, FLOWER_RED)
    for u in range(10, 38, 5):
        arched_window(m, u, v0, 14, 3, 7, facing=-1)
    # Portico: a stone floor, six columns, an entablature and a pediment
    m.box(9, 3, 0, 38, v0 - 1, 1, STONE_LIGHT)
    for u in (10, 15, 20, 27, 32, 37):
        m.box(u, 4, 2, u + 1, 5, 21, PLASTER_WHITE)
        m.box(u - 1, 3, 2, u + 2, 6, 2, STONE_LIGHT)
        m.box(u - 1, 3, 21, u + 2, 6, 21, STONE_LIGHT)
    m.box(8, 2, 22, 39, v0 - 1, 24, STONE_LIGHT)
    m.box(8, 2, 23, 39, 2, 23, BRASS)  # The name band
    for step in range(9):
        m.box(8 + step * 2, 2, 25 + step, 39 - step * 2, v0 - 1, 25 + step, STONE_LIGHT)
    m.box(22, 2, 27, 25, 2, 29, VELVET)  # A crest in the pediment
    for u in (12, 35):
        m.box(u, 3, 10, u, 3, 11, BRASS)
        m.set(u, 3, 12, WINDOW_GLASS)
    # The dome, a lantern and a finial
    dome(m, 23.5, 30.0, 11.0, 27, ROOF_GREEN)
    for u, v in disc_cells(23.5, 30.0, 11.5):
        if (u + 0.5 - 23.5) ** 2 + (v - 29.5) ** 2 > 10.2 ** 2:
            m.set(u, v, 27, STONE_LIGHT)
    cylinder(m, 23.5, 30.0, 2.5, 38, 42, PLASTER_WHITE)
    for u, v in disc_cells(23.5, 30.0, 2.5):
        if (u + v) % 2 == 0:
            m.set(u, v, 40, WINDOW_GLASS)
    dome(m, 23.5, 30.0, 3.0, 43, ROOF_GREEN)
    m.box(23, 29, 46, 24, 30, 49, BRASS)
    # Square with lamps
    for v in range(0, 3):
        for u in range(48):
            m.set(u, v, 0, COBBLE)
    for u in (2, 45):
        lamp_post(m, u, 6, 11)
    return m


def cattle_farm():
    """36 x 36 x 28: a cattle farm: a stone byre with a hay loft, a farmhouse with a red roof,
    milk churns and a hay rick; the cattle graze in the pastures round it."""
    m = Model(36, 36, 28)
    # Byre: long stone building with a timber loft
    u0, u1, v0, v1 = 2, 33, 20, 33
    m.ring(u0, v0, u1, v1, 0, 7, STONE_LIGHT)
    quoins(m, u0, v0, u1, v1, 0, 7)
    m.ring(u0, v0, u1, v1, 8, 11, TIMBER_LIGHT)
    for u in range(5, 32, 7):
        m.box(u, v0, 0, u + 2, v0, 5, DOOR_WOOD)
        m.box(u - 1, v0, 6, u + 3, v0, 6, TIMBER_DARK)
    m.box(14, v0, 8, 18, v0, 11, AIR)  # Loft door with hay
    m.box(14, v0 + 1, 8, 18, v0 + 2, 10, HAY)
    m.box(16, v0 - 3, 12, 16, v0, 12, TIMBER_DARK)  # Hoist beam
    gable_roof(m, u0, u1, v0, v1, 11, 19, ROOF_TILE_DARK, ROOF_SLATE, overhang=2, gable_wall=TIMBER_LIGHT)
    # Farmhouse at the front left
    h0, h1, w0, w1 = 2, 15, 3, 14
    m.ring(h0, w0, h1, w1, 0, 1, STONE_DARK)
    timber_frame(m, h0, h1, w0, w1, 2, 10, PLASTER_WHITE, post_spacing=4)
    m.box(7, w0, 2, 9, w0, 8, DOOR_WOOD)
    window(m, 11, w0, 5, 2, 3, SHUTTER_GREEN, FLOWER_RED, facing=-1)
    gable_roof(m, h0, h1, w0, w1, 10, 17, ROOF_TILE_RED, ROOF_TILE_DARK, overhang=1, gable_wall=PLASTER_WHITE)
    chimney(m, 4, 10, 10, 20)
    # Yard: milk churns, a hay rick, a calf
    for u in (19, 21, 23):
        cylinder(m, u + 0.5, 4.5, 1.0, 0, 2, STEEL)
        m.set(u, 4, 3, STEEL)
    m.box(27, 5, 0, 33, 11, 3, HAY)
    m.box(28, 6, 4, 32, 10, 5, HAY)
    m.box(29, 7, 6, 31, 9, 6, HAY)
    m.box(19, 10, 1, 21, 11, 2, CATTLE_BROWN)
    m.box(18, 10, 2, 18, 11, 3, CATTLE_BROWN)
    for u, v in ((19, 10), (21, 10), (19, 11), (21, 11)):
        m.set(u, v, 0, CATTLE_BROWN)
    return m


def pasture():
    """36 x 36 x 12: a cattle pasture (a farm module): a fenced meadow with cows, a water trough,
    a shade tree and a salt lick."""
    m = Model(36, 36, 12)
    fence(m, 0, 0, 35, 35, gate=(15, 19))
    for u, v in ((5, 6), (18, 10), (8, 20), (22, 24)):
        cow(m, u, v)
    m.box(26, 4, 0, 33, 5, 1, TIMBER_DARK)
    m.box(27, 4, 1, 32, 5, 1, WELL_WATER)
    tree(m, 30, 29, 0, height=8, crown=4)
    m.box(4, 30, 0, 5, 31, 1, STONE_LIGHT)
    for u, v in ((12, 4), (3, 14), (16, 31), (27, 16)):
        m.set(u, v, 0, FLOWER_YELLOW)
    return m


def iron_mine():
    """36 x 36 x 40: an iron mine: a rocky hill with rust-red ore veins, a timbered tunnel mouth,
    rails out to the front with ore carts, a headframe with its wheel on top, ore heaps."""
    m = Model(36, 36, 40)
    # The hill, highest at the back
    for v in range(36):
        for u in range(36):
            d = ((u - 18) ** 2 * 0.6 + (v - 30) ** 2) ** 0.5
            h = int(22 - d * 1.1 + ((u * 7 + v * 13) % 5) * 0.4)
            if h < 1:
                continue
            for y in range(h):
                vein = (u * 3 + y * 5 + v * 2) % 11 == 0 or (u + y * 2 - v) % 13 == 0
                m.set(u, v, y, IRON_ORE if vein else (STONE_DARK if (u + v + y) % 4 == 0 else 3))
            m.set(u, v, h, 1 if h > 3 else 3)  # Grass on the top, stone low down
    # The tunnel mouth: a timber frame in the hill's front face
    for v in range(10, 26):
        m.box(14, v, 0, 21, v, 7, AIR)
    for v in (12, 16, 20):
        m.box(13, v, 0, 13, v, 8, TIMBER_DARK)
        m.box(22, v, 0, 22, v, 8, TIMBER_DARK)
        m.box(13, v, 8, 22, v, 8, TIMBER_DARK)
    m.box(14, 24, 0, 21, 25, 7, COAL)  # Darkness inside
    # Rails to the front, with two ore carts
    for v in range(0, 24):
        m.set(15, v, 0, IRON)
        m.set(20, v, 0, IRON)
        if v % 2 == 0:
            m.box(14, v, 0, 21, v, 0, TIMBER_DARK)
            m.set(15, v, 0, IRON)
            m.set(20, v, 0, IRON)
    for cart_v in (3, 14):
        m.box(15, cart_v, 1, 20, cart_v + 4, 3, IRON)
        m.box(16, cart_v + 1, 3, 19, cart_v + 3, 4, IRON_ORE)
    # Headframe on top of the hill: two legs, a brace and a wheel
    for u in (14, 22):
        for y in range(22, 36):
            m.set(u + (y - 22) // 5 * (1 if u == 14 else -1), 28, y, TIMBER_DARK)
    m.box(14, 28, 30, 22, 28, 30, TIMBER_DARK)
    for du in range(-4, 5):
        for dy in range(-4, 5):
            if 3.0 <= (du * du + dy * dy) ** 0.5 <= 4.4 or du == 0 or dy == 0:
                if (du * du + dy * dy) ** 0.5 <= 4.4:
                    m.set(18 + du, 27, 35 + dy, IRON)
    # Ore heaps and a cart of coal
    for cu, cv, r, block in ((5, 5, 4.0, IRON_ORE), (30, 5, 3.5, IRON_ORE), (5, 13, 3.0, 3)):
        for u, v in disc_cells(cu, cv, r):
            h = int(r - ((u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2) ** 0.5)
            m.box(u, v, 0, u, v, max(0, h), block)
    return m


def charcoal_kiln():
    """36 x 36 x 28: a charcoal burner's place: two smoking earth-covered mounds, stacks of logs
    waiting, baskets of charcoal and the collier's hut."""
    m = Model(36, 36, 28)
    for cu, cv, r in ((11.0, 22.0, 8.5), (26.0, 11.0, 6.5)):
        for u, v in disc_cells(cu, cv, r):
            d = ((u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2) ** 0.5
            h = int((r * r - d * d) ** 0.5 * 0.8)
            for y in range(h + 1):
                m.set(u, v, y, 2 if (u + v + y) % 5 else COAL)  # Earth (dirt) with soot
        top = int(r * 0.8)
        m.set(int(cu), int(cv), top + 1, SMOKE_EMITTER)
        m.box(int(cu) - 1, int(cv - r), 0, int(cu), int(cv - r), 1, FIRE)  # A vent glowing at the foot
    # Log stacks
    log_pile(m, 2, 12, 2, 3)
    log_pile(m, 25, 34, 22, 3)
    # Baskets of charcoal
    for u, v in ((16, 3), (20, 3), (18, 6)):
        cylinder(m, u + 1.0, v + 1.0, 1.4, 0, 1, TIMBER_LIGHT)
        m.box(u, v, 2, u + 1, v + 1, 2, COAL)
    # The collier's hut: a lean-to of poles and bark
    for y in range(0, 10):
        w = 4 - y * 4 // 10
        m.box(28 - w, 30, y, 28 + w, 34, y, THATCH_DARK if y % 3 else TIMBER_DARK)
    m.box(27, 30, 0, 29, 30, 3, AIR)
    return m


def furnace():
    """36 x 36 x 52: a blast furnace: a tapering stone tower glowing at the top, a timber charging
    ramp up to it, a casting shed with molten iron running out, piles of ore and coal."""
    m = Model(36, 36, 52)
    # Tower
    for y in range(0, 38):
        inset = y // 6
        m.ring(10 + inset, 14 + inset, 25 - inset, 29 - inset, y, y, STONE_DARK if y % 6 == 0 else STONE_LIGHT)
    m.box(10, 14, 0, 25, 29, 0, STONE_DARK)
    for y in range(4, 38, 8):
        inset = y // 6
        m.ring(10 + inset, 14 + inset, 25 - inset, 29 - inset, y, y, IRON)  # Iron bands
    m.box(17, 20, 34, 18, 23, 37, FIRE)
    m.box(16, 19, 38, 19, 24, 40, CHIMNEY_BRICK)
    m.box(17, 20, 40, 18, 23, 40, AIR)
    m.set(17, 21, 40, SMOKE_EMITTER)
    # The tapping hole and a casting shed in front
    m.box(16, 14, 1, 19, 14, 4, FIRE)
    for v in range(5, 14):
        m.set(17, v, 0, FIRE)
        m.set(18, v, 0, FIRE)
    for u in (11, 24):
        for v in (3, 12):
            m.box(u, v, 0, u, v, 9, TIMBER_DARK)
    for v in range(2, 14):
        m.box(10, v, 10 + (v - 2) // 4, 25, v, 10 + (v - 2) // 4, ROOF_SLATE)
    # Charging ramp from the back up to the tower's top
    for v in range(29, 36):
        y = 34 - (v - 29) * 5
        for yy in range(max(0, y - 1), y + 1):
            m.box(16, v, yy, 19, v, yy, TIMBER_LIGHT)
        m.box(16, v, 0, 16, v, max(0, y - 2), TIMBER_DARK)
        m.box(19, v, 0, 19, v, max(0, y - 2), TIMBER_DARK)
    # Piles of ore and coal
    for cu, cv, r, block in ((4.5, 24.0, 4.0, IRON_ORE), (31.0, 22.0, 4.0, COAL), (4.0, 6.0, 3.0, COAL)):
        for u, v in disc_cells(cu, cv, r):
            h = int(r - ((u + 0.5 - cu) ** 2 + (v + 0.5 - cv) ** 2) ** 0.5)
            m.box(u, v, 0, u, v, max(0, h), block)
    return m


def steelworks():
    """36 x 48 x 52: a steelworks: a long brick hall with a sawtooth roof of skylights, two tall
    chimneys, big doors glowing from the rolling mill inside and steel beams stacked outside."""
    m = Model(36, 48, 52)
    u0, u1, v0, v1 = 3, 32, 14, 45
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    brick_walls(m, u0, v0, u1, v1, 1, 18)
    quoins(m, u0, v0, u1, v1, 1, 18, STONE_LIGHT)
    # Side windows in rows (the sides run along v)
    for v in range(v0 + 3, v1 - 2, 5):
        for u in (u0, u1):
            m.box(u, v, 6, u, v + 2, 14, WINDOW_GLASS)
            m.box(u, v, 15, u, v + 2, 15, STONE_LIGHT)
    # Front: two big doors with the glow of the mill
    for left in (7, 20):
        m.box(left, v0, 1, left + 7, v0, 13, STONE_LIGHT)
        m.box(left + 1, v0, 1, left + 6, v0, 12, AIR)
        m.box(left + 1, v0 + 3, 0, left + 6, v0 + 3, 3, FIRE)
        m.box(left + 1, v0 + 3, 4, left + 6, v0 + 3, 8, STEEL)
    m.box(u0, v0, 15, u1, v0, 17, BRICK_RED)
    m.box(14, v0, 15, 19, v0, 18, BRASS)  # A clock over the doors
    m.set(16, v0 - 1, 16, IRON)
    m.set(17, v0 - 1, 17, IRON)
    # Sawtooth roof: ridges along u, steep glass faces toward the front
    m.box(u0, v0, 19, u1, v1, 19, ROOF_SLATE)
    for tooth in range(v0, v1, 8):
        for i in range(8):
            v = tooth + i
            if v > v1:
                break
            m.box(u0, v, 19 + i, u1, v, 19 + i, ROOF_SLATE)
        m.box(u0, min(tooth + 7, v1), 20, u1, min(tooth + 7, v1), 26, WINDOW_GLASS)
    factory_chimney(m, 4, 40, 19, 51, size=4)
    factory_chimney(m, 27, 40, 19, 51, size=4)
    # Steel beams stacked outside: I-beams along u
    for layer in range(3):
        for i, v in enumerate(range(2, 11, 4)):
            m.box(3, v, layer * 3, 30, v + 2, layer * 3, STEEL)
            m.box(3, v + 1, layer * 3 + 1, 30, v + 1, layer * 3 + 1, STEEL)
            m.box(3, v, layer * 3 + 2, 30, v + 2, layer * 3 + 2, STEEL)
    return m


def cannery():
    """36 x 36 x 40: a cannery: a two-storey brick factory with a loading dock, a smoking chimney,
    a water tank on stilts, crates of canned food and stacks of tins."""
    m = Model(36, 36, 40)
    u0, u1, v0, v1 = 3, 25, 12, 33
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    brick_walls(m, u0, v0, u1, v1, 1, 22)
    quoins(m, u0, v0, u1, v1, 1, 22, STONE_LIGHT)
    m.ring(u0, v0, u1, v1, 11, 11, STONE_LIGHT)
    for u in (5, 10, 19):
        arched_window(m, u, v0, 3, 3, 5, facing=-1)
        arched_window(m, u, v0, 14, 3, 5, facing=-1)
    m.box(14, v0, 1, 17, v0, 8, DOOR_WOOD)
    # Loading dock with a canopy
    m.box(3, v0 - 4, 0, 25, v0 - 1, 1, STONE_LIGHT)
    for u in (3, 14, 25):
        m.box(u, v0 - 4, 2, u, v0 - 4, 9, IRON)
    m.box(2, v0 - 5, 10, 26, v0 - 1, 10, ROOF_SLATE)
    gable_roof(m, u0, u1, v0, v1, 22, 32, ROOF_SLATE, STONE_DARK, overhang=1, gable_wall=BRICK_RED)
    m.box(u0 - 1, v0 + 2, 25, u0 - 1, v1 - 2, 25, IRON)  # A sign rail
    factory_chimney(m, 28, 27, 0, 39, size=4)
    # Water tank on stilts
    for u, v in ((27, 13), (33, 13), (27, 19), (33, 19)):
        m.box(u, v, 0, u, v, 13, TIMBER_DARK)
    cylinder(m, 30.5, 16.5, 3.8, 14, 20, TIMBER_LIGHT)
    for y in (15, 18):
        cylinder(m, 30.5, 16.5, 3.8, y, y, IRON, hollow=True)
    dome(m, 30.5, 16.5, 3.5, 21, ROOF_SLATE)
    # Crates on the dock, stacks of tins
    for u in (4, 8, 20):
        crate(m, u, v0 - 4, 2)
    crate(m, 6, v0 - 4, 5)
    for u, v in ((28, 3), (31, 3), (29, 6)):
        for y in range(0, 3):
            cylinder(m, u + 1.0, v + 1.0, 1.0, y, y, STEEL)
    return m


def sewing_machine_factory():
    """36 x 36 x 46: a three-storey factory with rows of windows, a clock on the front, a flat roof
    with skylights and a chimney, crates of sewing machines on carts."""
    m = Model(36, 36, 46)
    u0, u1, v0, v1 = 3, 32, 10, 33
    m.ring(u0, v0, u1, v1, 0, 0, STONE_DARK)
    brick_walls(m, u0, v0, u1, v1, 1, 30)
    quoins(m, u0, v0, u1, v1, 1, 30, STONE_LIGHT)
    for y in (10, 20):
        m.ring(u0, v0, u1, v1, y, y, STONE_LIGHT)
    for floor_y in (3, 13, 23):
        for u in range(5, 31, 4):
            if floor_y == 3 and 14 <= u <= 20:
                continue
            m.box(u, v0, floor_y, u + 1, v0, floor_y + 5, WINDOW_GLASS)
            m.box(u, v1, floor_y, u + 1, v1, floor_y + 5, WINDOW_GLASS)
        for v in range(13, 31, 4):
            m.box(u0, v, floor_y, u0, v + 1, floor_y + 5, WINDOW_GLASS)
            m.box(u1, v, floor_y, u1, v + 1, floor_y + 5, WINDOW_GLASS)
    m.box(15, v0, 1, 20, v0, 8, DOOR_WOOD)
    m.box(14, v0, 9, 21, v0, 9, STONE_LIGHT)
    # The clock tower bit over the door
    m.box(14, v0 - 1, 31, 21, v0 + 2, 37, BRICK_RED)
    m.box(15, v0 - 1, 32, 20, v0 - 1, 36, PLASTER_WHITE)
    m.box(17, v0 - 1, 34, 17, v0 - 1, 36, IRON)
    m.box(17, v0 - 1, 34, 19, v0 - 1, 34, IRON)
    m.box(13, v0 - 2, 38, 22, v0 + 3, 38, STONE_LIGHT)
    m.box(17, v0, 39, 18, v0 + 1, 41, BRASS)
    # Flat roof with a parapet and skylights
    m.box(u0, v0, 31, u1, v1, 31, ROOF_SLATE)
    m.ring(u0, v0, u1, v1, 32, 32, STONE_LIGHT)
    for v in (16, 24):
        m.box(7, v, 32, 12, v + 3, 33, WINDOW_GLASS)
        m.box(23, v, 32, 28, v + 3, 33, WINDOW_GLASS)
    factory_chimney(m, 27, 27, 31, 45, size=3)
    # Crates of sewing machines on a cart in front, one machine on show
    m.box(3, 2, 1, 12, 6, 1, TIMBER_LIGHT)
    for u in (4, 8):
        crate(m, u, 3, 2)
    for u, v in ((3, 1), (12, 1), (3, 7), (12, 7)):
        m.box(u, v, 0, u, v, 1, IRON)
    m.box(26, 3, 0, 31, 6, 2, TIMBER_DARK)  # Table
    m.box(27, 4, 3, 30, 5, 3, COAL)          # The machine: black body...
    m.box(27, 4, 4, 27, 5, 6, COAL)
    m.box(27, 4, 6, 29, 5, 6, COAL)
    m.set(29, 4, 4, BRASS)                   # ...brass trim, a wheel
    m.box(31, 4, 3, 31, 5, 5, IRON)
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
        'sheepfold_1': sheepfold(),
        'pigsty_1': pigsty(),
        'slaughterhouse_1': slaughterhouse(),
        'harbor_1': harbor(),
        'school_1': school(),
        'grain_farm_1': grain_farm(),
        'wheat_field_1': wheat_field(),
        'flour_mill_1': flour_mill(),
        'bakery_1': bakery(),
        'rendering_works_1': rendering_works(),
        'soap_factory_1': soap_factory(),
        'clay_pit_1': clay_pit(),
        'brick_factory_1': brick_factory(),
        'artisan_house_1': artisan_house(0),
        'artisan_house_2': artisan_house(1),
        'artisan_house_3': artisan_house(2),
        'variety_theatre_1': variety_theatre(),
        'cattle_farm_1': cattle_farm(),
        'pasture_1': pasture(),
        'iron_mine_1': iron_mine(),
        'charcoal_kiln_1': charcoal_kiln(),
        'furnace_1': furnace(),
        'steelworks_1': steelworks(),
        'cannery_1': cannery(),
        'sewing_machine_factory_1': sewing_machine_factory(),
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
