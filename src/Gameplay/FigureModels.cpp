#include "Gameplay/FigureModels.h"

#include "World/BlockTypes.h"

#include <algorithm>
#include <cstdlib>

namespace {

// Writes into a model whose s (across, right +), t (forward) and y (up) start at these offsets
struct ModelWriter {
    VoxelObjectModel& model;
    int sOffset, tOffset;

    void Put(int s, int t, int y, uint8_t id) {
        int x = s + sOffset, z = t + tOffset;
        const glm::ivec3& size = model.size;
        if (x < 0 || x >= size.x || z < 0 || z >= size.z || y < 0 || y >= size.y) return;
        model.ids[(size_t)x + (size_t)size.x * ((size_t)z + (size_t)size.z * (size_t)y)] = id;
    }
};

VoxelObjectModel Empty(glm::ivec3 size) {
    VoxelObjectModel model;
    model.size = size;
    model.ids.assign((size_t)size.x * size.y * size.z, 0);
    return model;
}

// Walk and trot cycle: 0 = one side forward, 1 and 3 = passing, 2 = the other side forward
int Swing(int frame) {
    return frame == 0 ? 1 : frame == 2 ? -1 : 0;
}

} // namespace

// About 11 voxels tall: legs and arms swinging, a straw hat (Farmers) or a cap (Workers)
VoxelObjectModel BuildPersonModel(int tier, int variant, int frame) {
    VoxelObjectModel model = Empty(glm::ivec3(5, 11, 4));
    ModelWriter w{ model, 2, 2 }; // s -2..2, t -2..1
    uint8_t skin = (variant & 1) ? Block::PERSON_SKIN_DARK : Block::PERSON_SKIN;
    uint8_t shirt = tier == 0 ? ((variant & 2) ? Block::PERSON_SHIRT_FARMER_GREEN : Block::PERSON_SHIRT_FARMER)
                              : ((variant & 2) ? Block::PERSON_SHIRT_WORKER_GREY : Block::PERSON_SHIRT_WORKER);
    uint8_t trousers = (variant & 4) ? Block::PERSON_TROUSERS_BLUE : Block::PERSON_TROUSERS;
    int swing = Swing(frame);

    // Legs with shoes: the lower leg swings, the knee stays under the hips
    for (int leg = -1; leg <= 1; leg += 2) {
        int legSwing = leg < 0 ? swing : -swing;
        w.Put(leg, legSwing, 0, Block::PERSON_SHOES);
        w.Put(leg, legSwing, 1, trousers);
        w.Put(leg, 0, 2, trousers);
        w.Put(leg, 0, 3, trousers);
    }
    // Hips and torso, two deep
    for (int s = -1; s <= 1; s++) {
        w.Put(s, 0, 4, trousers);
        for (int y = 5; y <= 7; y++) {
            w.Put(s, 0, y, shirt);
            w.Put(s, -1, y, shirt);
        }
    }
    // Arms swinging against the legs
    for (int arm = -2; arm <= 2; arm += 4) {
        int armSwing = arm < 0 ? -swing : swing;
        w.Put(arm, 0, 7, shirt);
        w.Put(arm, 0, 6, shirt);
        w.Put(arm, armSwing, 5, skin);
    }
    // Head
    for (int s = -1; s <= 1; s++) {
        w.Put(s, 0, 8, skin);
        w.Put(s, -1, 8, skin);
    }
    if (tier == 0) {
        for (int s = -2; s <= 2; s++) {
            for (int t = -2; t <= 1; t++) w.Put(s, t, 9, Block::PERSON_STRAW_HAT); // Brim
        }
        for (int s = -1; s <= 1; s++) {
            w.Put(s, 0, 10, Block::PERSON_STRAW_HAT);
            w.Put(s, -1, 10, Block::PERSON_STRAW_HAT);
        }
    } else {
        for (int s = -1; s <= 1; s++) {
            for (int t = -1; t <= 1; t++) w.Put(s, t, 9, Block::PERSON_CAP); // Cap with a visor
        }
    }
    return model;
}

// About 23 voxels long: a trotting horse in harness (t 1..12), shafts and reins, a four-wheeled
// wagon (t -10..-2) with a driver at the front and a row of crates per good carried
VoxelObjectModel BuildCartModel(int frame, int item, int amount) {
    VoxelObjectModel model = Empty(glm::ivec3(7, 12, 23));
    ModelWriter w{ model, 3, 10 }; // s -3..3, t -10..12
    int swing = Swing(frame);

    // Horse legs: hind at t = 2, fore at t = 7, diagonal pairs together
    for (int leg = 0; leg < 4; leg++) {
        int s = (leg & 1) == 0 ? -1 : 1;
        int t = leg >= 2 ? 7 : 2;
        int legSwing = (leg == 0 || leg == 3) ? swing : -swing;
        w.Put(s, t + legSwing, 0, Block::HORSE_DARK);
        w.Put(s, t + legSwing, 1, Block::HORSE_COAT);
        w.Put(s, t, 2, Block::HORSE_COAT);
        w.Put(s, t, 3, Block::HORSE_COAT);
    }
    // Body, rounded at the rump and the chest
    for (int t = 2; t <= 7; t++) {
        for (int s = -1; s <= 1; s++) {
            for (int y = 4; y <= 6; y++) {
                if (y == 6 && s != 0 && (t == 2 || t == 7)) continue;
                w.Put(s, t, y, Block::HORSE_COAT);
            }
        }
    }
    // Neck, head, muzzle, ears and mane
    w.Put(0, 8, 6, Block::HORSE_COAT);
    w.Put(0, 8, 7, Block::HORSE_COAT);
    w.Put(0, 8, 8, Block::HORSE_COAT);
    w.Put(0, 9, 8, Block::HORSE_COAT);
    w.Put(0, 9, 9, Block::HORSE_COAT);
    for (int t = 10; t <= 11; t++) {
        w.Put(0, t, 9, Block::HORSE_COAT);
        w.Put(0, t, 8, Block::HORSE_COAT);
    }
    w.Put(0, 12, 8, Block::HORSE_DARK);
    w.Put(0, 10, 10, Block::HORSE_DARK);
    w.Put(0, 7, 7, Block::HORSE_DARK);
    w.Put(0, 8, 9, Block::HORSE_DARK);
    w.Put(0, 9, 10, Block::HORSE_DARK);
    // Tail swishing with the trot
    int tailSide = (frame & 2) == 0 ? -1 : 1;
    w.Put(0, 1, 6, Block::HORSE_DARK);
    w.Put(0, 1, 5, Block::HORSE_DARK);
    w.Put(tailSide, 1, 4, Block::HORSE_DARK);
    w.Put(tailSide, 1, 3, Block::HORSE_DARK);

    // Harness: collar, saddle pad, girth strap, shafts, reins
    for (int y = 6; y <= 8; y++) {
        w.Put(-1, 8, y, Block::HARNESS);
        w.Put(1, 8, y, Block::HARNESS);
    }
    for (int s = -1; s <= 1; s++) {
        w.Put(s, 4, 7, Block::HARNESS);
        w.Put(s, 5, 7, Block::HARNESS);
    }
    for (int y = 4; y <= 6; y++) {
        w.Put(-1, 5, y, Block::HARNESS);
        w.Put(1, 5, y, Block::HARNESS);
    }
    for (int t = -1; t <= 7; t++) {
        w.Put(-2, t, 5, Block::WAGON_WOOD);
        w.Put(2, t, 5, Block::WAGON_WOOD);
    }
    for (int t = -2; t <= 7; t++) {
        if (t == 4 || t == 5) continue;
        w.Put(-1, t, 7, Block::HARNESS);
        w.Put(1, t, 7, Block::HARNESS);
    }

    // Wheels (4 x 4, iron rim, spokes turning with the frame), bed, side boards, front board
    for (int side = -3; side <= 3; side += 6) {
        for (int t0 : { -10, -6 }) {
            for (int y = 0; y < 4; y++) {
                for (int t = 0; t < 4; t++) {
                    bool rimRow = y == 0 || y == 3, rimColumn = t == 0 || t == 3;
                    if (rimRow && rimColumn) continue;
                    if (rimRow || rimColumn) w.Put(side, t0 + t, y, Block::WHEEL_IRON);
                    else if ((frame & 1) == 0 ? t == y : t + y == 3) w.Put(side, t0 + t, y, Block::WAGON_WOOD);
                }
            }
        }
    }
    for (int t = -10; t <= -2; t++) {
        for (int s = -2; s <= 2; s++) w.Put(s, t, 4, Block::WAGON_WOOD);
        w.Put(-2, t, 5, Block::WAGON_WOOD);
        w.Put(2, t, 5, Block::WAGON_WOOD);
    }
    for (int s = -1; s <= 1; s++) {
        w.Put(s, -10, 5, Block::WAGON_WOOD);
        w.Put(s, -2, 5, Block::WAGON_WOOD);
        w.Put(s, -2, 6, Block::WAGON_WOOD);
    }

    // The driver on the front of the bed, hands on the reins, straw hat
    for (int s = -1; s <= 1; s++) {
        w.Put(s, -4, 5, Block::PERSON_TROUSERS);
        w.Put(s, -5, 5, Block::PERSON_TROUSERS);
        for (int y = 6; y <= 8; y++) {
            w.Put(s, -4, y, Block::PERSON_SHIRT_FARMER);
            w.Put(s, -5, y, Block::PERSON_SHIRT_FARMER);
        }
        w.Put(s, -4, 9, Block::PERSON_SKIN);
        w.Put(s, -5, 9, Block::PERSON_SKIN);
    }
    w.Put(-1, -3, 5, Block::PERSON_TROUSERS);
    w.Put(1, -3, 5, Block::PERSON_TROUSERS);
    w.Put(-2, -4, 7, Block::PERSON_SHIRT_FARMER);
    w.Put(2, -4, 7, Block::PERSON_SHIRT_FARMER);
    w.Put(-1, -3, 7, Block::PERSON_SKIN);
    w.Put(1, -3, 7, Block::PERSON_SKIN);
    for (int s = -2; s <= 2; s++) {
        for (int t = -6; t <= -3; t++) w.Put(s, t, 10, Block::PERSON_STRAW_HAT);
    }
    for (int s = -1; s <= 1; s++) {
        w.Put(s, -4, 11, Block::PERSON_STRAW_HAT);
        w.Put(s, -5, 11, Block::PERSON_STRAW_HAT);
    }

    // Cargo: a row per good behind the driver; wood and pigs are piled, the rest in crates
    if (amount > 0) {
        uint8_t cargo = (uint8_t)(Block::CARGO_FIRST + item);
        bool pile = item == (int)ItemType::Wood || item == (int)ItemType::Pigs;
        for (int row = 0; row < std::min(amount, 4); row++) {
            int t = -6 - row;
            for (int s = -1; s <= 1; s++) {
                w.Put(s, t, 5, cargo);
                if (!pile || s == 0) w.Put(s, t, 6, cargo);
                if (!pile && s == 0 && (row & 1) == 0) w.Put(s, t, 7, cargo);
            }
        }
    }
    return model;
}
