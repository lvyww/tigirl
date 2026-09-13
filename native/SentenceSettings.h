#pragma once
#include "SentenceLexicon.h"
#include <string>
namespace tiger {
struct SentenceSettings {
    bool autoEnableBySchema=true,autoCommit=true,allowDuplicateSingleCharacters=true;
    bool selfLearning=true;
    int minimumRetainedRaw=0,commonCharacterLimit=1500;
    std::u16string fullCodeWhitelist=u"便深候整调脸照病增响剑哪微营修愿密脑续假值弹您球激游模静源副座喝富宣呼检救嘴税探脱误释跳睡减蒙镇域洞湾卖暴输缓熟庭俄韩混词授摆诺稳塔潜硬萧侵懂蒋赞赛胸偷烧墙爆操挑撤筑戴植援凭聚凌梁箭圈惨飘旗牌废缩碎挺晓桥赫凝潮掩拔播艘滚兽隆薄愤漫爹撒佩绕";
    // Native resource-path extension; all other fields follow original Core.
    std::u16string modelPath;
    bool activeForSchema(std::u16string_view schema) const {return autoEnableBySchema && schema.find(u"整句")!=schema.npos;}
    SentenceLexicon::Characters whitelist() const;
};
// Preserve explicit empty values, unlike readers which trim off a trailing
// separator before checking whether a key exists.
SentenceSettings parseSentenceSettings(std::u16string_view text);
}
