#include "UserStore.h"
#include <sys/resource.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
static std::string read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary); return {std::istreambuf_iterator<char>(input),{}};
}
static void write(const std::filesystem::path& path,const std::string& bytes) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc); output.write(bytes.data(),bytes.size());
    require(output.good(),"Cannot restore fixture journal");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"user_store_short_write <dictionary> <new journal>");
        const std::filesystem::path path=argv[2]; require(!std::filesystem::exists(path),"Refuse existing journal");
        auto dictionary=tiger::Dictionary::Open(argv[1]); tiger::UserStore store(dictionary,path);
        const std::u16string code=u"failuretest";
        store.commit({{tiger::ChangeKind::Add,code,u"a"},{tiger::ChangeKind::Add,code,u"b"},{tiger::ChangeKind::Add,code,u"c"}});
        const auto original=read(path);
        const std::vector<tiger::UserChange> changes={{tiger::ChangeKind::Advance,code,u"c"},{tiger::ChangeKind::Advance,code,u"c"}};
        store.commit(changes); const auto complete=read(path);
        const auto appended=complete.size()-original.size();require(appended>1,"No multi-record batch");
        std::signal(SIGXFSZ,SIG_IGN);
        struct rlimit previous{};require(getrlimit(RLIMIT_FSIZE,&previous)==0,"Cannot read size limit");
        for(std::size_t cut=0;cut<appended;++cut) {
            write(path,original);
            auto limit=previous;limit.rlim_cur=original.size()+cut;
            require(setrlimit(RLIMIT_FSIZE,&limit)==0,"Cannot impose short-write limit");
            bool failed=false;
            try { store.commit(changes); } catch(const std::exception&) { failed=true; }
            require(setrlimit(RLIMIT_FSIZE,&previous)==0,"Cannot restore size limit");
            require(failed,"Short write reported success");
            require(read(path)==original,"Short write retained partial batch");
            store.commit(changes);
            require(read(path)==complete,"Retry duplicated or lost adjustment");
        }
        std::cout<<"{\"status\":\"passed\",\"short_write_positions\":"<<appended<<",\"rollback_and_retry\":true}\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
