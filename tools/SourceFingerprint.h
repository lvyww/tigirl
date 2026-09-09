#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <array>
#include "LexiconOrder.h"
#include "OrdinalCase.h"
#pragma comment(lib,"bcrypt.lib")

// Hash contents and directory enumeration order (which breaks collation ties).
// No timestamp-only cache: edits with preserved timestamps must invalidate it.
inline std::u16string sourceFingerprint(const std::filesystem::path& source,
    const std::filesystem::path& pinyin,const std::string& culture) {
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
        ~Hash(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
        void add(const void* data,std::size_t size) {
            if(BCryptHashData(hash,(PUCHAR)data,static_cast<ULONG>(size),0)<0)throw std::runtime_error("Cannot hash source");
        }
        void text(std::u16string_view value) {auto n=static_cast<std::uint64_t>(value.size());add(&n,sizeof(n));add(value.data(),value.size()*2);}
    } h;
    if(BCryptOpenAlgorithmProvider(&h.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
       BCryptCreateHash(h.algorithm,&h.hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Cannot initialize source hash");
    h.text(u"native-folder-import-v4-sentence-source-spelling");h.text(std::u16string(culture.begin(),culture.end()));
    for(const auto& directory:{source,pinyin}) {
        h.text(std::filesystem::absolute(directory).lexically_normal().u16string());
        if(!std::filesystem::is_directory(directory)) {h.text(u"missing");continue;}
        h.text(u"present");
        for(const auto& file:tiger::enumerateLexiconFiles(directory)) {
            const auto name=tiger::ordinalCaseKey(file.filename().u16string());
            const auto ends=[&](std::u16string_view suffix){return name.size()>=suffix.size() && std::u16string_view(name).substr(name.size()-suffix.size())==suffix;};
            if(!ends(u".TXT") && !ends(u".DICT.YAML") && !ends(u".注释") && !ends(u".拆分"))continue;
            h.text(file.filename().u16string());
            auto size=std::filesystem::file_size(file);h.add(&size,sizeof(size));
            std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot read source for hashing");
            std::array<char,65536> buffer{};
            while(input){input.read(buffer.data(),buffer.size());h.add(buffer.data(),static_cast<std::size_t>(input.gcount()));}
            if(!input.eof())throw std::runtime_error("Source hashing read failed");
        }
    }
    std::array<unsigned char,32> digest{};
    if(BCryptFinishHash(h.hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)throw std::runtime_error("Cannot finish source hash");
    constexpr char16_t hex[]=u"0123456789abcdef";std::u16string result;
    for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
