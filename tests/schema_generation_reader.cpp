#include "SchemaCatalog.h"
#include "Dictionary.h"
#include <iostream>
#include <iomanip>
static void value(const std::shared_ptr<const tiger::Dictionary>& dictionary) {
    const auto entry=dictionary->find(tiger::Section::Main,u"ab");
    if(entry.count!=1) throw std::runtime_error("Expected one candidate");
    for(char16_t c:dictionary->value(entry,0)) std::cout<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=2) return 2;
        const std::filesystem::path schema(argv[1]);
        const auto old=tiger::Dictionary::Open(tiger::schemaDictionaryPath(schema));
        std::cout<<"ready ";value(old);std::cout<<std::endl;
        for(std::string command;std::getline(std::cin,command);) {
            if(command=="quit") return 0;
            const auto current=tiger::Dictionary::Open(tiger::schemaDictionaryPath(schema));
            value(old);std::cout<<' ';value(current);std::cout<<std::endl;
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
