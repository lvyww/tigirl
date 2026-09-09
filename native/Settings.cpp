#include "Settings.h"
#include "Text.h"
#include "SelectionKeys.h"
#include <algorithm>
#include <map>
#include <cstdint>
#include <sstream>
#include <locale>
#include <cmath>

namespace tiger {
namespace {
bool whitespace(char16_t c) {
    return (c>=9 && c<=13) || c==32 || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000;
}
std::u16string_view trim(std::u16string_view s) {
    while(!s.empty() && whitespace(s.front())) s.remove_prefix(1);
    while(!s.empty() && whitespace(s.back())) s.remove_suffix(1);
    return s;
}
bool boolean(std::u16string value,bool fallback) {
    for(auto& c:value) if(c>=u'A' && c<=u'Z') c+=u'a'-u'A';
    if(value==u"是" || value==u"true" || value==u"on" || value==u"1") return true;
    if(value==u"否" || value==u"false" || value==u"off" || value==u"0") return false;
    return fallback;
}
int integer(std::u16string_view value,int fallback,int limit,int minimum=1) {
    bool negative=false;
    if(!value.empty() && (value.front()==u'+' || value.front()==u'-')) { negative=value.front()==u'-'; value.remove_prefix(1); }
    if(value.empty()) return fallback;
    std::int64_t n=0;
    for(auto c:value) {
        if(c<u'0' || c>u'9') return fallback;
        n=n*10+c-u'0';
        if(n>(negative?2147483648ll:2147483647ll)) return fallback;
    }
    return static_cast<int>(std::clamp<std::int64_t>(negative?-n:n,minimum,limit));
}
std::map<std::u16string,std::u16string> settingsValues(std::u16string_view text) {
    std::map<std::u16string,std::u16string> values;
    while(!text.empty()) {
        const auto end=text.find_first_of(u"\r\n"); auto line=text.substr(0,end);
        while(!line.empty() && whitespace(line.front()))line.remove_prefix(1);
        if(end==text.npos) text={}; else text.remove_prefix(end+1);
        if(line.empty() || line.front()==u'#') continue;
        const auto sep=line.find_first_of(u"\t ,");
        if(sep==line.npos || sep==0) continue;
        values[std::u16string(trim(line.substr(0,sep)))]=trim(line.substr(sep+1));
    }
    return values;
}
ActionShortcut shortcut(std::u16string_view text,ActionShortcut fallback) {
    ActionShortcut result; result.vk=0; result.ctrl=result.alt=result.shift=false;
    bool haveKey=false;
    while(!text.empty()) {
        const auto end=text.find(u'+'); auto part=std::u16string(trim(text.substr(0,end)));
        if(part.empty()) return fallback;
        for(auto& c:part) if(c>=u'a' && c<=u'z') c-=u'a'-u'A';
        if(part==u"WIN" || part==u"WINDOWS") return fallback;
        bool* flag=part==u"CTRL" || part==u"CONTROL"?&result.ctrl:part==u"ALT"?&result.alt:part==u"SHIFT"?&result.shift:nullptr;
        if(flag) { if(*flag) return fallback; *flag=true; }
        else {
            if(haveKey) return fallback;
            if(part.rfind(u"0X",0)==0) {
                SelectionKeys keys; std::u16string error;
                if(!SelectionKeys::parse(u"1选 0X"+std::u16string(trim(std::u16string_view(part).substr(2))),keys,error) || keys.bindings[0].size()!=1) return fallback;
                result.vk=keys.bindings[0][0];
            } else {
                static const auto names=[] {
                    std::map<std::u16string,int> map={
                        {u"VK_BACK",8},{u"VK_TAB",9},{u"VK_RETURN",13},{u"VK_ESCAPE",27},{u"VK_SPACE",32},
                        {u"VK_PRIOR",33},{u"VK_NEXT",34},{u"VK_END",35},{u"VK_HOME",36},{u"VK_LEFT",37},
                        {u"VK_UP",38},{u"VK_RIGHT",39},{u"VK_DOWN",40},{u"VK_INSERT",45},{u"VK_DELETE",46},
                        {u"VK_OEM_1",186},{u"VK_OEM_PLUS",187},{u"VK_OEM_COMMA",188},{u"VK_OEM_MINUS",189},
                        {u"VK_OEM_PERIOD",190},{u"VK_OEM_2",191},{u"VK_OEM_3",192},{u"VK_OEM_4",219},
                        {u"VK_OEM_5",220},{u"VK_OEM_6",221},{u"VK_OEM_7",222}};
                    for(int i=0;i<10;++i) map[u"VK_"+utf16(std::to_string(i))]=48+i;
                    for(int i=0;i<26;++i) map[u"VK_"+std::u16string(1,static_cast<char16_t>(u'A'+i))]=65+i;
                    for(int i=1;i<=24;++i) map[u"VK_F"+utf16(std::to_string(i))]=111+i;
                    return map;
                }();
                auto found=names.find(part); if(found==names.end()) return fallback;
                result.vk=found->second;
            }
            haveKey=true;
        }
        if(end==text.npos) break;
        text.remove_prefix(end+1); if(text.empty()) return fallback;
    }
    const int vk=result.vk;
    if(!haveKey || (!result.ctrl && !result.alt) || vk<=0 || vk>255 || vk==16 || vk==17 || vk==18 || (vk>=160 && vk<=165) || vk==91 || vk==92) return fallback;
    return result;
}
}
Config parseEngineSettings(std::u16string_view text) {
    const auto values=settingsValues(text);
    Config config;
    if(auto found=values.find(u"_native_settings_reload");found!=values.end())config.reloadRequest=found->second;
    const std::pair<const char16_t*,bool Config::*> flags[]={
        {u"默认中文",&Config::defaultChinese},{u"shift切换中英文",&Config::shiftToggle},
        {u"Ctrl+空格切换中英文",&Config::ctrlSpaceToggle},
        {u"中文状态下使用英文标点",&Config::englishPunctuation},{u"/输出顿号",&Config::slashDunhao},
        {u"回车清屏",&Config::enterClear},{u"TAB清屏",&Config::tabClear},
        {u"空码自动清屏",&Config::clearOnNoCode},{u"最大码长无重自动上屏",&Config::maxCodeAutoCommit},
        {u"中英文不限长混合输入",&Config::mixedInput},
        {u"`键拼音反查",&Config::reverseLookup},{u"分号次选",&Config::semicolonSecond},
        {u"引号三选",&Config::quoteThird},{u"显示注释",&Config::showComment},{u"显示拆分",&Config::showSplit}};
    for(const auto& flag:flags) {
        const auto found=values.find(flag.first);
        if(found!=values.end()) config.*flag.second=boolean(found->second,config.*flag.second);
    }
    if(auto found=values.find(u"最大码长");found!=values.end()) config.maxCodeLength=integer(found->second,4,16);
    if(auto found=values.find(u"每页候选个数");found!=values.end()) config.pageSize=integer(found->second,5,10);
    if(auto found=values.find(u"翻页键");found!=values.end()) {
        const auto& page=found->second;
        config.pageKeys=page==u"[ ]"?1:page==u"Shift Tab/Tab"?2:page==u"PageUp/PageDown"?3:0;
    }
    auto enabled=[&](const char16_t* key,bool fallback) { auto found=values.find(key); return found==values.end()?fallback:boolean(found->second,fallback); };
    auto gesture=[&](const char16_t* key,ActionShortcut fallback) { auto found=values.find(key); return found==values.end()?fallback:shortcut(found->second,fallback); };
    config.addWordEnabled=enabled(u"Ctrl+等号手动加词",true);
    config.addWordShortcut=gesture(u"手动加词快捷键",{});
    auto recent=gesture(u"切换最近码表快捷键",{77,true,false,false});
    auto reserved=[&](ActionShortcut s) {
        return (config.ctrlSpaceToggle && s.vk==32 && s.ctrl && !s.alt && !s.shift) ||
            (enabled(u"Alt+\\启用或禁用外挂版",true) && s.vk==220 && s.alt && !s.ctrl && !s.shift) ||
            (s.vk>=49 && s.vk<=57 && ((s.ctrl && !s.alt) || (s.alt && !s.ctrl && !s.shift)));
    };
    const auto s=config.addWordShortcut;
    config.recentSchemaShortcut=recent;
    config.recentSchemaEnabled=enabled(u"Ctrl+m切换最近码表",false) && !reserved(recent);
    if(reserved(s)) config.addWordEnabled=false;
    if(config.addWordEnabled && config.recentSchemaEnabled && recent.matches(s.vk,s.shift,s.ctrl,s.alt,false)) {
        config.addWordEnabled=false;config.recentSchemaEnabled=false;
    }
    return config;
}
CandidateStyle parseCandidateStyle(std::u16string_view text) {
    const auto values=settingsValues(text);
    CandidateStyle style;
    const std::pair<const char16_t*,bool CandidateStyle::*> flags[]={
        {u"竖排候选",&CandidateStyle::vertical},{u"显示候选序号",&CandidateStyle::showIndex},
        {u"候选窗显示编码",&CandidateStyle::showCode},{u"隐藏候选",&CandidateStyle::hideCandidates}};
    for(const auto& flag:flags) {
        const auto found=values.find(flag.first);
        if(found!=values.end()) style.*flag.second=boolean(found->second,style.*flag.second);
    }
    if(auto found=values.find(u"编码伪装");found!=values.end())style.codeMask=found->second;
    if(auto found=values.find(u"字体");found!=values.end() && !found->second.empty()) style.font=found->second;
    if(auto found=values.find(u"主题");found!=values.end() && !found->second.empty()) style.theme=found->second;
    if(auto found=values.find(u"字体大小");found!=values.end()) {
        std::istringstream input(utf8(found->second)); input.imbue(std::locale::classic());
        double size;
        if(input>>size && input.peek()==std::char_traits<char>::eof() && std::isfinite(size)) style.fontSize=std::clamp(size,3.0,200.0);
    }
    if(auto found=values.find(u"延时显示候选(毫秒)");found!=values.end()) style.candidateDelayMs=integer(found->second,0,60000,0);
    if(auto found=values.find(u"延时展开注释和拆分(毫秒)");found!=values.end()) style.annotationDelayMs=integer(found->second,0,60000,0);
    return style;
}
Config loadEngineSettings(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return {};
    return parseEngineSettings(readUnicodeFile(path));
}
std::u16string currentSchemaSetting(std::u16string_view text) {
    return configurationValue(text,u"当前码表");
}
bool validSchemaName(std::u16string_view name) {
    return !name.empty() && name!=u"." && name!=u".." && trim(name)==name &&
        name.find_first_of(u"/\\:<>\"|?*")==name.npos && name.back()!=u'.' &&
        std::none_of(name.begin(),name.end(),[](char16_t c){return c<32;});
}
std::u16string configurationValue(std::u16string_view text,std::u16string_view key) {
    const auto values=settingsValues(text);
    const auto found=values.find(std::u16string(key));
    return found==values.end()?std::u16string{}:found->second;
}
std::u16string withHiddenCandidateSetting(std::u16string_view text,bool hidden) {
    return withConfigurationValue(text,u"隐藏候选",hidden?u"是":u"否");
}
std::u16string withConfigurationValue(std::u16string_view text,std::u16string_view key,std::u16string_view value) {
    if(key.empty() || key.front()==u'#' || key.find_first_of(u"\t ,\r\n")!=key.npos ||
       trim(key)!=key || value.find_first_of(u"\r\n")!=value.npos)
        throw std::invalid_argument("Invalid configuration key or value");
    std::u16string result;
    while(!text.empty()) {
        auto end=text.find_first_of(u"\r\n");
        auto length=end==text.npos?text.size():end+1;
        if(end!=text.npos && text[end]==u'\r' && length<text.size() && text[length]==u'\n') ++length;
        auto line=trim(text.substr(0,end));
        const auto separator=line.find_first_of(u"\t ,");
        const bool setting=separator!=line.npos && trim(line.substr(0,separator))==key;
        if(!setting) result.append(text.substr(0,length));
        text.remove_prefix(length);
    }
    if(!result.empty() && result.back()!=u'\r' && result.back()!=u'\n') result+=u"\r\n";
    result.append(key); result+=u'\t'; result.append(value); result+=u"\r\n";
    return result;
}
}
