#include "Dictionary.h"
#include "FileCachePath.h"
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tiger {
namespace {
[[noreturn]] void invalid(const char* reason) {
    throw std::runtime_error(std::string("Invalid TigerClaw dictionary: ") + reason);
}
std::size_t slot(Section section) noexcept {
    return static_cast<std::uint32_t>(section) - 1u;
}
}

std::shared_ptr<const Dictionary> Dictionary::Open(const std::filesystem::path& path) {
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::weak_ptr<const Dictionary>> cache;
    const auto canonical = fileCachePath(path);
    std::lock_guard<std::mutex> guard(mutex);
    auto found = cache.find(canonical);
    if (found != cache.end()) if (auto live = found->second.lock()) return live;
    auto dictionary = std::shared_ptr<Dictionary>(new Dictionary());
    dictionary->map(canonical);
    dictionary->validate();
    for (auto it = cache.begin(); it != cache.end();)
        if (it->second.expired()) it = cache.erase(it); else ++it;
    cache[canonical] = dictionary;
    return dictionary;
}

void Dictionary::map(const std::filesystem::path& path) {
#ifdef _WIN32
    // No write sharing: this generation must stay immutable for every reader.
    file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open dictionary");
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file_, &size) || size.QuadPart < 128) invalid("file size");
    length_ = static_cast<std::uint64_t>(size.QuadPart);
    if (length_ > std::numeric_limits<std::size_t>::max()) invalid("address space");
    mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) throw std::runtime_error("Cannot create dictionary mapping");
    data_ = static_cast<const unsigned char*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
    if (!data_) throw std::runtime_error("Cannot map dictionary");
#else
    file_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (file_ < 0) throw std::runtime_error("Cannot open dictionary");
    struct stat info{};
    if (fstat(file_, &info) || info.st_size < 128) invalid("file size");
    length_ = static_cast<std::uint64_t>(info.st_size);
    if (length_ > std::numeric_limits<std::size_t>::max()) invalid("address space");
    void* view = mmap(nullptr, static_cast<std::size_t>(length_), PROT_READ, MAP_SHARED, file_, 0);
    if (view == MAP_FAILED) throw std::runtime_error("Cannot map dictionary");
    data_ = static_cast<const unsigned char*>(view);
#endif
}

Dictionary::~Dictionary() {
#ifdef _WIN32
    if (data_) UnmapViewOfFile(data_);
    if (mapping_) CloseHandle(mapping_);
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
#else
    if (data_) munmap(const_cast<unsigned char*>(data_), static_cast<std::size_t>(length_));
    if (file_ >= 0) close(file_);
#endif
}

std::uint32_t Dictionary::u32(std::uint64_t offset) const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}
std::uint64_t Dictionary::u64(std::uint64_t offset) const noexcept {
    std::uint64_t value;
    std::memcpy(&value, data_ + offset, sizeof(value));
    return value;
}
std::u16string_view Dictionary::string(std::uint64_t offset, std::uint32_t length) const noexcept {
    return {reinterpret_cast<const char16_t*>(data_ + offset), length};
}

void Dictionary::validate() {
    const std::uint32_t endian = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian) != 1) invalid("requires little endian");
    if (std::memcmp(data_, "TIGERD02", 8) || u32(8) != 2 || u32(12) != 6 ||
        u64(16) != length_ || u32(28) != 0 || (length_ & 1)) invalid("header");
    quick_ = u32(24);
    if (quick_ & ~15u) invalid("quick-symbol flags");
    auto range = [this](std::uint64_t offset, std::uint64_t size) {
        if (offset > length_ || size > length_ - offset) invalid("offset outside file");
    };
    std::uint64_t recordsEnd = 128;
    for (std::size_t i = 0; i < indices_.size(); ++i) {
        const std::uint64_t directory = 32 + i * 16;
        if (u32(directory) != i + 1 || u64(directory + 8) != recordsEnd) invalid("section directory");
        auto& index = indices_[i];
        index.count = u32(directory + 4);
        index.offset = recordsEnd;
        const std::uint64_t bytes = static_cast<std::uint64_t>(index.count) * 32;
        range(recordsEnd, bytes);
        recordsEnd += bytes;
    }
    std::uint64_t valuesEnd = recordsEnd;
    for (const auto& index : indices_) for (std::uint32_t n = 0; n < index.count; ++n) {
        const auto record = index.offset + static_cast<std::uint64_t>(n) * 32;
        if (u64(record + 16) != valuesEnd || u32(record + 28) != 0) invalid("value directory");
        const auto bytes = static_cast<std::uint64_t>(u32(record + 24)) * 16;
        range(valuesEnd, bytes);
        valuesEnd += bytes;
    }
    auto checkString = [&](std::uint64_t offset, std::uint32_t size) {
        if (offset < valuesEnd || (offset & 1)) invalid("string pool offset");
        range(offset, static_cast<std::uint64_t>(size) * 2);
    };
    for (std::size_t i = 0; i < indices_.size(); ++i) {
        const auto& index = indices_[i];
        std::u16string_view previous;
        for (std::uint32_t n = 0; n < index.count; ++n) {
            const auto record = index.offset + static_cast<std::uint64_t>(n) * 32;
            checkString(u64(record), u32(record + 8));
            const auto key = string(u64(record), u32(record + 8));
            if (key.empty() || (n && previous >= key)) invalid("unsorted or duplicate keys");
            previous = key;
            const auto flags = u32(record + 12);
            const auto count = u32(record + 24);
            if ((i == 0 && (flags & ~15u)) || (i != 0 && flags)) invalid("entry flags");
            if (i == 0 && count && !(flags & 8)) invalid("missing exact-key flag");
            if (i >= 2 && count != 1) invalid("metadata cardinality");
            if ((flags & 1) && (count != 1 || (flags & 2))) invalid("unique flag");
            for (std::uint32_t v = 0; v < count; ++v) {
                const auto value = u64(record + 16) + static_cast<std::uint64_t>(v) * 16;
                if (u32(value + 12)) invalid("reserved value field");
                checkString(u64(value), u32(value + 8));
            }
        }
    }
}

std::uint32_t Dictionary::count(Section section) const noexcept {
    const auto i = slot(section);
    return i < indices_.size() ? indices_[i].count : 0;
}
Dictionary::Entry Dictionary::at(Section section, std::uint32_t n) const noexcept {
    Entry entry;
    if (n >= count(section)) return entry;
    const auto offset = indices_[slot(section)].offset + static_cast<std::uint64_t>(n) * 32;
    entry.key = string(u64(offset), u32(offset + 8));
    entry.flags = u32(offset + 12);
    entry.values = u64(offset + 16);
    entry.count = u32(offset + 24);
    return entry;
}
Dictionary::Entry Dictionary::find(Section section, std::u16string_view key) const noexcept {
    std::uint32_t low = 0, high = count(section);
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        auto entry = at(section, mid);
        const int comparison = entry.key.compare(key);
        if (!comparison) return entry;
        if (comparison < 0) low = mid + 1;
        else high = mid;
    }
    return {};
}
std::u16string_view Dictionary::value(const Entry& entry, std::uint32_t n) const noexcept {
    if (n >= entry.count) return {};
    const auto offset = entry.values + static_cast<std::uint64_t>(n) * 16;
    // Entry is public and can be copied between mappings; fail safely in that case.
    if (offset < entry.values || offset > length_ || length_ - offset < 16) return {};
    const auto stringOffset = u64(offset);
    const auto size = u32(offset + 8);
    if ((stringOffset & 1) || stringOffset > length_ ||
        static_cast<std::uint64_t>(size) * 2 > length_ - stringOffset) return {};
    return string(stringOffset, size);
}
}
