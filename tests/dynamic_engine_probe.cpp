#include "Engine.h"
#include <iostream>
#include <stdexcept>
using namespace tiger;
static void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static void type(Engine& engine,std::u16string_view code) {
    for(auto c:code) {
        KeyEvent key; key.vk=c-u'a'+'A';
        require(engine.process(key).commit.empty(),"Unexpected auto commit");
        key.down=false; engine.process(key);
    }
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"dynamic_engine_probe <dictionary>");
        auto dictionary=Dictionary::Open(std::filesystem::u8path(argv[1]));
        const std::u16string code=u"zzzzzz", repeatCode=u"zzzzzx";
        require(!dictionary->find(Section::Main,code).count,"Test code already exists");
        Config config; config.maxCodeLength=16; config.maxCodeAutoCommit=false;
        unsigned cases=0;
        {
            auto base=std::make_shared<const Lexicon>(dictionary);
            auto lexicon=base->changed({ChangeKind::Add,code,u"a"});
            lexicon=lexicon->changed({ChangeKind::Add,repeatCode,u"\u0301"});
            Engine history(lexicon,config);
            type(history,code); history.select(0);
            type(history,repeatCode); history.select(0);
            require(history.recentElements()==std::vector<std::u16string>{u"a",u"\u0301"},"Separate commits merged history graphemes");
            KeyEvent back; back.vk=8;
            auto preview=history; preview.process(back);
            require(history.recentElements().size()==2,"History preview changed live engine");
            history.process(back);
            require(history.recentText()==u"a","Backspace crossed commit boundary");
            for(auto word:{u"😀",u"a\u0301",u"👩🏽‍💻",u"🇨🇳"}) {
                auto words=base->changed({ChangeKind::Add,code,word});
                Engine unicode(words,config); type(unicode,code); unicode.select(0);
                require(unicode.recentElements().size()==1,"Unicode commit split history element");
                unicode.process(back); require(unicode.recentText().empty(),"Backspace left part of Unicode grapheme");
            }
        }
        for(auto token:{u"{日期}",u"{日期.}",u"{日期-}",u"{日期/}",u"{时分秒}",u"{时分}",u"{星期}",u"{周}"}) {
            for(bool alias:{false,true}) for(bool mouse:{false,true}) {
                auto base=std::make_shared<const Lexicon>(dictionary);
                auto lexicon=base->changed({ChangeKind::Add,code,(alias?std::u16string(u"日期别名\x1e"):std::u16string{})+token});
                lexicon=lexicon->changed({ChangeKind::Add,repeatCode,u"{重复上屏}"});
                Engine engine(lexicon,config); type(engine,code);
                std::minstd_rand random(1);
                const auto before=expandDynamicText(token,random);
                const auto candidate=engine.candidateAt(0);
                KeyEvent space; space.vk=32;
                const auto commit=mouse?engine.select(0):engine.process(space);
                const auto after=expandDynamicText(token,random);
                require(candidate.display==(alias?u"日期别名":before) || (!alias && candidate.display==after),"Dynamic candidate display");
                require(commit.commit==before || commit.commit==after,"Dynamic commit");
                require(!engine.composing(),"Commit did not end composition");
                type(engine,repeatCode);
                require(engine.process(space).commit==commit.commit,"Repeat did not retain dynamic commit");
                ++cases;
            }
        }
        for(bool mouse:{false,true}) {
            auto lexicon=std::make_shared<const Lexicon>(dictionary)->changed({ChangeKind::Add,code,u"{||{日期}||}"});
            Engine engine(lexicon,config); type(engine,code);
            std::minstd_rand random(1); const auto before=expandDynamicText(u"{日期}",random);
            KeyEvent space; space.vk=32;
            auto result=mouse?engine.select(0):engine.process(space);
            require(result.commit==before || result.commit==expandDynamicText(u"{日期}",random),"Nested token not normalized");
            ++cases;
        }
        auto lexicon=std::make_shared<const Lexicon>(dictionary)->changed({ChangeKind::Add,code,u"{甲|乙|丙|丁}"});
        Engine engine(lexicon,config),control=engine;
        for(int i=0;i<64;++i) {
            type(engine,code); type(control,code);
            auto preview=engine;
            for(int j=0;j<7;++j) preview.snapshot();
            KeyEvent space; space.vk=32; preview.process(space);
            require(engine.process(space).commit==control.process(space).commit,"Preview advanced real engine random state");
        }
        auto selectionLexicon=std::make_shared<const Lexicon>(dictionary)->changed({ChangeKind::Add,code,u"一"});
        selectionLexicon=selectionLexicon->changed({ChangeKind::Add,code,u"二"});
        selectionLexicon=selectionLexicon->changed({ChangeKind::Add,code,u"三"});
        for(int vk:{32,49,50,51,186,222,16}) {
            auto selectionConfig=config; selectionConfig.selection[16]=1;
            Engine selected(selectionLexicon,selectionConfig); type(selected,code);
            KeyEvent key; key.vk=vk;
            auto result=selected.process(key);
            require(!result.commit.empty() && selected.recentText()==result.commit,"Selection recorded history more than once");
        }
        for(auto token:{u"{添加}",u"{加词}",u"{||{添加}||}"}) for(bool mouse:{false,true}) {
            auto actionLexicon=std::make_shared<const Lexicon>(dictionary)->changed({ChangeKind::Add,code,token});
            Engine action(actionLexicon,config); type(action,code);
            KeyEvent key; key.vk=32;
            auto preview=action; auto predicted=preview.process(key);
            require(predicted.openAddWord && action.composing(),"Action preview changed actual composition");
            auto result=mouse?action.select(0):action.process(key);
            require(result.openAddWord && result.commit.empty() && action.recentText().empty() && !action.composing(),"Add-word action leaked literal text or history");
        }
        for(auto token:{u"{隐藏候选}",u"{||{隐藏候选}||}",u"显示\x1e{隐藏候选}"}) for(bool mouse:{false,true}) {
            auto actionLexicon=std::make_shared<const Lexicon>(dictionary)->changed({ChangeKind::Add,code,token});
            Engine action(actionLexicon,config); type(action,code);
            KeyEvent key; key.vk=32;
            auto preview=action; auto predicted=preview.process(key);
            require(predicted.toggleHiddenCandidates && action.composing(),"Hide preview changed actual composition");
            auto result=mouse?action.select(0):action.process(key);
            require(result.toggleHiddenCandidates && !result.openAddWord && result.commit.empty() && action.recentText().empty() && !action.composing(),"Hide action leaked text or history");
        }
        std::cout<<"{\"status\":\"passed\",\"commit_cases\":"<<cases<<",\"preview_cycles\":64,\"selection_history_cases\":7,\"add_word_action_cases\":6,\"hide_action_cases\":6}\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
