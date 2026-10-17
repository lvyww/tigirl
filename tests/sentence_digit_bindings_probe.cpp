#include "../native/Engine.h"
#include "../native/SelectionKeys.h"
#include "../native/LexiconSerialize.h"
#include "../native/SentenceFivegram.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;

static int checks=0;
static void check(bool value,const char* message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
static Config bindings(std::u16string_view text) {
    SelectionKeys keys;std::u16string error;
    check(SelectionKeys::parse(text,keys,error),"parse selection bindings");
    Config config;config.selection=keys.dispatch();return config;
}
struct Harness {
    Engine engine;
    SentenceDecoder decoder;
    SentencePathQueries queries;
    Harness(std::shared_ptr<const Dictionary> dictionary,Config config,bool automatic=false,int retained=0,
        std::shared_ptr<const SentenceLanguageModel> model={})
        :engine(dictionary,config),decoder(std::make_shared<SentenceLexicon>(dictionary),std::move(model)) {
        engine.enableSentenceInput(true,1,automatic,retained);
        queries.complete=[this](auto raw,auto prefix,auto excluded,bool grouped,const SentenceLockedPrefix* locked) {
            return decoder.hasCompleteCandidate(raw,prefix,excluded,grouped,locked);
        };
        queries.properPrefix=[this](auto raw){return decoder.isProperCodePrefix(raw);};
    }
    void settle() {
        if(!engine.sentenceDecodePending())return;
        if(auto ticket=engine.sentenceRequest())
            check(engine.applySentenceResult(*ticket,decoder.decode(ticket->raw,20,true,ticket->requiredPrefix,ticket->lockedPrefix)),"publish real decode");
    }
    KeyResult tap(int vk,bool settleAfter=true) {
        KeyEvent key;key.vk=vk;
        // TSF previews a copy before actual dispatch, and may retry after decoding.
        auto preview=engine;preview.process(key,queries);
        auto result=engine.process(key,queries);
        if(result.awaitSentenceDecode){settle();result=engine.process(key,queries);}
        check(!result.awaitSentenceDecode,"retry resolves decode wait");
        const auto raw=engine.snapshot().raw;
        key.down=false;auto up=engine.process(key,queries);
        check(up.commit.empty() && engine.snapshot().raw==raw,"keyup must not append or commit");
        if(settleAfter)settle();
        return result;
    }
};

int main(int argc,char** argv) {
    try {
        // Optional read-only real-data check: sentence table, Q8 model, bindings.
        if(argc==4) {
            auto dictionary=Dictionary::Open(std::filesystem::u8path(argv[1]));
            auto model=SentenceFivegram::Open(std::filesystem::u8path(argv[2]));
            Config config;config.selection=SelectionKeys::load(std::filesystem::u8path(argv[3])).dispatch();
            check(config.selection['9']==2,"real configuration binds digit 9 to rank 2");
            for(int vk:{'2','9'}) {
                Harness h(dictionary,config,false,0,model);h.tap('T');h.tap(vk);
                check(h.engine.snapshot().raw==u"t2","real raw code is t2");
                check(!h.engine.snapshot().candidates.empty() && h.engine.snapshot().candidates.front().commit==u"我们","real candidate is expected table rank");
                check(h.tap(32).commit==u"我们","real space commits expected text");
            }
            std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"real_table\":true,\"q8_model\":true,\"physical_input_tested\":false}\n";
            return 0;
        }
        if(argc!=2)return 2;
        auto path=std::filesystem::u8path(argv[1]);
        check(!std::filesystem::exists(path),"fixture must use a new path");
        auto bytes=serializeImportedLexicon(prepareSentenceLexicon({
            {u"t",{u"我",u"我们",u"他们",u"四",u"五",u"六",u"七",u"八",u"九",u"十"}},
            {u"a",{u"好"}},{u"aa",{u"好"}},{u"b",{u"人"}}
        }));
        {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(out),"write fixture");}
        auto dictionary=Dictionary::Open(path);
        const auto custom=bindings(u"2选 0x32 0x39 0x69\n3选 0x33 0x38\n");
        check(custom.selection['9']==2 && custom.selection['8']==3,"lower rank wins default conflicts");
        for(auto pair:{std::pair<int,char16_t>{'2',u'2'},{'9',u'2'},{105,u'2'},{'3',u'3'},{'8',u'3'}}) {
            Harness h(dictionary,custom);h.tap('T');h.tap(pair.first);
            const std::u16string raw=std::u16string(u"t")+pair.second;
            check(h.engine.snapshot().raw==raw,"custom digit must normalize raw code");
            // Check decoded candidate and final commit independently of alias parity.
            const auto expected=pair.second==u'2'?u"我们":u"他们";
            check(h.engine.snapshot().candidates.front().commit==expected,"custom digit selects table rank");
            check(h.tap(32).commit==expected,"space commits selected table text");
        }
        {
            Harness h(dictionary,custom);h.tap('T');h.tap('9');h.tap(8);
            check(h.engine.snapshot().raw==u"t","backspace removes canonical suffix");
            h.tap('9');check(h.tap(13).commit==u"t2","enter commits canonical code");
        }
        {
            Harness h(dictionary,custom);h.tap('T');h.tap('9');
            check(h.engine.refreshConfiguration(Config{}),"reload defaults during composition");
            check(h.engine.snapshot().raw==u"t2","reload preserves canonical buffer");
            h.tap(8);h.tap('9');check(h.engine.snapshot().raw==u"t9","reload takes effect on next key");
            h.engine.refreshConfiguration(custom);h.tap(8);h.tap('9');
            check(h.tap(32).commit==u"我们","restored binding works immediately");
        }
        {
            Harness h(dictionary,bindings(u"2选 0x39\n3选 0x32\n"));h.tap('T');h.tap('9');
            check(h.engine.snapshot().raw==u"t2","no chained remapping 9 to 2 to 3");
            h.tap('2');check(h.engine.snapshot().raw==u"t23","consecutive aliases map individually");
        }
        for(int vk:{'0','9'}) {
            Harness h(dictionary,bindings(u"9选\n10选 0x30 0x39\n"));h.tap('T');h.tap(vk);
            check(h.engine.snapshot().raw==u"t0" && h.tap(32).commit==u"十","tenth selection normalizes to zero");
        }
        for(int vk:std::initializer_list<int>{'9',105}) {
            Harness h(dictionary,bindings(u"9选\n"));h.tap('T');h.tap(vk);
            check(h.engine.snapshot().raw==u"t9","unbound digits retain original rank");
        }
        {
            Harness h(dictionary,bindings(u"2选 0x32 0x39\n"));h.tap('T');h.tap(105);
            check(h.engine.snapshot().raw==u"t9","numpad does not inherit top row alias");
        }
        // Modifier, idle, NumLock and punctuation boundaries match default routing.
        for(int mode=0;mode<8;++mode) {
            Harness customCase(dictionary,custom),defaultCase(dictionary,Config{});
            if(mode!=4){customCase.tap('T');defaultCase.tap('T');}
            KeyEvent key;key.vk='9';
            if(mode==0)key.shift=true;if(mode==1)key.ctrl=true;if(mode==2)key.alt=true;if(mode==3)key.win=true;
            if(mode==5){key.vk=33;key.num=false;} // NumLock-off numpad 9 arrives as PageUp.
            if(mode==6)key.vk=186;if(mode==7)key.vk=222;
            auto a=customCase.engine.process(key),b=defaultCase.engine.process(key);
            check(a.handled==b.handled && a.commit==b.commit && a.cancelComposition==b.cancelComposition &&
                customCase.engine.snapshot().raw==defaultCase.engine.snapshot().raw,"unchanged non-digit or modified routing");
        }
        // Paired traces cover deferred results, Tab locks, continuation, deletion,
        // early commit, and learning; only the physical alias differs.
        for(bool automatic:{false,true})for(int retained:{0,3})for(bool deferred:{false,true}) {
            Harness alias(dictionary,custom,automatic,retained),canonical(dictionary,Config{},automatic,retained);
            for(int vk:std::initializer_list<int>{'T','9',9,'A','A','B',8,8,'T','8',32,'T','9',27}) {
                auto a=alias.tap(vk,!deferred),b=canonical.tap(vk=='9'?'2':vk=='8'?'3':vk,!deferred);
                auto x=alias.engine.snapshot(),y=canonical.engine.snapshot();
                check(a.commit==b.commit && a.handled==b.handled && x.raw==y.raw && x.surface==y.surface &&
                    x.mode==y.mode && x.selectedCandidate==y.selectedCandidate && x.candidates.size()==y.candidates.size(),"alias continuation matches canonical trace");
                for(std::size_t i=0;i<x.candidates.size();++i)
                    check(x.candidates[i].commit==y.candidates[i].commit,"continuation candidate parity");
                check(a.learning.size()==b.learning.size(),"learning event count parity");
                for(std::size_t i=0;i<a.learning.size();++i) {
                    const auto& l=a.learning[i];const auto& r=b.learning[i];
                    check(l.code==r.code && l.text==r.text && l.context==r.context && l.mode==r.mode &&
                        l.rawStart==r.rawStart && l.rawEnd==r.rawEnd && l.textStart==r.textStart &&
                        l.textEnd==r.textEnd && l.levels==r.levels,"canonical learning source parity");
                }
                auto p=alias.engine.sentenceRequest(),q=canonical.engine.sentenceRequest();
                check(bool(p)==bool(q) && (!p || (p->raw==q->raw && p->requiredPrefix==q->requiredPrefix)),"full raw and committed prefix parity");
            }
        }
        {
            // Ordinary tables keep lowercase keys; sentence preparation uses
            // ordinal folded keys and must not stand in for an ordinary table.
            ImportedLexicon ordinary;ordinary.main={{u"t",{u"我",u"我们",u"他们"}}};
            ordinary.indexedMain={{u"t",8,0}};
            auto ordinaryPath=path;ordinaryPath+=".ordinary";
            auto ordinaryBytes=serializeImportedLexicon(ordinary);
            {std::ofstream out(ordinaryPath,std::ios::binary);out.write(reinterpret_cast<const char*>(ordinaryBytes.data()),ordinaryBytes.size());check(bool(out),"write ordinary fixture");}
            Harness h(dictionary,custom);h.engine.enableSentenceInput(false,2);
            h.engine.switchSchema(std::make_shared<Lexicon>(Dictionary::Open(ordinaryPath)),custom);h.tap('T');
            check(h.tap('9').commit==u"我们","ordinary mode selection unchanged");
            h.engine.switchSchema(std::make_shared<Lexicon>(dictionary),custom);
            h.engine.enableSentenceInput(true,3);h.tap('T');h.tap('9');
            check(h.tap(32).commit==u"我们","switch back to sentence mode");
        }
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"physical_input_tested\":false}\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
