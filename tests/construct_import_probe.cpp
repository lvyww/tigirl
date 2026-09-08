#include "LexiconImport.h"
#include "LexiconDirectory.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static std::u16string decode(std::string_view hex) {
    if(hex.size()%4) throw std::runtime_error("Invalid UTF-16 hex");std::u16string result;
    for(std::size_t i=0;i<hex.size();i+=4) result+=static_cast<char16_t>(std::stoul(std::string(hex.substr(i,4)),nullptr,16));
    return result;
}
static void json(std::u16string_view text) {
    const char* hex="0123456789abcdef";std::cout<<'"';
    for(auto c:text) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];std::cout<<'"';
}
static void lookup(const std::map<std::u16string,std::u16string>& values) {
    std::cout<<'[';bool first=true;for(const auto& entry:values) {
        if(!first) std::cout<<',';first=false;std::cout<<'[';json(entry.first);std::cout<<',';json(entry.second);std::cout<<']';
    } std::cout<<']';
}
int main(int argc,char** argv) {
 try {
    if(argc!=2 && argc!=3) throw std::runtime_error("construct_import_probe <cases.hex> [--directories]");
    std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Cannot open cases");
    const bool all=argc==3 && std::string_view(argv[2])=="--all-directories";
    for(std::string line;std::getline(input,line);) {
        tiger::ImportedLexicon imported;
        if(argc==3 && (all || std::string_view(argv[2])=="--directories")) {
            auto tab=line.find('\t');if(tab==line.npos) throw std::runtime_error("Invalid directory case");
            if(all) {
                auto next=line.find('\t',tab+1);if(next==line.npos) throw std::runtime_error("Missing pinyin directory");
                imported=tiger::importLexiconDirectory(std::filesystem::u8path(line.substr(tab+1,next-tab-1)),
                    std::filesystem::u8path(line.substr(next+1)),line.substr(0,tab));
            } else imported=tiger::importMainDirectory(std::filesystem::u8path(line.substr(tab+1)),line.substr(0,tab));
        } else {
        const auto separator=line.find(' ',2);if(separator==line.npos) throw std::runtime_error("Invalid case");
        auto rows=tiger::parseLexiconRows(decode(std::string_view(line).substr(2,separator-2)),false);
        const auto adjustmentSeparator=line.find(' ',separator+1);
        auto construction=tiger::parseLexiconRows(decode(std::string_view(line).substr(separator+1,
            adjustmentSeparator==line.npos?line.npos:adjustmentSeparator-separator-1)),false);
        const auto adjustmentText=adjustmentSeparator==line.npos?std::u16string{}:decode(std::string_view(line).substr(adjustmentSeparator+1));
        const auto ordinaryAdjustments=tiger::parseLexiconRows(adjustmentText,false);
        rows.coded.insert(rows.coded.end(),ordinaryAdjustments.coded.begin(),ordinaryAdjustments.coded.end());
        rows.uncoded.insert(rows.uncoded.end(),ordinaryAdjustments.uncoded.begin(),ordinaryAdjustments.uncoded.end());
        imported=tiger::buildImportedLexicon(std::move(rows),line[0]=='1'?&construction:nullptr,adjustmentText);
        }
        std::cout<<"{\"main\":[";
        for(std::size_t i=0;i<imported.main.size();++i) {
            if(i) std::cout<<',';const auto& entry=imported.main[i];std::cout<<'[';json(entry.code);std::cout<<",[";
            for(std::size_t j=0;j<entry.candidates.size();++j) {if(j) std::cout<<',';json(entry.candidates[j]);}
            std::cout<<"]]";
        }
        std::cout<<"],\"construct\":";lookup(imported.construct);std::cout<<",\"full\":";lookup(imported.fullCodes);
        std::cout<<",\"indexed\":[";
        for(std::size_t i=0;i<imported.indexedMain.size();++i) {
            if(i) std::cout<<',';const auto& entry=imported.indexedMain[i];std::cout<<'[';json(entry.code);std::cout<<','<<entry.flags<<",[";
            if(entry.sourceIndex<imported.main.size()) {
                const auto& candidates=imported.main[entry.sourceIndex].candidates;
                for(std::size_t j=0;j<candidates.size();++j) {if(j) std::cout<<',';json(candidates[j]);}
            }
            std::cout<<"]]";
        }
        std::cout<<"],\"quick\":"<<imported.quickSymbols;
        if(all) {
            std::cout<<",\"comments\":";lookup(imported.comments);std::cout<<",\"splits\":";lookup(imported.splits);
            std::cout<<",\"pinyin\":[";
            for(std::size_t i=0;i<imported.pinyin.size();++i) {
                if(i) std::cout<<',';auto& row=imported.pinyin[i];std::cout<<'[';json(row.code);std::cout<<",[";
                for(std::size_t j=0;j<row.candidates.size();++j) {if(j) std::cout<<',';json(row.candidates[j]);}
                std::cout<<"]]";
            }
            std::cout<<']';
        }
        std::cout<<"}\n";
    }
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
