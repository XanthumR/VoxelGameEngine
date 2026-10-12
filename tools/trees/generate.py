"""Generates the New World's palm tree, assets/palm.vox (the Old World's tree is assets/tree.vox).

The model is 25 x 25 x 30 with the trunk's foot in the middle column (the game stamps a tree with
the middle of the model's x and y at its root). Run from the project root:
    python tools/trees/generate.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'building_models'))
from generate import Model, TIMBER_DARK, TIMBER_LIGHT, LEAVES, VEGETABLES, BARREL  # noqa: E402

SIZE, HEIGHT = 25, 30
MIDDLE = SIZE // 2


def palm():
    m = Model(SIZE, SIZE, HEIGHT)
    # The trunk: 2 x 2, ringed, leaning out a little more the higher it gets
    top = None
    for y in range(0, 24):
        lean = (y / 24.0) ** 2 * 4.0
        u, v = int(round(MIDDLE + lean)), MIDDLE
        block = TIMBER_DARK if y % 3 == 0 else TIMBER_LIGHT
        m.box(u, v, y, u + 1, v + 1, y, block)
        top = (u, v, y)
    cu, cv, cy = top
    # Coconuts under the crown
    for du, dv in ((-1, 0), (1, 1), (0, -1)):
        m.set(cu + du, cv + dv, cy - 1, BARREL)
    # Eight fronds arching out and drooping, each a spine with leaflets either side
    for i in range(8):
        angle = i * math.pi / 4 + 0.3
        du, dv = math.cos(angle), math.sin(angle)
        length = 12 if i % 2 else 10
        for step in range(1, length + 1):
            t = step / length
            u = cu + du * step
            v = cv + dv * step
            y = cy + 1 + int(round(t * 5.0 - t * t * 13.0))  # Up, then hanging down
            m.set(int(round(u)), int(round(v)), y, VEGETABLES)
            if 2 <= step < length:
                width = 1 if 3 <= step < length - 3 else 0
                for side in (-1, 1):
                    for w in range(1, width + 2):
                        m.set(int(round(u - dv * side * w)), int(round(v + du * side * w)), y - (1 if w > 1 else 0),
                              LEAVES if (step + w) % 3 == 0 else VEGETABLES)
    m.box(cu, cv, cy + 1, cu + 1, cv + 1, cy + 2, LEAVES)
    return m


if __name__ == '__main__':
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    path = os.path.join(root, 'assets', 'palm.vox')
    model = palm()
    model.save(path)
    print('palm: %d voxels -> %s' % (len(model.voxels), path))
