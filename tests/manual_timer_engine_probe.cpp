#include "Engine.h"
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"manual_timer_engine_probe <dictionary>");
        auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
        struct Case { const char16_t* code; int vk,delay; const char16_t* commit; };
        const Case cases[]={
            {u"Ds1",32,60000,u""},{u"DS2",13,120000,u""},{u"Ds0",32,-1,u""},
            {u"Ds12",190,720000,u"."},{u"Ds1",188,60000,u","},
            {u"Ds1",27,-1,u""},{u"Ds1",9,-1,u""},{u"Ds1",20,-1,u"Ds1"},
            {u"Ds99999999999",32,2147483647,u""},{u"Dq1",32,-1,u"Dq1"}
        };
        for(const auto& item:cases) {
            tiger::Engine engine(dictionary);
            for(auto c:std::u16string_view(item.code)) {
                tiger::KeyEvent key;key.shift=c>=u'A' && c<=u'Z';
                key.vk=c>=u'a' && c<=u'z'?c-32:c;
                require(engine.process(key).manualTimerMs<0,"Typing started timer before commit");
                key.down=false;engine.process(key);
            }
            tiger::KeyEvent key;key.vk=item.vk;
            const auto before=engine.snapshot().raw; auto preview=engine;
            auto tested=preview.process(key);
            require(engine.snapshot().raw==before && engine.recentText().empty(),"Preview changed live timer composition");
            auto actual=engine.process(key);
            require(actual.manualTimerMs==item.delay && actual.commit==item.commit,"Timer commit/action mismatch");
            require(tested.manualTimerMs==actual.manualTimerMs && tested.commit==actual.commit,"Timer preview/dispatch mismatch");
            key.down=false;require(engine.process(key).manualTimerMs<0,"Key release scheduled timer again");
        }
        std::cout<<"{\"status\":\"passed\",\"cases\":10,\"preview_isolation\":true}\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
