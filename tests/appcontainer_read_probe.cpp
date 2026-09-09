#define NOMINMAX
#include <windows.h>
#include "../native/Dictionary.h"
#include "../native/SentenceNgram.h"
#include <iostream>
#include <stdexcept>

// Read-only reproduction using an existing AppContainer token. This tests
// filesystem access, not process isolation, TSF callbacks or foreground input.
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value)CloseHandle(value);}
};
int wmain(int argc,wchar_t** argv) {
    if(argc!=4)return 2; // PID, dictionary, sentence model
    Handle process,token,duplicate;
    try {
        process.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,wcstoul(argv[1],nullptr,10));
        if(!process.value || !OpenProcessToken(process.value,TOKEN_QUERY|TOKEN_DUPLICATE,&token.value) ||
            !DuplicateToken(token.value,SecurityImpersonation,&duplicate.value))throw std::runtime_error("Cannot duplicate test token");
        DWORD container=0,bytes=0;
        if(!GetTokenInformation(token.value,TokenIsAppContainer,&container,sizeof(container),&bytes) || !container)
            throw std::runtime_error("Test requires an AppContainer token");
        if(!ImpersonateLoggedOnUser(duplicate.value))throw std::runtime_error("Cannot impersonate test token");
        struct Revert {~Revert(){RevertToSelf();}} revert;
        std::error_code canonicalError;
        (void)std::filesystem::canonical(argv[2],canonicalError);
        auto dictionary=tiger::Dictionary::Open(argv[2]);
        if(!dictionary || tiger::Dictionary::Open(argv[2])!=dictionary)throw std::runtime_error("Dictionary cache failed");
        auto model=tiger::SentenceNgram::Open(argv[3]);
        if(!model || tiger::SentenceNgram::Open(argv[3])!=model)throw std::runtime_error("Model cache failed");
        bool rejected=false;
        try {tiger::Dictionary::Open(std::filesystem::path(argv[2])/L"missing");}
        catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Missing file was accepted");
        std::cout<<"{\"status\":\"passed\",\"appcontainer\":true,\"canonical_error\":"<<canonicalError.value()
            <<",\"dictionary_mapped\":true,\"model_mapped\":true,\"cache_reused\":true,\"missing_rejected\":true}\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
