#define NOMINMAX
#include <windows.h>
#include "ConfigStore.h"
#include "Settings.h"
#include "Text.h"
#include <iostream>
int main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4) return 2;
        auto path=std::filesystem::u8path(argv[1]);
        const int count=std::stoi(argv[2]);
        if(argc==4 && std::string(argv[3])=="cycle") {
            bool horizontal=false,vertical=false;
            tiger::updateConfigurationValues(path,{{u"竖排候选",u"否"},{u"隐藏候选",u"否"},{u"候选窗显示编码",u"是"}});
            auto style=tiger::cycleCandidateMode(path,horizontal,vertical);
            if(!style.vertical || style.hideCandidates || style.showCode)throw std::runtime_error("Horizontal to vertical differs");
            tiger::updateConfigurationValues(path,{{u"候选窗显示编码",u"是"}});
            style=tiger::cycleCandidateMode(path,horizontal,vertical);
            if(!style.hideCandidates || !style.showCode)throw std::runtime_error("Vertical to code-only differs");
            style=tiger::cycleCandidateMode(path,horizontal,vertical);
            if(style.vertical || style.hideCandidates || !style.showCode)throw std::runtime_error("Horizontal show-code preference lost");
            style=tiger::cycleCandidateMode(path,horizontal,vertical);
            if(!style.vertical || style.hideCandidates || !style.showCode)throw std::runtime_error("Vertical show-code preference lost");
            std::cout<<"cycle preferences passed\n";return 0;
        }
        if(argc==4 && std::string(argv[3])=="wheel") {
            std::cout<<tiger::adjustCandidateFontSize(path,count)<<'\n';return 0;
        }
        if(argc==4 && (std::string(argv[3])=="save-settings" || std::string(argv[3])=="save-paired" || std::string(argv[3])=="save-english")) {
            for(int i=0;i<count;++i) {
                std::u16string request;
                const bool paired=std::string(argv[3])=="save-paired";
                const bool english=std::string(argv[3])=="save-english";
                const auto stamp=tiger::utf16(std::to_string(GetCurrentProcessId())+"-"+std::to_string(i));
                tiger::saveInputConfiguration(path,{{paired?u"_reload_fixture":english?u"默认中文":u"主题",paired?stamp:english?u"否":u"海蓝"}},
                    [&](std::u16string_view text){request=tiger::configurationValue(text,tiger::inputSettingsReloadKey);});
                std::cout<<tiger::utf8(request);
                if(paired)std::cout<<' '<<tiger::utf8(stamp);
                std::cout<<'\n';
            }
            return 0;
        }
        if(argc==4 && std::string(argv[3])=="reload-read") {
            for(int i=0;i<count;++i) {
                const auto text=tiger::readConfiguration(path);
                std::cout<<tiger::utf8(tiger::configurationValue(text,tiger::inputSettingsReloadKey))<<' '
                    <<tiger::utf8(tiger::configurationValue(text,u"_reload_fixture"))<<'\n';
            }
            return 0;
        }
        if(argc==4 && std::string(argv[3])=="save-noop") {
            tiger::saveInputConfiguration(path,{});std::cout<<"noop\n";return 0;
        }
        if(argc==4 && std::string(argv[3])=="save-invalid") {
            bool rejected=false;
            try {tiger::saveInputConfiguration(path,{{u"主题",u"清晨"}},[](std::u16string_view){throw std::runtime_error("Invalid fixture");});}
            catch(const std::runtime_error&) {rejected=true;}
            if(!rejected)throw std::runtime_error("Invalid save was accepted");
            std::cout<<"rejected\n";return 0;
        }
        if(argc==4 && std::string(argv[3]).rfind("select=",0)==0) {
            for(int i=0;i<count;++i) tiger::selectSchemaConfiguration(path,tiger::utf16(std::string(argv[3]).substr(7)));
            std::cout<<tiger::utf8(tiger::readConfiguration(path)); return 0;
        }
        if(argc==4 && std::string(argv[3])=="schema-read-loop") {
            for(int i=0;i<count;++i) {
                const auto text=tiger::readConfiguration(path);
                const auto current=tiger::currentSchemaSetting(text);
                const auto recent=tiger::configurationValue(text,u"最近码表对");
                if(current.empty() || recent.substr(0,recent.find(u'|'))!=current ||
                   text.find(u"# retained")==text.npos || text.find(u"主题 海蓝")==text.npos)
                    throw std::runtime_error("Reader saw mismatched schema/current metadata");
            }
            std::cout<<"read\n";return 0;
        }
        if(argc==4 && (std::string(argv[3])=="read-loop" || std::string(argv[3])=="focus-loop")) {
            for(int i=0;i<count;++i) {
                auto text=std::string(argv[3])=="focus-loop"?tiger::readConfiguration(path):tiger::readUnicodeFile(path);
                if(text.find(u"# retained")==text.npos || text.find(u"主题 海蓝")==text.npos) throw std::runtime_error("Reader saw partial configuration");
            }
            std::cout<<"read\n"; return 0;
        }
        if(argc==4 && (std::string(argv[3])=="deny-replace" || std::string(argv[3])=="deny-select" || std::string(argv[3])=="deny-save")) {
            HANDLE block=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(block==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot hold config open");
            bool failed=false;
            try {
                if(std::string(argv[3])=="deny-select") tiger::selectSchemaConfiguration(path,u"Blocked");
                else if(std::string(argv[3])=="deny-save")tiger::saveInputConfiguration(path,{{u"主题",u"清晨"}});
                else tiger::toggleHiddenCandidates(path);
            } catch(...) { failed=true; }
            CloseHandle(block);
            if(!failed) throw std::runtime_error("Expected replacement failure");
            std::cout<<"blocked\n"; return 0;
        }
        for(int i=0;i<count;++i) std::cout<<tiger::toggleHiddenCandidates(path)<<'\n';
        if(!count) std::cout<<tiger::parseCandidateStyle(tiger::readUnicodeFile(path)).hideCandidates<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
