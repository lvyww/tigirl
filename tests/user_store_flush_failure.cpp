#include "UserStore.h"
#include <cerrno>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
unsigned failures=0, flushes=0;
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    require(input.good(),"Cannot read fixture journal");
    return {std::istreambuf_iterator<char>(input),{}};
}
void restore(const std::filesystem::path& path,const std::string& bytes) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    output.write(bytes.data(),bytes.size());require(output.good(),"Cannot restore fixture");
}
}
extern "C" int __real_fsync(int fd);
extern "C" int __wrap_fsync(int fd) {
    ++flushes;
    if(failures) { --failures;errno=EIO;return -1; }
    return __real_fsync(fd);
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"user_store_flush_failure <dictionary> <new journal>");
        const std::filesystem::path path=argv[2];
        require(!std::filesystem::exists(path),"Refuse existing journal");
        auto dictionary=tiger::Dictionary::Open(argv[1]);tiger::UserStore store(dictionary,path);
        const std::u16string code=u"flushfailure";
        store.commit({{tiger::ChangeKind::Add,code,u"a"},{tiger::ChangeKind::Add,code,u"b"},{tiger::ChangeKind::Add,code,u"c"}});
        const auto original=read(path);
        const auto retained=store.refresh();
        const std::vector<tiger::UserChange> changes={{tiger::ChangeKind::Advance,code,u"c"},{tiger::ChangeKind::Advance,code,u"c"}};
        const auto expected=store.commit(changes);const auto complete=read(path);
        require(complete!=original,"Fixture made no changes");
        for(unsigned count:{1u,2u}) {
            restore(path,original);flushes=0;failures=count;
            std::string error;
            try {store.commit(changes);}catch(const std::exception& e){error=e.what();}
            require(!error.empty() && failures==0 && flushes==2,"Expected append and rollback flush attempts");
            require(count==1?error.find("Flush user journal")!=std::string::npos:
                error.find("write and rollback failed")!=std::string::npos,"Failure stage was not reported");
            require(read(path)==original,"Failed flush retained an uncommitted batch");
            require(store.refresh()->equivalent(*retained),"Reload changed persisted candidates");
            const auto retry=store.commit(changes);
            require(read(path)==complete && retry->equivalent(*expected),"Retry duplicated or lost adjustment");
            require(!retained->equivalent(*retry),"Published snapshot was mutated");
        }
        std::cout<<"{\"status\":\"passed\",\"injected_flush_cases\":2,\"rollback_and_retry\":true,\"physical_storage_failure_tested\":false}\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
