#include "Simulation/SaveGame.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

bool SaveWriter::WriteFile(const std::string& path) const {
    std::filesystem::path file(path);
    std::error_code error;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
    // Written beside it first, then renamed over it: a crash mid-write never ruins the old save
    std::filesystem::path temporary = file;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(m_Data.data()), (std::streamsize)m_Data.size());
        if (!out) return false;
    }
    std::filesystem::rename(temporary, file, error);
    return !error;
}

bool SaveReader::ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    m_Data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    m_Position = 0;
    m_Ok = true;
    return true;
}

std::vector<SaveFileInfo> ListSaves(const std::string& directory) {
    std::vector<SaveFileInfo> saves;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error) || entry.path().extension() != ".sav") continue;
        SaveFileInfo info;
        info.path = entry.path().string();
        info.name = entry.path().stem().string();
        std::ifstream in(entry.path(), std::ios::binary);
        uint32_t header[2] = {};
        in.read(reinterpret_cast<char*>(header), sizeof(header));
        if (in && header[0] == SAVE_MAGIC && header[1] == SAVE_VERSION) {
            in.read(reinterpret_cast<char*>(&info.summary), sizeof(info.summary));
            info.loadable = (bool)in;
        }
        saves.push_back(info);
    }
    std::sort(saves.begin(), saves.end(), [](const SaveFileInfo& a, const SaveFileInfo& b) {
        return a.summary.savedAt != b.summary.savedAt ? a.summary.savedAt > b.summary.savedAt : a.name < b.name;
    });
    return saves;
}
