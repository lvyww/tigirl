#pragma once
#include <string_view>
namespace tiger {
// Reserved basename families, not just extensions: backups, locks and temporary
// files must remain excluded even when their final extension is .txt/.dict.yaml.
inline bool isLearningFile(std::u16string_view name) {
    for(auto prefix:{std::u16string_view(u"用户调整"),std::u16string_view(u".tigirl-user"),std::u16string_view(u".tigirl-learning"),std::u16string_view(u".tigerclaw-learning")}) {
        if(name.size()<prefix.size())continue;
        bool same=true;
        for(std::size_t i=0;i<prefix.size();++i) {
            auto c=name[i];if(c>=u'A' && c<=u'Z')c+=u'a'-u'A';
            if(c!=prefix[i]){same=false;break;}
        }
        if(same)return true;
    }
    return false;
}
}
