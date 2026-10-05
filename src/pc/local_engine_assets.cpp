/* Super Rocket 64 local data loader. No game data is stored in this file. */
#include "local_engine_assets.h"
#include "utils/rocket_sha256.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Entry {
    void *destination;
    size_t size;
    std::string path;
    std::string hash;
};
std::vector<Entry>& entries() {
    static std::vector<Entry> result;
    return result;
}
bool relative_path(const std::string& path) {
    if (path.empty() || path.size() > 220 || path[0] == '/' || path.back() == '/') return false;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        std::string part = path.substr(start, end - start);
        if (part.empty() || part[0] == '.' || part.back() == '.' || part.back() == ' ') return false;
        for (unsigned char ch : part)
            if (ch < 32 || ch >= 127 || ch == '\\' || ch == ':' || ch == '?' || ch == '*' || ch == '"' || ch == '<' || ch == '>' || ch == '|') return false;
        start = end + 1;
    }
    return true;
}
}

extern "C" void local_engine_asset_register(void *destination, size_t size, const char *path, const char *hash) {
    entries().push_back(Entry{destination, size, path ? path : "", hash ? hash : ""});
}

extern "C" size_t local_engine_assets_count(void) { return entries().size(); }

extern "C" int local_engine_assets_load(const char *directory, char *error, size_t error_size) {
    if (error && error_size) error[0] = '\0';
    try {
        if (entries().empty()) return 1;
        if (!directory || !*directory)
            throw std::runtime_error("Local engine data is required. This development build needs its verified private asset folder.");
        std::string root(directory);
        if (root.size() > 3800) throw std::runtime_error("Local engine asset folder path is too long.");
        if (entries().size() > 4096) throw std::runtime_error("Invalid local engine asset inventory.");
        std::vector<std::vector<unsigned char> > pending;
        pending.reserve(entries().size());
        size_t total = 0;
        for (const Entry& entry : entries()) {
            if (!entry.destination || !entry.size || entry.size > 16u * 1024u * 1024u ||
                !relative_path(entry.path) || entry.hash.size() != 64)
                throw std::runtime_error("Invalid compiled local engine asset record.");
            for (char ch : entry.hash)
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                    throw std::runtime_error("Invalid compiled local engine asset hash.");
            total += entry.size;
            if (total > 128u * 1024u * 1024u) throw std::runtime_error("Local engine asset inventory is too large.");
            std::ifstream file(root + "/" + entry.path, std::ios::binary | std::ios::ate);
            if (!file || file.tellg() != static_cast<std::streamoff>(entry.size))
                throw std::runtime_error("Missing or wrong-size local engine asset: " + entry.path);
            file.seekg(0);
            std::vector<unsigned char> data(entry.size);
            if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size())))
                throw std::runtime_error("Could not read local engine asset: " + entry.path);
            if (rocket_assets::sha256(data) != entry.hash)
                throw std::runtime_error("Changed local engine asset; restore the verified local input: " + entry.path);
            pending.push_back(std::move(data));
        }
        for (size_t i = 0; i < entries().size(); ++i)
            std::memcpy(entries()[i].destination, pending[i].data(), pending[i].size());
        return 1;
    } catch (const std::exception& exception) {
        if (error && error_size) std::snprintf(error, error_size, "%s", exception.what());
        return 0;
    }
}
