"""Generates the foliage models in assets/foliage: trees, bushes, rocks, logs and flowers.

Run from the repository root:  python tools/foliage/generate.py

Each model is odd-sized across with its root (where it stands) in the middle column; y = 0 is the
ground layer (a trunk or rock there replaces the grass voxel), so plants start at y = 1 and trunks
and rocks reach down to 0. The game stamps each model in four quarter turns, picked per spot, and
places them by the table in src/World/Foliage.cpp (keep the names in step). Palette index = block
ID: the foliage colors are 193-222 (src/World/BlockTypes.h, FOLIAGE_PALETTE in shade.comp).
"""

import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'building_models'))
from generate import Model  # noqa: E402

# --- Block IDs (src/World/BlockTypes.h, FOLIAGE_*) -------------------------------------------
BARK, BARK_DARK, BIRCH_BARK, BIRCH_MARK = 193, 194, 195, 196
LEAF_DARK, LEAF, LEAF_LIGHT, LEAF_YELLOW = 197, 198, 199, 200
NEEDLE_DARK, NEEDLE, MOSS = 201, 202, 203
ROCK_LIGHT, ROCK, ROCK_DARK = 204, 205, 206
BLOSSOM_WHITE, BLOSSOM_PINK, BERRY, FLOWER_BLUE, FLOWER_PURPLE, FERN = 207, 208, 209, 210, 211, 212
FROND, FROND_DARK, PALM_TRUNK, BANANA_LEAF, AGAVE, HIBISCUS, COCONUT = 213, 214, 215, 216, 217, 218, 219
MUSHROOM, DEADWOOD, FLOWER_ORANGE = 220, 221, 222
BEECH_BARK = ROCK  # Smooth grey


# --- Shapes -------------------------------------------------------------------------------------

def blob(m, cu, cv, cy, ru, rv, ry, color, rng, rough=0.25, density=1.0):
    """A rough ellipsoid; color(u, v, y, t) picks each voxel's block, t = height in it (0 bottom, 1 top)."""
    for y in range(int(cy - ry - 1), int(cy + ry + 2)):
        for v in range(int(cv - rv - 1), int(cv + rv + 2)):
            for u in range(int(cu - ru - 1), int(cu + ru + 2)):
                d = ((u - cu) / ru) ** 2 + ((v - cv) / rv) ** 2 + ((y - cy) / ry) ** 2
                if d > 1.0 + rough * (rng.random() - 0.5) * 2:
                    continue
                if density < 1.0 and rng.random() > density:
                    continue
                m.set(u, v, y, color(u, v, y, (y - (cy - ry)) / (2.0 * ry)))


def leaves(dark, mid, light, rng, speck=None, speck_share=0.0):
    """Shaded foliage: dark underneath, light on top, with a few specks (blossom, berries)."""
    def color(u, v, y, t):
        if speck is not None and rng.random() < speck_share:
            return speck
        r = t + (rng.random() - 0.5) * 0.45
        return dark if r < 0.35 else (mid if r < 0.72 else light)
    return color


def line(m, a, b, radius, block, rng=None, alt=None, alt_share=0.0):
    """A limb from point a to b (u, v, y), radius in voxels (0 = one voxel)."""
    steps = int(max(abs(b[i] - a[i]) for i in range(3)) * 2) + 1
    for s in range(steps + 1):
        t = s / steps
        p = [a[i] + (b[i] - a[i]) * t for i in range(3)]
        r = int(math.ceil(radius))
        for dv in range(-r, r + 1):
            for du in range(-r, r + 1):
                if du * du + dv * dv > radius * radius + 0.3:
                    continue
                pick = alt if alt is not None and rng is not None and rng.random() < alt_share else block
                m.set(int(round(p[0] + du)), int(round(p[1] + dv)), int(round(p[2])), pick)


def roots(m, cu, cv, block, reach=2):
    for du, dv in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        for k in range(1, reach + 1):
            m.set(cu + du * k, cv + dv * k, 0, block)
        m.set(cu + du, cv + dv, 1, block)


# --- Old World trees ----------------------------------------------------------------------------

def oak(seed):
    rng = random.Random(seed)
    S, H = 25, 30
    m, c = Model(S, S, H), S // 2
    top = (c + rng.choice((-1, 1)), c + rng.choice((-1, 0, 1)), 11)
    line(m, (c, c, 0), top, 1.3, BARK, rng, BARK_DARK, 0.3)
    roots(m, c, c, BARK_DARK, 3)
    color = leaves(LEAF_DARK, LEAF, LEAF_LIGHT, rng)
    clusters = [(top[0], top[1], 21, 6.5, 5.0)]
    for i in range(5 + seed % 2):
        a = i * 2 * math.pi / (5 + seed % 2) + rng.random() * 0.6
        d = 5.5 + rng.random() * 2.5
        end = (c + math.cos(a) * d, c + math.sin(a) * d, 15 + rng.random() * 4)
        line(m, top, end, 0.6, BARK_DARK)
        clusters.append((end[0], end[1], end[2] + 2, 4.0 + rng.random() * 1.5, 3.5 + rng.random()))
    for cu, cv, cy, r, ry in clusters:
        blob(m, cu, cv, cy, r, r, ry, color, rng)
    return m


def beech(seed):
    rng = random.Random(seed)
    S, H = 21, 34
    m, c = Model(S, S, H), S // 2
    line(m, (c, c, 0), (c, c, 16), 1.0, BEECH_BARK, rng, ROCK_LIGHT, 0.2)
    roots(m, c, c, BEECH_BARK, 2)
    color = leaves(LEAF_DARK, LEAF, LEAF_LIGHT, rng)
    blob(m, c, c, 22, 7.5, 7.5, 9.5, color, rng)
    for i in range(4):
        a = i * math.pi / 2 + seed + rng.random() * 0.5
        blob(m, c + math.cos(a) * 4, c + math.sin(a) * 4, 18 + rng.random() * 6, 4, 4, 4, color, rng)
    return m


def birch(seed):
    rng = random.Random(seed)
    S, H = 15, 31
    m, c = Model(S, S, H), S // 2
    color = leaves(LEAF, LEAF_LIGHT, LEAF_YELLOW, rng)
    stems = [(1.5, 0.0)] if seed % 2 else [(2.5, 0.0), (2.5, math.pi)]
    for lean, a in stems:
        a += rng.random()
        top = (c + math.cos(a) * lean, c + math.sin(a) * lean, 24)
        for y in range(0, 25):
            t = y / 24.0
            u, v = int(round(c + (top[0] - c) * t)), int(round(c + (top[1] - c) * t))
            m.set(u, v, y, BIRCH_MARK if rng.random() < 0.18 else BIRCH_BARK)
            if len(stems) == 1 and y < 10:
                m.set(u + 1, v, y, BIRCH_MARK if rng.random() < 0.18 else BIRCH_BARK)
        for k in range(9):  # Airy crown: small loose clumps along the top of the stem
            y = 12 + k * 2 + rng.random() * 2
            t = y / 24.0
            u, v = c + (top[0] - c) * t, c + (top[1] - c) * t
            off = (4.5 - k * 0.35) * rng.random()
            b = rng.random() * 2 * math.pi
            blob(m, u + math.cos(b) * off, v + math.sin(b) * off, y, 2.6, 2.6, 2.2, color, rng, density=0.75)
    return m


def spruce(seed):
    rng = random.Random(seed)
    S, H = 15, 38
    m, c = Model(S, S, H), S // 2
    top = H - 2 - seed % 3
    m.box(c, c, 0, c, c, top, BARK_DARK)
    m.box(c, c, 0, c + 1, c + 1, 3, BARK_DARK)
    for y in range(4, top + 2):
        t = (y - 4) / float(top - 2)
        tier = ((top - y) % 5) / 4.0  # Each tier wide at its foot, narrow up to the next
        r = (7.0 * (1.0 - t)) * (0.55 + 0.45 * tier) + 0.6
        for v in range(c - 8, c + 9):
            for u in range(c - 8, c + 9):
                d = math.hypot(u - c, v - c)
                if d <= r + (rng.random() - 0.5) * 0.9:
                    edge = d > r - 1.2
                    m.set(u, v, y, NEEDLE if edge and rng.random() < 0.6 else NEEDLE_DARK)
    m.set(c, c, top + 2, NEEDLE)
    return m


# --- New World trees ----------------------------------------------------------------------------

def palm(seed):
    rng = random.Random(seed)
    S, H = 27, 36
    m, c = Model(S, S, H), S // 2
    height = (24, 30, 20)[seed % 3]
    bend = (4.0, 1.5, 8.0)[seed % 3]
    a = rng.random() * 2 * math.pi
    top = None
    for y in range(0, height):
        lean = (y / float(height)) ** 2 * bend
        u, v = int(round(c + math.cos(a) * lean)), int(round(c + math.sin(a) * lean))
        block = BARK_DARK if y % 3 == 0 else PALM_TRUNK
        m.box(u, v, y, u + 1, v + 1, y, block)
        top = (u, v, y)
    cu, cv, cy = top
    for du, dv in ((-1, 0), (1, 1), (0, -1), (2, 0)):
        m.set(cu + du, cv + dv, cy - 1, COCONUT)
    fronds = 8 + seed % 2
    for i in range(fronds):
        fa = i * 2 * math.pi / fronds + rng.random() * 0.3
        du, dv = math.cos(fa), math.sin(fa)
        length = 12 if i % 2 else 10
        for step in range(1, length + 1):
            t = step / length
            u, v = cu + du * step, cv + dv * step
            y = cy + 1 + int(round(t * 5.0 - t * t * 13.0))  # Up, then hanging down
            m.set(int(round(u)), int(round(v)), y, FROND_DARK)
            if 2 <= step < length:
                width = 1 if 3 <= step < length - 3 else 0
                for side in (-1, 1):
                    for w in range(1, width + 2):
                        m.set(int(round(u - dv * side * w)), int(round(v + du * side * w)), y - (1 if w > 1 else 0),
                              FROND if (step + w) % 3 else FROND_DARK)
    m.box(cu, cv, cy + 1, cu + 1, cv + 1, cy + 2, FROND)
    return m


def jungle_tree(seed):
    rng = random.Random(seed)
    S, H = 27, 34
    m, c = Model(S, S, H), S // 2
    line(m, (c, c, 0), (c, c, 22), 1.2, BARK, rng, MOSS, 0.15)
    for i in range(4):  # Buttress roots
        a = i * math.pi / 2 + 0.4
        for k in range(1, 5):
            for y in range(0, 5 - k):
                m.set(int(round(c + math.cos(a) * k)), int(round(c + math.sin(a) * k)), y, BARK)
    color = leaves(LEAF_DARK, LEAF, LEAF_LIGHT, rng)
    for i in range(6):
        a = i * math.pi / 3 + rng.random() * 0.5
        end = (c + math.cos(a) * 7, c + math.sin(a) * 7, 24 + rng.random() * 3)
        line(m, (c, c, 20), end, 0.5, BARK)
        blob(m, end[0], end[1], end[2] + 2, 5.5, 5.5, 2.5, color, rng)
    blob(m, c, c, 28, 6, 6, 3, color, rng)
    for i in range(7):  # Hanging vines
        a = rng.random() * 2 * math.pi
        u, v = int(round(c + math.cos(a) * 9)), int(round(c + math.sin(a) * 9))
        for y in range(24 - rng.randint(4, 9), 25):
            m.set(u, v, y, LEAF_DARK)
    return m


# --- Bushes -------------------------------------------------------------------------------------

def bush(seed):
    rng = random.Random(seed)
    m, c = Model(11, 11, 9), 5
    speck, share = ((None, 0.0), (BERRY, 0.08), (BLOSSOM_PINK if seed % 2 else BLOSSOM_WHITE, 0.3))[seed % 3]
    color = leaves(LEAF_DARK, LEAF, LEAF_LIGHT, rng, speck, share)
    blob(m, c, c, 3.5, 4.2, 3.6, 3.2, color, rng)
    blob(m, c + 2, c - 1, 2.5, 2.5, 2.5, 2.2, color, rng)
    return m


def banana(seed):
    rng = random.Random(seed)
    m, c = Model(19, 19, 16), 9
    m.box(c, c, 0, c, c, 8, PALM_TRUNK)
    for i in range(6):
        a = i * math.pi / 3 + rng.random() * 0.4
        du, dv = math.cos(a), math.sin(a)
        for step in range(1, 9):
            t = step / 8.0
            y = 8 + int(round(t * 5 - t * t * 8))
            u, v = c + du * step, c + dv * step
            m.set(int(round(u)), int(round(v)), y, BANANA_LEAF)
            if 1 < step < 8:
                for side in (-1, 1):
                    m.set(int(round(u - dv * side)), int(round(v + du * side)), y - 1 if step > 5 else y, BANANA_LEAF if rng.random() < 0.8 else LEAF)
    m.box(c, c, 9, c, c, 10, LEAF)
    return m


def monstera(seed):
    rng = random.Random(seed)
    m, c = Model(13, 13, 8), 6
    color = leaves(LEAF_DARK, LEAF_DARK, LEAF, rng)
    for i in range(5 + seed % 2):
        a = i * 2 * math.pi / (5 + seed % 2) + rng.random() * 0.5
        blob(m, c + math.cos(a) * 3.2, c + math.sin(a) * 3.2, 2.5 + rng.random() * 2, 2.6, 2.6, 1.1, color, rng, density=0.9)
    return m


def agave(seed):
    rng = random.Random(seed)
    m, c = Model(11, 11, 8), 5
    m.box(c, c, 1, c, c, 2, AGAVE)
    for i in range(10):
        a = i * 2 * math.pi / 10 + rng.random() * 0.3
        rise = 1.4 if i % 2 else 0.7
        for step in range(1, 6):
            m.set(int(round(c + math.cos(a) * step)), int(round(c + math.sin(a) * step)), 1 + int(step * rise * (1.1 - step / 8.0)), AGAVE)
    return m


# --- Rocks and logs -----------------------------------------------------------------------------

def rock_color(rng, mossy):
    def color(u, v, y, t):
        if mossy and t > 0.6 and rng.random() < 0.7:
            return MOSS
        r = t + (rng.random() - 0.5) * 0.4
        return ROCK_DARK if r < 0.35 else (ROCK if r < 0.75 else ROCK_LIGHT)
    return color


def boulder(seed):
    rng = random.Random(seed)
    m, c = Model(11, 11, 8), 5
    if seed % 3 == 2:  # A pile of three
        for du, dv, r in ((-2, -1, 2.4), (2, 0, 2.0), (0, 2, 1.6)):
            blob(m, c + du, c + dv, r * 0.6, r, r, r * 0.9, rock_color(rng, False), rng, rough=0.35)
    else:
        blob(m, c, c, 1.5, 4.2, 3.4, 3.6, rock_color(rng, seed % 3 == 1), rng, rough=0.35)
    return m


def log(seed):
    rng = random.Random(seed)
    m, c = Model(17, 9, 6), 4
    length = 14 - seed % 2 * 2
    u0 = (17 - length) // 2
    for u in range(u0, u0 + length):
        for v in range(c - 2, c + 3):
            for y in range(0, 5):
                d = math.hypot(v - c, y - 1.6)
                if d <= 1.9:
                    end = u == u0 or u == u0 + length - 1
                    top = y >= 3 and rng.random() < 0.45
                    m.set(u, v, y, DEADWOOD if end and d < 1.2 else (MOSS if top else BARK))
    line(m, (u0 + 5, c, 2), (u0 + 7, c + 3, 4), 0, BARK_DARK)  # Branch stub
    if seed % 2 == 0:
        m.set(u0 + 9, c - 3, 1, MUSHROOM)
        m.set(u0 + 10, c - 3, 1, MUSHROOM)
    return m


def stump(seed):
    rng = random.Random(seed)
    m, c = Model(9, 9, 5), 4
    for v in range(c - 2, c + 3):
        for u in range(c - 2, c + 3):
            d = math.hypot(u - c, v - c)
            if d <= 2.2:
                h = 3 if d < 1.6 else 2
                for y in range(0, h):
                    m.set(u, v, y, BARK if d > 1.4 or y < h - 1 else DEADWOOD)
    roots(m, c, c, BARK, 3)
    for du, dv in ((3, 1), (-2, 3), (3, -1)):  # Mushrooms: white stalk, red cap
        m.set(c + du, c + dv, 1, BLOSSOM_WHITE)
        m.set(c + du, c + dv, 2, MUSHROOM)
    m.set(c, c, 3, MOSS if rng.random() < 0.5 else DEADWOOD)
    return m


def palm_log(seed):
    m, c = Model(19, 7, 4), 3
    for u in range(2, 17):
        m.box(u, c, 0, u, c + 1, 1, BARK_DARK if u % 3 == 0 else PALM_TRUNK)
    for du in range(0, 4):  # The crown's dead fronds at one end
        m.set(15 + du % 2, c - 1 - du // 2, 1, DEADWOOD)
        m.set(15 + du % 2, c + 2 + du // 2, 1, DEADWOOD)
    return m


def driftwood(seed):
    m = Model(15, 9, 4)
    line(m, (1, 3, 1), (13, 5, 1), 0.7, DEADWOOD)
    line(m, (8, 4, 1), (11, 1, 2), 0, DEADWOOD)
    line(m, (4, 3, 1), (2, 7, 1), 0, DEADWOOD)
    return m


# --- Flowers and ferns --------------------------------------------------------------------------

def flowers(seed):
    rng = random.Random(seed)
    m, c = Model(9, 9, 4), 4
    colors = ((FLOWER_RED_, FLOWER_YELLOW_), (FLOWER_BLUE, BLOSSOM_WHITE), (FLOWER_PURPLE, BLOSSOM_PINK))[seed % 3]
    for v in range(9):
        for u in range(9):
            if math.hypot(u - c, v - c) > 4.2 or rng.random() > 0.45:
                continue
            h = rng.randint(1, 2)
            m.box(u, v, 1, u, v, h, FERN)
            m.set(u, v, h + 1, rng.choice(colors))
    return m


def fern(seed, color=FERN, size=9, fronds=7):
    rng = random.Random(seed)
    m, c = Model(size, size, 5), size // 2
    for i in range(fronds):
        a = i * 2 * math.pi / fronds + rng.random() * 0.4
        reach = c
        for step in range(1, reach + 1):
            t = step / float(reach)
            y = 1 + int(round(t * 3.2 - t * t * 3.0))
            m.set(int(round(c + math.cos(a) * step)), int(round(c + math.sin(a) * step)), y, color)
    m.set(c, c, 1, LEAF_DARK)
    return m


def hibiscus(seed):
    rng = random.Random(seed)
    m, c = Model(9, 9, 7), 4
    color = leaves(LEAF_DARK, LEAF, LEAF, rng, HIBISCUS, 0.0)
    blob(m, c, c, 2.5, 3.6, 3.6, 2.6, color, rng)
    for (u, v, y) in list(m.voxels):
        if m.get(u, v, y + 1) == 0 and rng.random() < 0.35:
            m.set(u, v, y + 1, HIBISCUS if rng.random() < 0.7 else FLOWER_ORANGE)
    return m


# Building model palette's red and yellow flowers suit the meadows too
FLOWER_RED_, FLOWER_YELLOW_ = 80, 81

MODELS = {
    'oak_1': lambda: oak(1), 'oak_2': lambda: oak(2),
    'beech_1': lambda: beech(1), 'beech_2': lambda: beech(2),
    'birch_1': lambda: birch(1), 'birch_2': lambda: birch(2),
    'spruce_1': lambda: spruce(1), 'spruce_2': lambda: spruce(2),
    'palm_1': lambda: palm(0), 'palm_2': lambda: palm(1), 'palm_3': lambda: palm(2),
    'jungle_tree_1': lambda: jungle_tree(1),
    'bush_1': lambda: bush(0), 'bush_2': lambda: bush(1), 'bush_3': lambda: bush(2),
    'banana_1': lambda: banana(1), 'monstera_1': lambda: monstera(1), 'monstera_2': lambda: monstera(2),
    'agave_1': lambda: agave(1),
    'boulder_1': lambda: boulder(0), 'boulder_2': lambda: boulder(1), 'boulder_3': lambda: boulder(2),
    'log_1': lambda: log(0), 'log_2': lambda: log(1), 'stump_1': lambda: stump(1),
    'palm_log_1': lambda: palm_log(1), 'driftwood_1': lambda: driftwood(1),
    'flowers_1': lambda: flowers(0), 'flowers_2': lambda: flowers(1), 'flowers_3': lambda: flowers(2),
    'fern_1': lambda: fern(1), 'fern_2': lambda: fern(2, LEAF, 9, 6),
    'hibiscus_1': lambda: hibiscus(1), 'tropical_fern_1': lambda: fern(3, FROND, 11, 8),
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    folder = os.path.join(root, 'assets', 'foliage')
    os.makedirs(folder, exist_ok=True)
    for name, make in MODELS.items():
        model = make()
        model.save(os.path.join(folder, name + '.vox'))
        print('%-16s %3d x %3d x %3d  %5d voxels' % (name, model.width, model.depth, model.height, len(model.voxels)))


if __name__ == '__main__':
    main()
