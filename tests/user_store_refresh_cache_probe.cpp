#include "UserStore.h"
#include <filesystem>
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
    std::cout<<"PASS: UserStore refresh reuses unchanged snapshots and reloads external writes.\n";
    return 0;
}
