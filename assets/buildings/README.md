# Building models

The look of every building is a MagicaVoxel (`.vox`) file here, named
`<model name>_<n>.vox` with `n = 1, 2, ...` (up to 4 variants per building). The model name of each
building type is in `src/Simulation/BuildingTypes.h` (`modelName`). New buildings pick a variant
from their object ID, so neighbouring houses differ. A type without a usable model falls back to the
procedural look (`src/Simulation/BuildingLook.cpp`), and the game prints a warning.

| Building | File | Size (x by y by z) |
|---|---|---|
| Warehouse | `warehouse_n.vox` | 48 x 48 x up to 40 |
| Farmer House | `farmer_house_n.vox` | 36 x 36 x up to 30 |
| Worker House | `worker_house_n.vox` | 36 x 36 x up to 42 |
| Marketplace | `marketplace_n.vox` | 48 x 36 x up to 24 |
| Fishery | `fishery_n.vox` | 36 x 48 x up to 32 (a coastal building: see below) |
| Lumberjack | `lumberjack_n.vox` | 36 x 36 x up to 24 |
| Sawmill | `sawmill_n.vox` | 36 x 36 x up to 26 |
| Sheep Farm | `sheep_farm_n.vox` | 36 x 36 x up to 28 |
| Framework Knitter | `framework_knitter_n.vox` | 36 x 36 x up to 34 |
| Pig Farm | `pig_farm_n.vox` | 36 x 36 x up to 22 |
| Slaughterhouse | `slaughterhouse_n.vox` | 36 x 36 x up to 32 |
| School | `school_n.vox` | 36 x 36 x up to 38 |
| Grain Farm, Wheat Field | `grain_farm_n.vox`, `wheat_field_n.vox` | 36 x 36 x up to 28, 10 |
| Flour Mill | `flour_mill_n.vox` | 36 x 36 x up to 64 |
| Bakery, Rendering Works | `bakery_n.vox`, `rendering_works_n.vox` | 36 x 36 x up to 34 |
| Soap Factory | `soap_factory_n.vox` | 36 x 36 x up to 40 |
| Clay Pit, Brick Factory | `clay_pit_n.vox`, `brick_factory_n.vox` | 36 x 36 x up to 24, 44 |
| Artisan House | `artisan_house_n.vox` | 36 x 36 x up to 54 |
| Variety Theatre | `variety_theatre_n.vox` | 48 x 48 x up to 50 |
| Cattle Farm, Pasture | `cattle_farm_n.vox`, `pasture_n.vox` | 36 x 36 x up to 28, 12 |
| Iron Mine, Charcoal Kiln | `iron_mine_n.vox`, `charcoal_kiln_n.vox` | 36 x 36 x up to 40, 28 |
| Furnace | `furnace_n.vox` | 36 x 36 x up to 52 |
| Steelworks | `steelworks_n.vox` | 36 x 48 x up to 52 |
| Cannery, Sewing Machine Factory | `cannery_n.vox`, `sewing_machine_factory_n.vox` | 36 x 36 x up to 40, 46 |

## Rules

- **Size:**
  - x and y must be exactly the footprint in tiles × 12 (a build tile is 12×12 voxel columns).
  - z (up) can be at most the building type's `height`.
  - A model of the wrong size is skipped and the game prints a warning.
- **Orientation:**
  - z is up, and z = 0 is the first layer above the ground.
  - The door side (the front) is the y = 0 side.
  - The game rotates the model in quarter turns when the player presses R.
- **Palette:** palette index = block ID.
  - The colors saved in these files are the in-game colors, so what you see in MagicaVoxel is what you get.
  - Use the building materials, indices **60–99** and **160–178**:

    | Range | Materials |
    |---|---|
    | 60–62 | plaster (white, cream, ochre) |
    | 63–64 | timber (dark, light) |
    | 65–66 | thatch |
    | 67–69 | roof tiles (red, dark) and slate |
    | 70–72 | stone (light, dark) and cobble |
    | 73 | window glass (lit at night) |
    | 74 | window frame |
    | 75 | door |
    | 76–78 | shutters (green, blue, red) |
    | 79 | chimney brick |
    | 80–82 | flowers (red, yellow) and leaves |
    | 83 | fence |
    | 84 | hay |
    | 85 | crate |
    | 86 | barrel |
    | 87 | iron |
    | 88–91 | awnings (red, white, blue, yellow) |
    | 92 | well water |
    | 93 | garden soil |
    | 94 | vegetables |
    | 95 | fish |
    | 96 | wool |
    | 97 | pig |
    | 98 | sausage |
    | 99 | mud |
    | 160–161 | wheat (ears, stalks) |
    | 162–165 | clay, coal, iron ore, steel |
    | 166 | fire (glows) |
    | 167–168 | cattle (brown, white) |
    | 169–172 | bread, soap, brass, velvet |
    | 173 | red brick |
    | 174–175 | plaster (blue, pink) |
    | 176 | green copper roof |
    | 177–178 | sack, sail canvas |

  - The older blocks (1–50) also work. Changing a color in the palette changes nothing in the game: colors come from `shaders/render/shade.comp`.
- **Markers:** two palette indices are not drawn. The game reads their positions instead:
  - **100, smoke emitter:** put it in a chimney opening; smoke rises from there while the building is in use.
  - **101, boat berth:** where a boat moors. It sails out from there toward the model's back (+y).
- **Coastal buildings** (the fishery, `dockRows` in the building table): the back rows of the footprint stand over the water as a dock.
  - The model reaches `belowGround` voxels under the ground (6 for the fishery), so z = 6 is ground level and z = 2 is the sea's surface.
  - Below ground only solid voxels are written, so the sea stays around pilings and quay walls.
  - Put a foundation under the land rows: on a coast they may stand over the shore's slope.
- **Editing:** edit a file in MagicaVoxel and restart the game to see it.

## Moving parts

Parts that move are not in the building models: windmill sails, a mine's wheel, signs, flags,
bells, hoists, and the animals in the pens. Each is a small model in `parts/`, drawn by the game as
a voxel object that turns, swings or slides about a pivot (`src/Gameplay/BuildingAnimations.h`).
`parts/parts.txt` places them, one line each:

```
<building model> <part file> <u> <v> <y> <motion> <axis> <a> <b>
flour_mill flour_mill_sails 18 6 44 spin forward 0.8 0
```

- **u v y:** the pivot, a voxel in the building model's frame (u across the front, v from the front
  to the back, y up). A part model is centered on its pivot, so its sizes are odd.
- **motion:**
  - `spin`: a radians per second about the axis;
  - `swing`: back and forth a radians per second, b radians each way;
  - `slide`: back and forth along the axis, b voxels, a radians per second;
  - `wander`: an animal walking about a pen; u v is the pen's middle, y the ground, a and b how far it strays along u and v. Its model stands on its bottom and faces +v.
- **axis:** `across` (u), `up` (y) or `forward` (v).

A producer's parts only move while it works; the others move all the time.

## Regenerating

The first versions of these files are generated by `tools/building_models/generate.py`:

```
python tools/building_models/generate.py
```

Running it **overwrites** the files listed in its `main()`, and always rewrites `parts/`. Once a model has been edited by hand,
remove it from that list, or save the edited version under another variant number.
