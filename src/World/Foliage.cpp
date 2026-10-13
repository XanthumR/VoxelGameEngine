#include "World/Foliage.h"

#include <algorithm>
#include <iostream>

bool FoliageLibrary::Load(const std::string& assetDirectory) {
    m_Models.clear();
    m_Models.reserve(FOLIAGE_KINDS.size() * 4);
    m_Reach = m_Height = 0;
    for (const FoliageKind& kind : FOLIAGE_KINDS) {
        VoxModel model;
        if (!model.Load(assetDirectory + "/foliage/" + kind.file + ".vox")) {
            std::cerr << "Foliage model missing: " << kind.file << std::endl;
            m_Models.clear();
            return false;
        }
        for (int rotation = 0; rotation < 4; rotation++) {
            m_Models.push_back(model);
            for (VoxelOffset& voxel : model.voxels) voxel = { -voxel.z, voxel.y, voxel.x, voxel.blockType }; // A quarter turn
            std::swap(model.min.x, model.min.z);
            std::swap(model.max.x, model.max.z);
            model.min.x = -model.min.x;
            model.max.x = -model.max.x;
            std::swap(model.min.x, model.max.x);
        }
        m_Height = std::max(m_Height, model.max.y);
    }
    for (const VoxModel& model : m_Models) m_Reach = std::max({ m_Reach, -model.min.x, -model.min.z, model.max.x, model.max.z });
    return true;
}
