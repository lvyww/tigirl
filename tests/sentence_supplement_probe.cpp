#include "../native/SentenceSupplement.h"
#include "../native/MappedSentenceSupplement.h"
#include "../native/LexiconSerialize.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
std::u16string token(const std::string& h){if(h=="-")return {};std::u16string t;for(std::size_t i=0;i<h.size();i+=4)t+=static_cast<char16_t>(std::stoul(h.substr(i,4),nullptr,16));return t;}
int wmain(int argc,wchar_t** argv){
 try {
    if(argc!=3)return 2;int fixture=0;std::ifstream input(argv[1]);std::string line;
    std::vector<tiger::SentenceSupplementEntry> entries;std::vector<std::pair<int,std::u16string>> queries;
    std::cout<<std::setprecision(17);
    while(std::getline(input,line)){
        std::istringstream row(line);std::string type,text;row>>type;
        if(type=="B"){entries.clear();queries.clear();}
        if(type=="E"){std::int64_t weight;row>>text>>weight;entries.push_back(tiger::SentenceSupplementEntry::create(token(text),weight));}
        if(type=="Q"){int state;row>>state>>text;queries.emplace_back(state,token(text));}
        if(type=="X"){
            tiger::SentenceSupplementMatcher builder(entries);
            const auto path=std::filesystem::path(argv[2])/(std::to_string(fixture++)+".tcs");
            const auto bytes=tiger::serializeImportedLexicon(builder.serializeGraph());
            {std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!output)throw std::runtime_error("Cannot save graph");}
            auto mapping=tiger::Dictionary::Open(path);tiger::MappedSentenceSupplement matcher(mapping);
            if(mapping!=tiger::Dictionary::Open(path))throw std::runtime_error("Graph mapping not reused");
            if(fixture==1) {
                int badIndex=0;
                auto reject=[&](tiger::ImportedLexicon graph) {
                    const auto corrupt=std::filesystem::path(argv[2])/("bad"+std::to_string(badIndex++)+".tcs");
                    const auto data=tiger::serializeImportedLexicon(graph);
                    {std::ofstream out(corrupt,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());}
                    bool rejected=false;try {tiger::MappedSentenceSupplement invalid(tiger::Dictionary::Open(corrupt));}catch(const std::exception&){rejected=true;}
                    if(!rejected)throw std::runtime_error("Corrupt supplement graph accepted");
                };
                auto graph=builder.serializeGraph();graph.splits.clear();reject(graph);
                graph=builder.serializeGraph();graph.main[0].candidates[0].replace(16,16,u"7ff0000000000000");reject(graph);
                graph=builder.serializeGraph();graph.main[1].candidates[0].replace(0,8,u"00000001");reject(graph);
                graph=builder.serializeGraph();graph.pinyin[0].candidates[0]=u"ffffffff";reject(graph);
                graph=builder.serializeGraph();graph.pinyin[0].candidates[0]=u"00000000";reject(graph);
            }
            int state=0;
            for(const auto& q:queries){double reward;state=matcher.advance(q.first==-2?state:q.first,q.second,reward);std::cout<<state<<' '<<reward<<'\n';}
        }
    }return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
