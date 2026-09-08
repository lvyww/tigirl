#include "Engine.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
static void json(std::u16string_view text) {
    const char* hex="0123456789abcdef";std::cout<<'"';
    for(auto c:text) std::cout<<"\\u"<<hex[(c>>12)&15]<<hex[(c>>8)&15]<<hex[(c>>4)&15]<<hex[c&15];
    std::cout<<'"';
}
int main(int argc,char** argv) {
 try {
    if(argc!=3) throw std::runtime_error("switch_composition_probe <dictionary> <cases.tsv>");
    auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
    // A distinct immutable lexicon must invalidate decoded-prefix caches,
    // while an existing preview keeps its original dictionary and composition.
    {
        tiger::Config config;config.maxCodeLength=2;config.mixedInput=true;
        tiger::Engine engine(dictionary,config);
        for(char c:std::string("abab")) {
            tiger::KeyEvent key;key.vk=c-32;engine.process(key);
            key.down=false;engine.process(key);
        }
        const auto before=engine.snapshot();auto preview=engine;
        const auto history=engine.recentText();
        auto target=engine.lexicon()->changed({tiger::ChangeKind::Add,u"ab",u"新方案"});
        target=target->changed({tiger::ChangeKind::Top,u"ab",u"新方案"});
        engine.switchSchema(target,config);
        auto switched=engine.snapshot();
        if(switched.raw!=u"abab" || switched.compositionText()!=u"新方案ab" ||
           switched.candidates.empty() || switched.candidates[0].display!=u"新方案" ||
           preview.snapshot().compositionText()!=before.compositionText() ||
           engine.recentText()!=history)
            throw std::runtime_error("Schema replacement/cache/preview preservation failed");
        tiger::KeyEvent space;space.vk=32;
        if(engine.process(space).commit!=u"新方案新方案")
            throw std::runtime_error("Schema replacement did not change mixed commit");
    }
    std::ifstream source(std::filesystem::u8path(argv[2]));
    if(!source) throw std::runtime_error("Cannot read cases");
    for(std::string line;std::getline(source,line);) {
        std::istringstream row(line);bool before,after;int maximum;std::string text;
        if(!(row>>before>>after>>maximum)) throw std::runtime_error("Invalid case");
        row.get();std::getline(row,text);
        tiger::Config config;config.maxCodeLength=16;config.mixedInput=before;
        tiger::Engine engine(dictionary,config);
        for(char c:text) {
            tiger::KeyEvent key;key.shift=c>='A'&&c<='Z';
            key.vk=c=='`'?192:c=='='?187:c>='a'&&c<='z'?c-32:c;
            engine.process(key);key.down=false;engine.process(key);
        }
        config.maxCodeLength=maximum;config.mixedInput=after;
        engine.switchSchema(engine.lexicon(),config);
        auto snapshot=engine.snapshot();tiger::KeyEvent key;key.vk=32;
        auto commit=engine.process(key);
        std::cout<<"{\"raw\":";json(snapshot.raw);std::cout<<",\"surface\":";json(snapshot.compositionText());
        std::cout<<",\"mode\":"<<static_cast<int>(snapshot.mode)<<",\"page\":"<<snapshot.page<<",\"candidates\":[";
        for(std::size_t i=0;i<snapshot.candidates.size();++i) {if(i) std::cout<<',';json(snapshot.candidates[i].display);}
        std::cout<<"],\"commit\":";json(commit.commit);std::cout<<"}\n";
    }
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
