#include "UserStore.h"
#include "Text.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv) {
 try {
    require(argc==3 || argc==4,"user_store_replay_probe <dictionary> <new journal> [churn-count]");
    const auto churn=argc==4?std::stoul(argv[3]):0ul;
    require(churn<=1000000,"Churn fixture limited to one million changes");
    const auto path=std::filesystem::u8path(argv[2]);require(!std::filesystem::exists(path),"Refuse existing journal");
    auto dictionary=tiger::Dictionary::Open(std::filesystem::u8path(argv[1]));
    tiger::UserStore store(dictionary,path);
    std::vector<tiger::UserChange> changes;
    for(int i=0;i<4000;++i)changes.push_back({tiger::ChangeKind::Add,u"replayfixture"+tiger::utf16(std::to_string(i)),u"a"});
    const auto before=store.commit(changes);
    require(before->addedCodes().size()==4000,"New-code inventory missing entries");
    for(int i=0;i<4000;++i)
        require(before->addedCodes()[i]==u"replayfixture"+tiger::utf16(std::to_string(i)),"New codes lost insertion order");
    auto preview=before->changed({tiger::ChangeKind::Delete,u"replayfixture1",u"a"});
    preview=preview->changed({tiger::ChangeKind::Add,u"replayfixture1",u"again"});
    preview=preview->changed({tiger::ChangeKind::Top,u"zzorderfixture",u"new"});
    require(preview->addedCodes().size()==4001 && preview->addedCodes()[1]==u"replayfixture1" &&
        preview->addedCodes().back()==u"zzorderfixture" && before->addedCodes().size()==4000,
        "Delete/re-add/top changed insertion order or mutated original snapshot");
    require(before->changed({tiger::ChangeKind::Delete,u"absentorderfixture",u"missing"})->addedCodes()==before->addedCodes(),
        "No-op delete introduced a code");
    std::vector<double> samples;
    for(int sample=0;sample<3;++sample) {
        const auto start=std::chrono::steady_clock::now();const auto replay=store.refresh();
        samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        require(replay->equivalent(*before),"Replay changed the published lexicon");
    }
    const std::u16string code=u"replayfixture0";
    const auto after=store.commit({{tiger::ChangeKind::Add,code,u"b"},{tiger::ChangeKind::Delete,code,u"a"},
        {tiger::ChangeKind::Top,code,u"c"},{tiger::ChangeKind::Advance,code,u"b"}});
    const auto original=before->find(tiger::Section::Main,code),updated=after->find(tiger::Section::Main,code);
    require(original.count==1 && before->value(original,0)==u"a","Commit mutated a retained snapshot");
    require(updated.count==2 && after->value(updated,0)==u"b" && after->value(updated,1)==u"c","Batch operation order changed");
    require(store.refresh()->equivalent(*after),"Restart replay lost batch effects");
    require(before->dictionary()==dictionary && after->dictionary()==dictionary,"User store duplicated base dictionary");
    double churnReplay=0;
    if(churn) {
        std::vector<tiger::UserChange> adjustments;
        adjustments.reserve(churn);
        for(unsigned i=0;i<churn;++i)
            adjustments.push_back({tiger::ChangeKind::Top,code,i%2?u"b":u"c"});
        const auto adjusted=store.commit(adjustments);
        const auto start=std::chrono::steady_clock::now();
        const auto replay=store.refresh();
        churnReplay=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        require(replay->equivalent(*adjusted),"Long adjustment history failed replay");
        const auto found=replay->find(tiger::Section::Main,code);
        require(found.count==2 && replay->value(found,0)==(churn%2?u"c":u"b"),"Long history lost final ordering");
        require(after->value(after->find(tiger::Section::Main,code),0)==u"b","Churn mutated earlier snapshot");
    }
    const auto baseCandidates=dictionary->find(tiger::Section::Main,u"ab");
    require(baseCandidates.count>=2,"Fixture requires multiple original ab candidates");
    const auto removed=std::u16string(dictionary->value(baseCandidates,0));
    const auto promoted=std::u16string(dictionary->value(baseCandidates,1));
    const auto alias=tiger::parseEntry(u"显示别名=>实际\\n提交");
    const auto final=store.commit({{tiger::ChangeKind::Add,u";checkpointempty",u"gone"},
        {tiger::ChangeKind::Delete,u";checkpointempty",u"gone"},
        {tiger::ChangeKind::Delete,u"ab",removed},
        {tiger::ChangeKind::Top,u"ab",promoted},
        {tiger::ChangeKind::Add,u"checkpointalias",alias}});
    const auto journalBytes=std::filesystem::file_size(path);
    const auto checkpoint=store.checkpoint();
    require(std::filesystem::file_size(path)==journalBytes && store.refresh()->equivalent(*final),"Checkpoint changed live journal");
    auto checkpointPath=path;checkpointPath+=".checkpoint";
    require(!std::filesystem::exists(checkpointPath),"Refuse existing checkpoint fixture");
    {
        std::ofstream output(checkpointPath,std::ios::binary);
        output.write(reinterpret_cast<const char*>(checkpoint.data()),checkpoint.size());
        require(output.good(),"Cannot write checkpoint fixture");
    }
    tiger::UserStore restored(dictionary,checkpointPath);
    require(restored.refresh()->equivalent(*final) && restored.refresh()->quickSymbols()==final->quickSymbols(),"Checkpoint lost edits or empty-key metadata");
    const auto snapshot=restored.refresh();
    require(snapshot->addedCodes()==final->addedCodes(),"Checkpoint reordered user codes");
    require(snapshot->addedCodes().size()==4002 && snapshot->addedCodes()[4000]==u";checkpointempty" &&
        snapshot->addedCodes()[4001]==u"checkpointalias","Empty-code or alias inventory order changed");
    const auto ab=snapshot->find(tiger::Section::Main,u"ab");
    require(ab.count==baseCandidates.count-1 && snapshot->value(ab,0)==promoted,"Checkpoint changed base deletion or ordering");
    const auto aliasMatch=snapshot->find(tiger::Section::Main,u"checkpointalias");
    require(aliasMatch.count==1 && tiger::displayText(snapshot->value(aliasMatch,0))==u"显示别名" &&
        tiger::commitText(snapshot->value(aliasMatch,0))==u"实际\r\n提交","Checkpoint changed display/commit alias");
    bool nativePublication=false;
#ifdef _WIN32
    {
        const auto held=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(held!=INVALID_HANDLE_VALUE,"Cannot hold legacy journal handle");
        const bool busy=store.compact();CloseHandle(held);
        require(!busy && std::filesystem::file_size(path)==journalBytes,"Compaction bypassed legacy handle");
    }
    nativePublication=store.compact();
    require(nativePublication==(checkpoint.size()<journalBytes),"Unexpected native compaction result");
    require(store.refresh()->equivalent(*final) && std::filesystem::file_size(path)==checkpoint.size(),"Published checkpoint differs");
    require(!store.compact(),"Unchanged checkpoint was republished");
    if(nativePublication) {
        unsigned backups=0;
        std::filesystem::path selectedBackup;
        for(const auto& item:std::filesystem::directory_iterator(path.parent_path())) {
            const auto prefix=path.filename().native()+L".compact-";
            if(item.path().extension()!=L".old" || item.path().filename().native().find(prefix)!=0)continue;
            tiger::UserStore backup(dictionary,item.path());
            require(backup.refresh()->equivalent(*final),"Publication backup lost original words");
            selectedBackup=item.path();
            ++backups;
        }
        require(backups==1,"Missing or duplicated publication backup");
        auto displaced=path;displaced+=L".displaced";
        require(!std::filesystem::exists(displaced),"Refuse existing displaced fixture");
        std::filesystem::rename(path,displaced);
        for(bool checkpointRead:{false,true}) {
            bool rejected=false;
            try {if(checkpointRead)store.checkpoint();else store.refresh();}
            catch(const std::runtime_error& error) {
                rejected=std::string(error.what()).find("checkpoint recovery files exist")!=std::string::npos;
            }
            require(rejected && !std::filesystem::exists(path),"Missing published journal became an empty store");
        }
        auto truncated=path;truncated+=L".compact-truncated.old";
        {std::ofstream output(truncated,std::ios::binary);output.write("TIG",3);}
        bool refused=false;
        try {store.restoreCheckpoint(truncated);}catch(const std::runtime_error&){refused=true;}
        require(refused && !std::filesystem::exists(path),"Incomplete backup was restored");
        refused=false;
        try {store.restoreCheckpoint(path.parent_path()/L"unrelated.tcu.compact-fixture.old");}
        catch(const std::invalid_argument&){refused=true;}
        require(refused && !std::filesystem::exists(path),"Unrelated backup was restored");
        require(store.restoreCheckpoint(selectedBackup),"Selected valid checkpoint did not restore");
        require(store.refresh()->equivalent(*final),"Native recovery changed words");
        require(!store.restoreCheckpoint(selectedBackup),"Recovery overwrote existing live journal");
        require(std::filesystem::exists(displaced) && std::filesystem::file_size(selectedBackup)==journalBytes,
            "Recovery removed retained fixture or backup");
    }
#endif
    const std::vector<tiger::UserChange> followup={{tiger::ChangeKind::Add,u"ab",removed},
        {tiger::ChangeKind::Advance,u"ab",removed},{tiger::ChangeKind::Delete,u"checkpointalias",u"实际\r\n提交"}};
    require(restored.commit(followup)->equivalent(*store.commit(followup)),"Post-checkpoint edits diverged from original history");
    if(churn)require(checkpoint.size()<journalBytes/2,"Churn history did not shrink");
    std::sort(samples.begin(),samples.end());
    std::cout<<"{\"status\":\"passed\",\"codes\":4000,\"replay_median_ms\":"<<samples[1]
        <<",\"churn_changes\":"<<churn<<",\"churn_replay_ms\":"<<churnReplay
        <<",\"journal_bytes\":"<<journalBytes
        <<",\"checkpoint_bytes\":"<<checkpoint.size()<<",\"checkpoint_equivalent\":true"
        <<",\"native_publication\":"<<(nativePublication?"true":"false")
        <<",\"base_delete_and_reorder\":true,\"display_commit_alias\":true,\"post_checkpoint_edits\":true"
        <<",\"retained_snapshot_unchanged\":true,\"batch_order_and_restart\":true,\"shared_dictionary\":true}\n";
 } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
