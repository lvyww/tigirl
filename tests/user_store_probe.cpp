#include "UserStore.h"
#include "Text.h"
#include <iostream>

int main(int argc,char** argv) {
    try {
        if(argc<5) throw std::runtime_error("user_store_probe <dictionary> <journal> <dump|write|checkpoint> <code> [prefix] [count]");
        auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        tiger::UserStore store(dictionary,std::filesystem::u8path(argv[2]));
        auto code=tiger::utf16(argv[4]);
        if(std::string(argv[3])=="checkpoint") {
            const auto bytes=store.checkpoint();
            // Hex avoids Windows stdout newline translation of binary bytes.
            constexpr char hex[]="0123456789abcdef";
            for(auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];
            std::cout<<'\n';return 0;
        }
        if(std::string(argv[3])=="write") {
            if(argc!=7) throw std::runtime_error("write needs prefix and count");
            const int count=std::stoi(argv[6]);
            for(int i=0;i<count;++i)
                store.commit({{tiger::ChangeKind::Add,code,tiger::utf16(std::string(argv[5])+std::to_string(i))}});
        } else if(std::string(argv[3])!="dump") throw std::runtime_error("Unknown command");
        auto snapshot=store.refresh();
        if(snapshot->dictionary()!=dictionary) throw std::runtime_error("Journal duplicated main dictionary");
        auto entry=snapshot->find(tiger::Section::Main,code);
        constexpr char hex[]="0123456789abcdef";
        for(unsigned i=0;i<entry.count;++i) {
            for(auto ch:snapshot->value(entry,i)) std::cout<<hex[(ch>>12)&15]<<hex[(ch>>8)&15]<<hex[(ch>>4)&15]<<hex[ch&15];
            std::cout<<'\n';
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
