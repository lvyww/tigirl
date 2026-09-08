#include "Engine.h"
#include "Settings.h"
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* message) {if(!v) throw std::runtime_error(message);}
int main(int argc,char** argv) {
 try {
    require(argc==2,"schema_shortcut_probe <dictionary>");
    auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
    auto config=tiger::parseEngineSettings(u"Ctrl+m切换最近码表 是\n");
    require(config.recentSchemaEnabled,"Recent shortcut not enabled");
    tiger::Engine engine(dictionary,config);
    for(int vk:{65,66}) {tiger::KeyEvent k;k.vk=vk;engine.process(k);k.down=false;engine.process(k);}
    auto before=engine.snapshot();auto preview=engine;
    tiger::KeyEvent chord;chord.vk=77;chord.ctrl=true;chord.recentSchemaAvailable=true;
    require(preview.process(chord).switchRecentSchema,"Preview omitted switch request");
    require(engine.snapshot().raw==before.raw,"Preview changed original composition");
    auto result=engine.process(chord);
    require(result.handled && result.switchRecentSchema && engine.snapshot().raw==u"ab","Switch request lost composition");
    engine.switchSchema(engine.lexicon(),config);
    require(!engine.process(chord).switchRecentSchema,"Auto-repeat switched twice");
    chord.down=false;engine.process(chord);chord.down=true;
    result=engine.process(chord);require(result.handled && !result.switchRecentSchema,"Repeated press under held Ctrl switched again");
    auto add=chord;add.vk=187;
    result=engine.process(add);require(result.handled && !result.openAddWord,"Schema chord rollover opened add-word");
    tiger::KeyEvent release;release.vk=17;release.down=false;engine.process(release);
    require(engine.process(add).openAddWord,"Modifier release did not rearm add-word");
    engine.focusChanged();require(engine.process(chord).switchRecentSchema,"Focus recovery did not rearm schema shortcut");
    engine.focusChanged();chord.recentSchemaAvailable=false;
    result=engine.process(chord);require(!result.handled && !result.switchRecentSchema,"Single-schema shortcut was swallowed");
    require(engine.snapshot().raw.empty(),"Unavailable chord did not follow normal composition cancellation");
    const auto collision=tiger::parseEngineSettings(u"Ctrl+m切换最近码表 是\n手动加词快捷键 Ctrl+VK_M\n");
    require(!collision.recentSchemaEnabled && !collision.addWordEnabled,"Conflicting action shortcuts were not both disabled");
    const auto reserved=tiger::parseEngineSettings(u"Ctrl+m切换最近码表 是\n切换最近码表快捷键 Ctrl+VK_SPACE\n");
    require(!reserved.recentSchemaEnabled,"Reserved shortcut was overridden");
    const auto custom=tiger::parseEngineSettings(u"Ctrl+m切换最近码表 是\n切换最近码表快捷键 Alt+Shift+VK_J\n");
    require(custom.recentSchemaEnabled && custom.recentSchemaShortcut.matches(74,true,false,true,false),"Custom recent shortcut failed");
    std::cout<<"{\"status\":\"passed\",\"preview_isolation\":true,\"repeat_and_rollover\":true,\"unavailable_passthrough\":true,\"conflict_rules\":true}\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
