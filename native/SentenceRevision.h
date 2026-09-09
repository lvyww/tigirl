#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <stdexcept>
#include "SentenceLexicon.h"
#include "Lexicon.h"
#pragma comment(lib,"bcrypt.lib")
namespace tiger {
// Worker/helper operation, never per key. Hash mapped base/inventory bytes and
// the effective overlay (including new-key order), not journal timestamps or
// append history. Identical snapshots survive journal compaction unchanged.
inline std::u16string sentenceRevision(const SentenceLexicon& inventory,const Lexicon& snapshot) {
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        ~Hash(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
        void bytes(const void* data,std::uint64_t size) {
            auto p=static_cast<const unsigned char*>(data);
            while(size){const ULONG n=static_cast<ULONG>(size>1048576?1048576:size);
                if(BCryptHashData(hash,const_cast<PUCHAR>(p),n,0)<0)throw std::runtime_error("Cannot hash sentence revision");
                p+=n;size-=n;}
        }
        void number(std::uint64_t n){unsigned char b[8];for(unsigned i=0;i<8;++i)b[i]=static_cast<unsigned char>(n>>(8*i));bytes(b,8);}
        void text(std::u16string_view s){number(s.size());bytes(s.data(),s.size()*2);}
        void dictionary(const Dictionary& d){number(d.mappedBytes());bytes(d.baseAddress(),d.mappedBytes());}
    } h;
    if(BCryptOpenAlgorithmProvider(&h.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
       BCryptCreateHash(h.algorithm,&h.hash,nullptr,0,nullptr,0,0)<0)
        throw std::runtime_error("Cannot initialize sentence revision hash");
    h.text(u"native-sentence-revision-1");
    h.dictionary(*snapshot.dictionary());h.dictionary(*inventory.dictionary());
    h.number(snapshot.editedCodes());
    snapshot.visitUserEdits([&](std::u16string_view code,const auto& values){
        h.text(code);h.number(values.size());for(const auto& value:values)h.text(value);
    });
    h.number(snapshot.addedCodes().size());for(const auto& code:snapshot.addedCodes())h.text(code);
    std::array<unsigned char,32> digest{};
    if(BCryptFinishHash(h.hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)
        throw std::runtime_error("Cannot finish sentence revision hash");
    constexpr char16_t hex[]=u"0123456789abcdef";std::u16string result;
    for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
}
