#include "LexiconDecode.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
#else
int main(int argc,char** argv) {
#endif
    try {
        if(argc!=2 && argc!=3) return 2;
        std::ifstream input{std::filesystem::path(argv[1])};if(!input) return 2;
        const auto fixture=argc==3?std::filesystem::path(argv[2]):std::filesystem::path{};
        if(argc==3 && std::filesystem::exists(fixture)) throw std::runtime_error("Fixture already exists");
        std::string line;
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.size()%2) throw std::runtime_error("Odd hex input");
            std::string bytes;
            for(std::size_t i=0;i<line.size();i+=2) bytes.push_back(static_cast<char>(std::stoul(line.substr(i,2),nullptr,16)));
            auto text=tiger::decodeLexiconBytes(bytes);
            if(argc==3) {
                std::ofstream output(fixture,std::ios::binary|std::ios::trunc);
                output.exceptions(std::ios::badbit|std::ios::failbit);
                output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));output.close();
                auto fromFile=tiger::readLexiconText(fixture);
                if(text!=fromFile) throw std::runtime_error("File decoding differs from supplied bytes");
                text=std::move(fromFile);
            }
            for(char16_t unit:text) std::cout<<std::hex<<std::setfill('0')<<std::setw(4)<<static_cast<unsigned>(unit);
            std::cout<<'\n';
        }
        if(argc==3) {
            unsigned rejected=0;
            for(const auto& bad:{fixture.parent_path(),fixture.parent_path()/"missing-file"}) {
                try {tiger::readLexiconText(bad);} catch(const std::exception&) {++rejected;}
            }
            if(rejected!=2) throw std::runtime_error("Invalid file accepted");
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
