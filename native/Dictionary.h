#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace tiger {

enum class Section : std::uint32_t {
    Main = 1, Pinyin, Comment, Split, FullCode, ConstructCode
};

// Views point directly into an immutable file mapping. Keep the Dictionary alive
// while retaining an Entry or string_view. No table-wide heap deserialization.
class Dictionary final {
public:
    struct Entry {
        std::u16string_view key;
        std::uint32_t flags = 0;
        std::uint32_t count = 0;
        bool unique() const noexcept { return (flags & 1) != 0; }
        bool prefix() const noexcept { return (flags & 2) != 0; }
        bool autoSymbol() const noexcept { return (flags & 4) != 0; }
    private:
        friend class Dictionary;
        std::uint64_t values = 0;
    };

    static std::shared_ptr<const Dictionary> Open(const std::filesystem::path& path);
    ~Dictionary();
    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Entry find(Section section, std::u16string_view key) const noexcept;
    Entry at(Section section, std::uint32_t index) const noexcept;
    std::u16string_view value(const Entry& entry, std::uint32_t index) const noexcept;
    std::uint32_t count(Section section) const noexcept;
    std::uint32_t quickSymbols() const noexcept { return quick_; }
    std::uint64_t mappedBytes() const noexcept { return length_; }
    const void* baseAddress() const noexcept { return data_; }

private:
    Dictionary() = default;
    void map(const std::filesystem::path& path);
    void validate();
    std::uint32_t u32(std::uint64_t offset) const noexcept;
    std::uint64_t u64(std::uint64_t offset) const noexcept;
    std::u16string_view string(std::uint64_t offset, std::uint32_t length) const noexcept;
    struct Index { std::uint64_t offset = 0; std::uint32_t count = 0; };
    std::array<Index, 6> indices_{};
    const unsigned char* data_ = nullptr;
    std::uint64_t length_ = 0;
    std::uint32_t quick_ = 0;
#ifdef _WIN32
    void* file_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
    void* mapping_ = nullptr;
#else
    int file_ = -1;
#endif
};

}
