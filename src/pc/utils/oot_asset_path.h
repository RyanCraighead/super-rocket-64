#ifndef OOT_ASSET_PATH_H
#define OOT_ASSET_PATH_H

/* Asset names crossing the C API and manifest are UTF-8. Keep canonical paths
 * in that encoding with '/' separators, and use wide Win32 APIs at the OS
 * boundary. In particular, _fullpath is not suitable here: it neither resolves
 * junctions/symlinks nor gives the same separators as POSIX realpath. */
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace oot_asset_path {
inline void validatePath(const std::string &path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::runtime_error("invalid asset path");
}
#ifdef _WIN32
inline std::wstring widePath(const std::string &path) {
    validatePath(path);
    if (path.size() > INT_MAX) throw std::runtime_error("asset path is too long");
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                                  static_cast<int>(path.size()), nullptr, 0);
    if (!count) throw std::runtime_error("asset path is not valid UTF-8");
    std::wstring wide(static_cast<size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                           static_cast<int>(path.size()), &wide[0], count) != count)
        throw std::runtime_error("cannot convert asset path to UTF-16");
    // Extended-length paths returned by GetFinalPathNameByHandleW require
    // native separators, even though the public helper returns generic ones.
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    return wide;
}
inline std::string utf8Path(const std::wstring &path) {
    if (path.empty() || path.size() > INT_MAX)
        throw std::runtime_error("invalid canonical asset path");
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(),
                                  static_cast<int>(path.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("cannot convert asset path to UTF-8");
    std::string result(static_cast<size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(),
                           static_cast<int>(path.size()), &result[0], count, nullptr, nullptr) != count)
        throw std::runtime_error("cannot convert asset path to UTF-8");
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}
struct PathHandle {
    HANDLE value;
    explicit PathHandle(HANDLE handle) : value(handle) {}
    ~PathHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    PathHandle(const PathHandle &) = delete;
    PathHandle &operator=(const PathHandle &) = delete;
};
#endif
inline std::string canonical(const std::string &path) {
    validatePath(path);
#ifdef _WIN32
    const std::wstring wide = widePath(path);
    PathHandle handle(CreateFileW(wide.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle.value == INVALID_HANDLE_VALUE)
        throw std::runtime_error("cannot resolve asset path: " + path);
    // Do not use FILE_FLAG_OPEN_REPARSE_POINT: the target must be resolved
    // before any containment decision, including directory junction targets.
    DWORD count = GetFinalPathNameByHandleW(handle.value, nullptr, 0, FILE_NAME_NORMALIZED);
    if (!count) throw std::runtime_error("cannot resolve final asset path: " + path);
    std::vector<wchar_t> result(static_cast<size_t>(count) + 1);
    DWORD length = GetFinalPathNameByHandleW(handle.value, result.data(),
                                           static_cast<DWORD>(result.size()), FILE_NAME_NORMALIZED);
    if (!length || length >= result.size())
        throw std::runtime_error("cannot resolve final asset path: " + path);
    return utf8Path(std::wstring(result.data(), length));
#else
    std::unique_ptr<char, decltype(&std::free)> result(realpath(path.c_str(), nullptr), &std::free);
    if (!result) throw std::runtime_error("cannot resolve asset path: " + path);
    return result.get();
#endif
}
inline std::string child(const std::string &canonicalRoot, const std::string &relative, size_t maxLength) {
    validatePath(canonicalRoot);
    if (relative.empty() || relative.size() > maxLength || relative[0] == '/' ||
        relative.find_first_of("\\:") != std::string::npos || relative.find('\0') != std::string::npos)
        throw std::runtime_error("invalid asset-relative path");
    size_t start = 0;
    while (start <= relative.size()) {
        size_t end = relative.find('/', start);
        std::string part = relative.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == "..")
            throw std::runtime_error("asset path may not traverse directories");
        if (end == std::string::npos) break;
        start = end + 1;
    }
    const std::string prefix = canonicalRoot + (canonicalRoot.back() == '/' ? "" : "/");
    const std::string result = canonical(prefix + relative);
    if (result.size() <= prefix.size() || result.compare(0, prefix.size(), prefix) != 0)
        throw std::runtime_error("asset path escapes local directory");
    return result;
}
struct FileCloser { void operator()(FILE *file) const { std::fclose(file); } };
inline std::vector<unsigned char> readFile(const std::string &path, size_t max, size_t exact = 0) {
    validatePath(path);
#ifdef _WIN32
    FILE *raw = _wfopen(widePath(path).c_str(), L"rb");
#else
    FILE *raw = std::fopen(path.c_str(), "rb");
#endif
    std::unique_ptr<FILE, FileCloser> file(raw);
    if (!file) throw std::runtime_error("cannot open asset: " + path);
    if (std::fseek(file.get(), 0, SEEK_END) != 0)
        throw std::runtime_error("cannot measure asset: " + path);
    long size = std::ftell(file.get());
    if (size < 0 || static_cast<uint64_t>(size) > max || (exact && static_cast<uint64_t>(size) != exact))
        throw std::runtime_error("asset size mismatch/limit: " + path);
    if (std::fseek(file.get(), 0, SEEK_SET) != 0)
        throw std::runtime_error("cannot seek asset: " + path);
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    if (!bytes.empty() && std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
        throw std::runtime_error("cannot read asset: " + path);
    return bytes;
}
} // namespace oot_asset_path
#endif
