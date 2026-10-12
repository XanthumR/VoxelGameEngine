#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// Save games: each system writes its state as plain values and arrays of them, in a fixed order,
// and reads it back in the same order (Simulation::Save / Load). Values are copied byte for byte,
// so a save only loads in the build that wrote it or one with the same layouts: SAVE_VERSION goes
// up whenever a saved struct or the order changes.
constexpr uint32_t SAVE_MAGIC = 0x54584F56; // "VOXT"
constexpr uint32_t SAVE_VERSION = 3; // 3: the islands laid out by size class

// What the list of saves shows of each, written right after the magic word and the version, so
// listing reads only the start of each file
struct SaveSummary {
    int64_t savedAt = 0;      // Seconds since 1970 (std::time)
    double playSeconds = 0.0; // Game time played
    int64_t coins = 0;
    int32_t residents = 0;
    int32_t islands = 0;      // Settled
};

struct SaveFileInfo {
    std::string path, name; // name: the file's name without .sav
    SaveSummary summary;
    bool loadable = false;  // Written by this version
};
// The .sav files in a directory, newest first
std::vector<SaveFileInfo> ListSaves(const std::string& directory);

class SaveWriter {
public:
    template <typename T>
    void Write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "only plain data is saved");
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        m_Data.insert(m_Data.end(), bytes, bytes + sizeof(T));
    }
    // A count, then the values
    template <typename T>
    void WriteArray(const T* values, size_t count) {
        static_assert(std::is_trivially_copyable_v<T>, "only plain data is saved");
        Write((uint64_t)count);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(values);
        m_Data.insert(m_Data.end(), bytes, bytes + count * sizeof(T));
    }
    template <typename T>
    void WriteVector(const std::vector<T>& values) { WriteArray(values.data(), values.size()); }

    const std::vector<uint8_t>& Data() const { return m_Data; }
    bool WriteFile(const std::string& path) const;

private:
    std::vector<uint8_t> m_Data;
};

// Reads what SaveWriter wrote. Reading past the end or an array longer than allowed fails: the
// reader stops (Ok() is false) and every later read gives zeros, so a damaged file is never
// trusted (the caller checks Ok() before using what was loaded).
class SaveReader {
public:
    SaveReader() = default;
    explicit SaveReader(std::vector<uint8_t> data) : m_Data(std::move(data)) {}
    bool ReadFile(const std::string& path);

    template <typename T>
    void Read(T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "only plain data is saved");
        if (!Take(&value, sizeof(T))) std::memset(&value, 0, sizeof(T));
    }
    template <typename T>
    T Read() {
        T value;
        Read(value);
        return value;
    }
    // Up to maxCount values into a vector (resized to the count)
    template <typename T>
    void ReadVector(std::vector<T>& values, size_t maxCount) {
        uint64_t count = Read<uint64_t>();
        if (count > maxCount) {
            m_Ok = false;
            count = 0;
        }
        values.resize((size_t)count);
        if (count > 0 && !Take(values.data(), (size_t)count * sizeof(T))) values.clear();
    }
    // Exactly count values into an array (a different count fails)
    template <typename T>
    void ReadArray(T* values, size_t count) {
        if (Read<uint64_t>() != count) m_Ok = false;
        if (!Take(values, count * sizeof(T))) std::memset(values, 0, count * sizeof(T));
    }

    bool Ok() const { return m_Ok; }
    void Fail() { m_Ok = false; }
    bool AtEnd() const { return m_Position == m_Data.size(); }

private:
    bool Take(void* out, size_t size) {
        if (!m_Ok || size > m_Data.size() - m_Position) {
            m_Ok = false;
            return false;
        }
        std::memcpy(out, m_Data.data() + m_Position, size);
        m_Position += size;
        return true;
    }

    std::vector<uint8_t> m_Data;
    size_t m_Position = 0;
    bool m_Ok = true;
};
