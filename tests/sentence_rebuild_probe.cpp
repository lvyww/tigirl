#include "../native/SentenceRebuild.h"
#include "../native/SentenceRevision.h"
#include "../native/LexiconSerialize.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tiger;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::shared_ptr<const Dictionary> save(const ImportedLexicon& data,const std::filesystem::path& path) {
    auto bytes=serializeImportedLexicon(data);
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(out.good(),"Fixture write failed");}
    return Dictionary::Open(path);
}
int wmain(int argc,wchar_t** argv) {
 try {
    if(argc!=2)return 2;const std::filesystem::path root=argv[1];
    ImportedLexicon ordinary;ordinary.main={{u"zz",{u"中"}},{u"ee",{}},{u"aa",{u"中"}},{u"ab",{u"人"}},
        {u"ςx",{u"字"}},{u"σy",{u"词"}}};
    for(std::size_t i=0;i<ordinary.main.size();++i)ordinary.indexedMain.push_back({ordinary.main[i].code,8,i});
    auto base=save(ordinary,root/L"base.tcd");
    SentenceLexicon inventory(save(prepareSentenceLexicon(ordinary.main),root/L"inventory.tcd"));
    require(inventory.sourceCode(4)==u"ςx","Source casing collapsed final sigma");
    auto snapshot=std::make_shared<const Lexicon>(base);
    const auto originalRevision=sentenceRevision(inventory,*snapshot);
    auto initial=rebuildSentenceLexicon(inventory,*snapshot);
    require(initial.fullCodes.at(u"中")==u"ZZ","Initial source-order tie changed");
    const std::vector<UserChange> edits={{ChangeKind::Delete,u"zz",u"中"},
        {ChangeKind::Add,u"ee",u"中"},{ChangeKind::Top,u"aa",u"人"},
        {ChangeKind::Add,u"yy",u"别名一\x1e国"},{ChangeKind::Add,u"bb",u"国"},
        {ChangeKind::Add,u"yy",u"别名二\x1e国"},{ChangeKind::Delete,u"yy",u"国"},
        {ChangeKind::Add,u"yy",u"别名三\x1e国"}};
    for(const auto& edit:edits)snapshot=snapshot->changed(edit);
    const auto revision=sentenceRevision(inventory,*snapshot);
    std::string revisionText;for(auto c:revision)revisionText.push_back(static_cast<char>(c));
    require(revision.size()==64 && revision!=originalRevision,"Revision omitted effective edits");
    require(sentenceRevision(inventory,*snapshot->changed(edits.back()))==revision,"Redundant history changed revision");
    auto one=Lexicon(base).changed({ChangeKind::Add,u"yy",u"新"})->changed({ChangeKind::Add,u"qq",u"新"});
    auto two=Lexicon(base).changed({ChangeKind::Add,u"qq",u"新"})->changed({ChangeKind::Add,u"yy",u"新"});
    require(sentenceRevision(inventory,*one)!=sentenceRevision(inventory,*two),"Revision omitted new-code order");
    auto rebuilt=rebuildSentenceLexicon(inventory,*snapshot);
    require(rebuilt.fullCodes.at(u"中")==u"EE","Formerly empty base key lost precedence");
    require(rebuilt.fullCodes.at(u"人")==u"AA","Top did not update primary code");
    require(rebuilt.fullCodes.at(u"国")==u"YY","New-code insertion order changed");
    auto mapped=save(rebuilt,root/L"rebuilt.tcd");SentenceLexicon updated(mapped);
    require(updated.sourceCodeCount()==8 && updated.sourceCode(6)==u"yy" && updated.sourceCode(7)==u"bb","Merged inventory changed");
    auto candidates=updated.candidates(u"yy");
    require(candidates.size()==1 && candidates[0].text==u"国" && candidates[0].rank==1,"Alias unpacking/dedup changed");
    require(updated.candidates(u"zz").empty() && inventory.candidates(u"zz").size()==1,"Delete changed retained generation");
    require(updated.candidates(u"ςx").size()==1,"Unicode source code lookup lost candidates");
    auto after=snapshot->changed({ChangeKind::Delete,u"ee",u"中"});
    require(rebuildSentenceLexicon(inventory,*after).fullCodes.at(u"中")==u"AA","Later edit used stale metadata");
    auto broken=prepareSentenceLexicon({{u"zz",{u"中"}}});
    SentenceLexicon incomplete(save(broken,root/L"incomplete.tcd"));bool rejected=false;
    try{rebuildSentenceLexicon(incomplete,*snapshot);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Mismatched base inventory accepted");
    require(snapshot->dictionary()==base,"Rebuild replaced base mapping");
    std::cout<<"{\"status\":\"passed\",\"source_order\":true,\"empty_keys\":true,\"user_order\":true,\"alias_dedup\":true,\"primary_updates\":true,\"unicode_spelling\":true,\"immutable_generation\":true,\"mismatch_rejected\":true,\"revision\":\""<<revisionText<<"\"}\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
