#include "../native/Engine.h"
#include "../native/LexiconSerialize.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
class PrefersCharacters final:public SentenceLanguageModel {
public:
    double logProbability(std::u16string_view,std::u16string_view,std::u16string_view target,bool) const override {
        return target==u"刍" || target==u"多" || target==u"字" || target==u"词"?20.0:0.0;
    }
    bool hasObservedBigram(std::u16string_view,std::u16string_view) const override{return false;}
};
int wmain(int argc,wchar_t** argv) {
    if(argc!=2)return 2;
    try {
        const std::filesystem::path path=argv[1];
        // Same fixture as upstream 954c82d, through the real decoder and Engine.
        auto data=prepareSentenceLexicon({{u"xr",{u"反"}},{u"xry",{u"反"}},{u"xbj",{u"秉",u"刍",u"多字词"}}});
        auto bytes=serializeImportedLexicon(data);
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"fixture write");}
        auto dictionary=Dictionary::Open(path);
        auto lexicon=std::make_shared<SentenceLexicon>(dictionary);
        int cases=0;
        for(auto prefix:{u"xr",u"xry"})for(bool automatic:{false,true})for(bool duplicates:{false,true}) {
            SentenceDecoderOptions options;options.beamWidth=100;options.allowDuplicateSingleCharacters=duplicates;
            SentenceDecoder decoder(lexicon,std::make_shared<PrefersCharacters>(),options);
            Engine engine(dictionary);engine.enableSentenceInput(true,1,automatic,0);
            SentencePathQueries queries;
            queries.complete=[&](auto raw,auto required,auto excluded,bool grouped,const SentenceLockedPrefix* locked){return decoder.hasCompleteCandidate(raw,required,excluded,grouped,locked);};
            queries.properPrefix=[&](auto raw){return decoder.isProperCodePrefix(raw);};
            auto tap=[&](int vk) {
                KeyEvent key;key.vk=vk;
                auto result=engine.process(key,queries);key.down=false;engine.process(key,queries);
                if(auto ticket=engine.sentenceRequest()) {
                    engine.applySentenceResult(*ticket,decoder.decode(ticket->raw,20,true,ticket->requiredPrefix,ticket->lockedPrefix));
                    result.commit+=engine.autoCommitSentence().commit;
                }
                return result.commit;
            };
            std::u16string output;
            for(auto c:std::u16string(prefix))output+=tap(c-u'a'+'A');
            output+=tap('X');
            require(output==(automatic?u"反":u""),"empty-code prefix commit differs");
            output+=tap('B');output+=tap('J');
            const auto view=engine.snapshot();
            require(!view.candidates.empty(),"missing continuation candidate");
            const std::u16string expected=duplicates?u"反刍":u"反秉";
            require(output+view.candidates[0].commit==expected,"rumination first candidate differs");
            for(const auto& candidate:view.candidates)require(candidate.commit.find(u"多字词")==std::u16string::npos,"implicit non-first word leaked");
            output+=tap(32);require(output==expected,"rumination space commit differs");
            ++cases;
        }
        std::cout<<"{\"status\":\"passed\",\"cases\":"<<cases<<",\"upstream\":\"954c82d\"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
