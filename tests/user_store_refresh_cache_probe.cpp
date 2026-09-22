#include "UserStore.h"
#include <filesystem>
#include <fstream>
#include "EditableText.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    using namespace tiger;
    const std::filesystem::path dictionaryPath(argv[1]);
    const auto journal=dictionaryPath.parent_path()/"refresh-cache.tcu";
    std::error_code ignored;std::filesystem::remove(journal,ignored);
    auto sidecar=journal;sidecar+=".lock";std::filesystem::remove(sidecar,ignored);
    auto dictionary=Dictionary::Open(dictionaryPath);
    UserStore store(dictionary,journal);
    auto first=store.refresh();
    auto second=store.refresh();
    check(first==second,"unchanged refresh replayed the journal");

    UserStore copy=store;
    auto committed=copy.commit({{ChangeKind::Add,u"c",u"缓存"}});
    check(store.refresh()==committed,"UserStore copies did not share the committed cache");

    UserStore external(dictionary,journal);
    auto externalView=external.commit({{ChangeKind::Add,u"d",u"外部"}});
    auto refreshed=store.refresh();
    check(refreshed!=committed,"external journal write was hidden by the refresh cache");
    auto match=refreshed->find(Section::Main,u"d");
    check(match.count==1 && refreshed->value(match,0)==u"外部","external write was not replayed");
    check(store.refresh()==refreshed,"post-reload unchanged refresh replayed the journal");

    auto externalMatch=externalView->find(Section::Main,u"d");
    check(externalMatch.count==1,"external writer did not persist its change");
    const auto special=std::u16string(u"中文\t换行\r\n反斜杠\\𰻞")+char16_t(0xd800);
    check(editableValue(editableField(special))==special,"editable field roundtrip lost characters");
    check(editableValue(editableField(special,true),true)==special,"user adjustment escaping lost characters");
    check(editableValue("A\\sB\\nC\\tD\\\\",true)==u"A B\r\nC\tD\\","TigerClaw escape semantics differ");
    {std::ofstream out(journal,std::ios::binary);out<<"\xef\xbb\xbf# editable\r\n{添加}xx "<<utf8(u"甲")<<"\r\n{添加}xx "<<utf8(u"乙")<<"\r\n  {置顶}xx\t"<<utf8(u"乙");}
    auto hand=store.refresh();auto list=hand->find(Section::Main,u"xx");
    check(list.count==2 && hand->value(list,0)==u"乙","manual UTF-8 edit/top not reloaded");
    store.commit({{ChangeKind::Advance,u"xx",u"甲"},{ChangeKind::Delete,u"xx",u"乙"}});
    auto edited=store.refresh();list=edited->find(Section::Main,u"xx");
    check(list.count==1 && edited->value(list,0)==u"甲","append after unterminated row lost edit");
    const auto multiline=std::u16string(u"空 格\r\n第二行\t反斜杠\\单LF\n");
    store.commit({{ChangeKind::Add,u"xy",multiline}});
    UserStore reopened(dictionary,journal);
    auto persisted=reopened.refresh();auto specialList=persisted->find(Section::Main,u"xy");
    check(specialList.count==1 && persisted->value(specialList,0)==multiline,"user adjustment persisted escaping changed text");
    {std::ifstream in(journal,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
     check(bytes.find(u8"{添加}xy\t空\\s格\\n第二行\\t反斜杠\\\\单LF\\u000a")!=std::string::npos,"written adjustment format differs from TigerClaw");}
    {std::ofstream out(journal,std::ios::app);out<<"bad row\n";}
    const auto size=std::filesystem::file_size(journal);bool rejected=false;
    try{store.commit({{ChangeKind::Add,u"xx",u"丙"}});}catch(...){rejected=true;}
    check(rejected && std::filesystem::file_size(journal)==size,"malformed user edit was overwritten");
    std::cout<<"PASS: UserStore refresh reuses unchanged snapshots and reloads external writes.\n";
    return 0;
}
