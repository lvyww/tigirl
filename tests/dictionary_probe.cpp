#include "Dictionary.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <vector>
#endif

static void json(std::u16string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::cout << '"';
    for (const auto ch : text) {
        std::cout << "\\u" << hex[(ch >> 12) & 15] << hex[(ch >> 8) & 15]
                  << hex[(ch >> 4) & 15] << hex[ch & 15];
    }
    std::cout << '"';
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    try {
        if (argc < 2) throw std::runtime_error("dictionary_probe <file> [dump|bench|hold]");
        const auto start = std::chrono::steady_clock::now();
        auto dictionary = tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        if (dictionary != tiger::Dictionary::Open(std::filesystem::u8path(argv[1])))
            throw std::runtime_error("process-local mapping was not reused");
        const auto opened = std::chrono::steady_clock::now();
        const std::string command = argc > 2 ? argv[2] : "check";
        std::uint64_t values = 0, records = 0, units = 0;
        for (unsigned section = 1; section <= 6; ++section) {
            const auto type = static_cast<tiger::Section>(section);
            for (std::uint32_t i = 0; i < dictionary->count(type); ++i) {
                const auto entry = dictionary->at(type, i);
                const auto found = dictionary->find(type, entry.key);
                if (found.key != entry.key || found.flags != entry.flags || found.count != entry.count)
                    throw std::runtime_error("index lookup differs from enumeration");
                ++records;
                if (command == "dump") {
                    std::cout << "{\"section\":" << section << ",\"key\":";
                    json(entry.key);
                    std::cout << ",\"flags\":" << entry.flags << ",\"values\":[";
                }
                for (std::uint32_t v = 0; v < entry.count; ++v) {
                    const auto text = dictionary->value(entry, v);
                    ++values; units += text.size();
                    if (command == "dump") { if (v) std::cout << ','; json(text); }
                }
                if (command == "dump") std::cout << "]}\n";
            }
        }
        if (command != "dump") {
            const auto elapsed = std::chrono::duration<double, std::milli>(opened - start).count();
            std::cout << "mapped=" << dictionary->mappedBytes() << " records=" << records
                      << " values=" << values << " units=" << units << " open_ms=" << elapsed << '\n';
        }
        if (command == "bench") {
            constexpr unsigned rounds = 1000000;
            std::uint64_t hits = 0;
            const auto before = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < rounds; ++i) {
                const auto sample = dictionary->at(tiger::Section::Main, i % dictionary->count(tiger::Section::Main));
                hits += dictionary->find(tiger::Section::Main, sample.key).count;
            }
            const auto ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - before).count();
            std::cout << "lookup_ns=" << ns / rounds << " checksum=" << hits << '\n';
        }
        if (command == "hold") {
            std::cout << "ready" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(45));
        }
#ifdef _WIN32
        if (command == "memory") {
            SYSTEM_INFO system{};
            GetSystemInfo(&system);
            const auto* base = static_cast<const unsigned char*>(dictionary->baseAddress());
            volatile unsigned char touch = 0;
            for (std::uint64_t i = 0; i < dictionary->mappedBytes(); i += system.dwPageSize)
                touch = static_cast<unsigned char>(touch ^ base[i]);
            std::cout << "ready" << std::endl;
            std::cin.get(); // Parent waits until all independent processes have faulted pages in.
            const auto pageCount = (dictionary->mappedBytes() + system.dwPageSize - 1) / system.dwPageSize;
            std::vector<PSAPI_WORKING_SET_EX_INFORMATION> pages(static_cast<std::size_t>(pageCount));
            for (std::size_t i = 0; i < pages.size(); ++i)
                pages[i].VirtualAddress = const_cast<unsigned char*>(base + i * system.dwPageSize);
            if (!QueryWorkingSetEx(GetCurrentProcess(), pages.data(), static_cast<DWORD>(pages.size() * sizeof(pages[0]))))
                throw std::runtime_error("QueryWorkingSetEx failed");
            std::uint64_t resident = 0, shared = 0, multiple = 0;
            for (const auto& page : pages) if (page.VirtualAttributes.Valid) {
                ++resident;
                if (page.VirtualAttributes.Shared) ++shared;
                if (page.VirtualAttributes.ShareCount > 1) ++multiple;
            }
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
                throw std::runtime_error("GetProcessMemoryInfo failed");
            std::cout << "{\"pid\":" << GetCurrentProcessId() << ",\"mapped_bytes\":" << dictionary->mappedBytes()
                      << ",\"private_bytes\":" << memory.PrivateUsage << ",\"pages\":" << pageCount
                      << ",\"resident_pages\":" << resident << ",\"shareable_pages\":" << shared
                      << ",\"multiply_shared_pages\":" << multiple << ",\"touch\":" << static_cast<unsigned>(touch) << "}" << std::endl;
            std::cin.get(); // Keep all readers alive until every measurement is collected.
        }
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
