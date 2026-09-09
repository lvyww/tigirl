#pragma once
#include <filesystem>
#include <memory>
#include <vector>
namespace tiger::tsf {
// Process-private GDI resources, reused across TSF services in the same process.
class PrivateFonts final {
public:
    static std::shared_ptr<PrivateFonts> Open(const std::filesystem::path& directory);
    ~PrivateFonts();
    PrivateFonts(const PrivateFonts&)=delete;
    PrivateFonts& operator=(const PrivateFonts&)=delete;
    const std::vector<std::filesystem::path>& paths() const { return files_; }
    unsigned faces() const { return faces_; }
    std::size_t files() const { return files_.size(); }
private:
    PrivateFonts()=default;
    void load(const std::filesystem::path& directory);
    std::vector<std::filesystem::path> files_;
    unsigned faces_=0;
};
}
