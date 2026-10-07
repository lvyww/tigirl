#include "SsfSkin.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace tiger::skin {
namespace {
std::u16string trim(std::u16string_view s){while(!s.empty() && (s.front()==u' ' || s.front()==u'\t' || s.front()==u'\r'))s.remove_prefix(1);while(!s.empty() && (s.back()==u' ' || s.back()==u'\t' || s.back()==u'\r'))s.remove_suffix(1);return std::u16string(s);}
std::u16string lower(std::u16string s){for(auto& c:s)if(c>=u'A' && c<=u'Z')c+=32;return s;}
using Section=std::map<std::u16string,std::u16string>;
std::u16string get(const Section& s,std::u16string_view key){auto i=s.find(std::u16string(key));return i==s.end()?std::u16string{}:i->second;}
std::map<std::u16string,Section> ini(std::u16string_view text){
    std::map<std::u16string,Section> result;std::u16string section;unsigned lines=0;
    while(!text.empty()){auto end=text.find(u'\n');auto line=trim(text.substr(0,end));text=end==text.npos?std::u16string_view{}:text.substr(end+1);
        if(++lines>20000 || line.size()>16384 || line.find(u'\0')!=line.npos)throw std::runtime_error("Invalid/oversized skin.ini line");
        if(line.empty() || line[0]==u';' || line[0]==u'#')continue;
        if(line.front()==u'['){if(line.back()!=u']')throw std::runtime_error("Malformed skin.ini section");section=lower(trim(std::u16string_view(line).substr(1,line.size()-2)));continue;}
        // SSF authors may leave legacy/trailing data in status-bar sections.
        // Only candidate-window sections are consumed; keep their syntax strict.
        if(!section.empty() && section!=u"general" && section!=u"display" &&
            section!=u"scheme_h1" && section!=u"scheme_h2" &&
            section!=u"scheme_v1" && section!=u"scheme_v2")continue;
        auto separator=line.find(u'=');if(separator==line.npos || section.empty())throw std::runtime_error("Malformed skin.ini field");
        auto key=lower(trim(std::u16string_view(line).substr(0,separator)));auto value=trim(std::u16string_view(line).substr(separator+1));
        if(key.empty() || !result[section].emplace(std::move(key),std::move(value)).second)throw std::runtime_error("Duplicate/empty skin.ini key");
    }return result;
}
std::vector<int> numbers(std::u16string_view text,std::size_t count,int minimum=0,int maximum=8192){
    std::vector<int> result;
    while(!text.empty()){auto end=text.find(u',');auto part=trim(text.substr(0,end));text=end==text.npos?std::u16string_view{}:text.substr(end+1);bool negative=false;std::size_t at=0;
        if(!part.empty() && (part[0]==u'-' || part[0]==u'+')){negative=part[0]==u'-';at=1;}if(at==part.size())return {};
        std::int64_t value=0;for(;at<part.size();++at){auto c=part[at];if(c<u'0' || c>u'9')return {};value=value*10+c-u'0';if(value>100000000)return {};}
        value=negative?-value:value;if(value<minimum || value>maximum)return {};result.push_back(static_cast<int>(value));if(result.size()>count)return {};
    }return result.size()==count?result:std::vector<int>{};
}
std::uint32_t color(std::u16string text,std::uint32_t fallback){
    text=lower(std::move(text));if(text.size()<3 || text.size()>8 || text.substr(0,2)!=u"0x")return fallback;std::uint32_t bgr=0;
    for(auto c:std::u16string_view(text).substr(2)){int digit=c>=u'0' && c<=u'9'?c-u'0':c>=u'a' && c<=u'f'?c-u'a'+10:-1;if(digit<0)return fallback;bgr=bgr*16+digit;}
    return 0xff000000u|((bgr&255)<<16)|(bgr&0xff00)|((bgr>>16)&255);
}
}
const Layout* Definition::choose(bool vertical,bool code,bool candidates) const{
    const auto requested=!candidates?(vertical?Variant::VerticalCode:Variant::HorizontalCode):!code?(vertical?Variant::VerticalCandidates:Variant::HorizontalCandidates):(vertical?Variant::Vertical:Variant::Horizontal);
    for(auto v:{requested,vertical?Variant::Vertical:Variant::Horizontal,vertical?Variant::VerticalCandidates:Variant::HorizontalCandidates,vertical?Variant::Horizontal:Variant::Vertical}){auto i=layouts.find(v);if(i!=layouts.end())return &i->second;}return nullptr;
}
Definition parseDefinition(Files files){
    auto config=files.find(u"skin.ini");if(config==files.end())throw std::runtime_error("Missing skin.ini");auto sections=ini(decodeText(config->second));Definition result;
    auto& general=sections[u"general"];auto& display=sections[u"display"];
    if(auto name=get(general,u"skin_name");!name.empty())result.name=name.substr(0,256);result.author=get(general,u"skin_author").substr(0,256);result.font=get(display,u"font_ch").substr(0,128);result.preview=get(general,u"preview_comp");
    if(auto n=numbers(get(display,u"font_size"),1,3,200);!n.empty())result.fontSize=static_cast<float>(n[0]);
    result.englishFont=get(display,u"font_en").substr(0,128);
    // Optional Tigirl extensions; absent values add no artificial spacing.
    const auto spacing=[&](const char16_t* key){auto n=numbers(get(display,key),1,0,200);return n.empty()?0.f:static_cast<float>(n[0]);};
    result.candidateSpacing=spacing(u"candidate_spacing");result.lineSpacing=spacing(u"line_spacing");result.characterSpacing=spacing(u"character_spacing");
    result.codeColor=color(get(display,u"pinyin_color"),result.codeColor);result.firstColor=color(get(display,u"zhongwen_first_color"),result.firstColor);result.textColor=color(get(display,u"zhongwen_color"),result.textColor);
    result.annotationColor=color(get(display,u"comphint_color"),result.textColor);

    struct Spec{Variant variant;const char16_t* section;const char16_t* prefix;};
    const Spec specs[]={{Variant::Horizontal,u"scheme_h1",u""},{Variant::Vertical,u"scheme_v1",u""},{Variant::HorizontalCode,u"scheme_h2",u"pinyin_"},{Variant::HorizontalCandidates,u"scheme_h2",u"zhongwen_"},{Variant::VerticalCode,u"scheme_v2",u"pinyin_"},{Variant::VerticalCandidates,u"scheme_v2",u"zhongwen_"}};
    std::set<std::u16string> used;
    for(const auto& spec:specs){auto it=sections.find(spec.section);if(it==sections.end())continue;const auto& values=it->second;const std::u16string prefix=spec.prefix;Layout layout;
        auto image=get(values,prefix+u"pic");if(image.empty())continue;layout.image=archivePath(image);
        if(files.find(layout.image)==files.end()){result.warnings.push_back(u"缺少背景图片："+image);continue;}used.insert(layout.image);
        const auto stretch=[&](const std::u16string& key){Stretch s;auto n=numbers(get(values,key),3);if(!n.empty()){s={n[0]==1?1:0,n[1],n[2]};}else if(!get(values,key).empty())result.warnings.push_back(u"已忽略无效伸缩参数："+key);return s;};
        const auto margins=[&](const char16_t* key){auto n=numbers(get(values,key),4);return n.empty()?Insets{}:Insets{n[0],n[1],n[2],n[3]};};
        layout.horizontal=stretch(prefix+u"layout_horizontal");layout.vertical=stretch(prefix+u"layout_vertical");layout.code=margins(u"pinyin_marge");layout.candidates=margins(u"zhongwen_marge");
        const std::u16string customPrefix=prefix==u"zhongwen_"?u"cand_custom":prefix==u"pinyin_"?u"comp_custom":u"custom";
        auto count=numbers(get(values,customPrefix+u"_cnt"),1,0,64);
        if(!count.empty())for(int i=0;i<count[0];++i){auto number=std::to_string(i);std::u16string key=customPrefix+std::u16string(number.begin(),number.end());auto file=get(values,key);if(file.empty()){result.warnings.push_back(u"已跳过未定义装饰："+key);continue;}
            Decoration decoration;decoration.image=archivePath(file);if(files.find(decoration.image)==files.end()){result.warnings.push_back(u"缺少装饰图片："+file);continue;}
            auto alignment=numbers(get(values,key+u"_align"),10,-8192,8192);if(!alignment.empty()){std::copy(alignment.begin(),alignment.end(),decoration.align.begin());decoration.explicitAlignment=true;}
            layout.decorations.push_back(std::move(decoration));used.insert(layout.decorations.back().image);
        }
        result.layouts.emplace(spec.variant,std::move(layout));
    }
    if(result.layouts.empty())throw std::runtime_error("SSF has no supported candidate background");
    if(!result.preview.empty()){try{result.preview=archivePath(result.preview);if(files.find(result.preview)!=files.end())used.insert(result.preview);else result.preview.clear();}catch(...){result.preview.clear();}}
    for(const auto& name:used)result.assets.emplace(name,std::move(files.at(name)));return result;
}
std::vector<AxisSlice> sliceAxis(unsigned source,float destination,Stretch stretch){
    std::vector<AxisSlice> out;if(!source || !std::isfinite(destination) || destination<=0)return out;
    float before=static_cast<float>(std::max(0,stretch.before)),after=static_cast<float>(std::max(0,stretch.after));const float size=static_cast<float>(source);
    if(before+after>=size){const float factor=(size-1)/std::max(1.f,before+after);before*=factor;after*=factor;}
    const float factor=before+after>destination?destination/(before+after):1.f;const float left=before*factor,right=after*factor;
    const auto add=[&](float s,float n,float d,float length){if(n>0 && length>0)out.push_back({s,n,d,length});};add(0,before,0,left);
    const float middle=size-before-after,available=std::max(0.f,destination-left-right);
    if(stretch.mode==1 && middle>0 && available/middle<=64){for(float d=0;d<available;){const float n=std::min(middle,available-d);if(n<=0)break;add(before,n,left+d,n);d+=n;}}
    else add(before,middle,left,available);add(size-after,after,destination-right,right);return out;
}
}
