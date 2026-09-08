#include "Engine.h"
#include "Settings.h"
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
static tiger::KeyResult tap(tiger::Engine& engine,int vk) {
    tiger::KeyEvent key;key.vk=vk;auto result=engine.process(key);key.down=false;engine.process(key);return result;
}
int main(int argc,char** argv) {
 try {
    require(argc==2,"configuration_reload_probe <dictionary>");
    auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
    auto config=tiger::parseEngineSettings(u"默认中文 是\n每页候选个数 1\n");
    tiger::Engine engine(dictionary,config);
    tap(engine,'A');tap(engine,'B');tap(engine,32);
    const auto history=engine.recentText();require(!history.empty(),"History fixture empty");
    tap(engine,'A');tap(engine,'B');engine.setPage(1);
    config.defaultChinese=false;
    engine.refreshConfiguration(config);
    require(engine.chinese() && engine.snapshot().raw==u"ab","Ordinary refresh reset mode or code");
    config.reloadRequest=u"request-1";
    auto preview=engine;preview.refreshConfiguration(config);
    require(engine.chinese() && engine.snapshot().raw==u"ab","Preview reset original engine");
    require(!preview.chinese() && preview.snapshot().raw.empty() && preview.recentText()==history,"Reload lost history or failed to cancel");
    engine=preview;engine.setChinese(true);tap(engine,'A');tap(engine,'B');engine.setPage(1);
    require(!engine.refreshConfiguration(config) && engine.chinese() && engine.snapshot().raw==u"ab","Duplicate request reset fresh input");
    config.pageSize=2;engine.refreshConfiguration(config);
    require(engine.chinese() && engine.snapshot().raw==u"ab","Changed settings replayed old request");
    config.reloadRequest.clear();engine.refreshConfiguration(config);
    config.reloadRequest=u"request-1";engine.refreshConfiguration(config);
    require(engine.snapshot().raw==u"ab","Restored request replayed cancellation");
    config.reloadRequest=u"request-2";
    engine.switchSchema(engine.lexicon(),config);
    require(!engine.chinese() && engine.snapshot().raw.empty(),"Schema switch restored code after explicit reload");
    config.defaultChinese=true;config.reloadRequest=u"request-3";engine.refreshConfiguration(config);
    tap(engine,'1');
    auto armed=engine;require(tap(armed,190).commit==u".","Digit punctuation fixture unarmed");
    config.reloadRequest=u"request-4";engine.refreshConfiguration(config);
    require(tap(engine,190).commit==u"。","Reload retained digit punctuation state");
    tiger::KeyEvent shift;shift.vk=160;engine.process(shift);
    config.reloadRequest=u"request-5";engine.refreshConfiguration(config);
    shift.down=false;engine.process(shift);require(!engine.chinese(),"Reload cleared held Shift unlike original core");
    config.mixedInput=true;config.reloadRequest=u"request-6";engine.refreshConfiguration(config);
    for(int vk:{65,66,65,66,65})tap(engine,vk);
    require(!engine.snapshot().raw.empty(),"Mixed input fixture empty");
    config.reloadRequest=u"request-7";engine.refreshConfiguration(config);
    require(engine.snapshot().raw.empty(),"Reload retained mixed code");
    tiger::Engine fresh(dictionary,config);tap(fresh,'A');fresh.refreshConfiguration(config);
    require(fresh.snapshot().raw==u"a","Fresh engine replayed existing request");
    std::cout<<"{\"status\":\"passed\",\"preview_isolation\":true,\"history_preserved\":true,\"duplicate_request_idempotent\":true,\"schema_reload_cancels\":true,\"digit_reset\":true,\"held_shift_preserved\":true,\"mixed_cancelled\":true,\"fresh_engine_baseline\":true}\n";
 } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
