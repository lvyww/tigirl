// On-demand desktop actions. There is no scheme-management window or core.
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include "SchemaCatalog.h"
#include "Settings.h"
#include "ConfigStore.h"
#include "InputSettings.h"
#include "UserStore.h"
#include "Text.h"
#include "ManagementUri.h"
#include <fstream>
#include <set>
#include <algorithm>
#include <iostream>
namespace {
std::wstring quote(std::wstring_view value) {
    std::wstring result=L"\"";unsigned slashes=0;
    for(auto c:value){if(c==L'\\'){++slashes;continue;}result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;}
    result.append(slashes*2,L'\\');return result+L'"';
}
void run(const std::filesystem::path& executable,const std::vector<std::wstring>& arguments) {
    auto command=quote(executable.wstring());for(const auto& argument:arguments)command+=L" "+quote(argument);
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("Cannot start dictionary compiler");
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code)throw std::runtime_error("Dictionary compilation failed; the previous scheme remains available");
}
void openTarget(const std::filesystem::path& path) {
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Cannot open target");
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool quiet=false;
    try {
        std::wstring action=L"settings",value;
        if(argc==3 && std::wstring_view(argv[1])==L"--uri")std::tie(action,value)=tiger::parseManagementUri(argv[2]);
        else if(argc==4 && std::wstring_view(argv[1])==L"--menu-action"){action=argv[2];value=argv[3];}
        else if(argc==2 && std::wstring_view(argv[1])==L"--initialize"){action=L"initialize";quiet=true;}
        else if(argc!=1 && !(argc==2 && std::wstring_view(argv[1])==L"--settings"))throw std::runtime_error("Invalid tool arguments");
        const std::set<std::wstring> allowed{L"settings",L"use",L"recent",L"reload",L"folder",L"export",L"official",L"initialize"};
        if(!allowed.count(action) || (action!=L"use" && !value.empty()))throw std::runtime_error("Invalid management action");
        wchar_t path[32768];auto length=GetModuleFileNameW(nullptr,path,32768);if(!length || length>=32768)throw std::runtime_error("Cannot locate installed tools");
        const auto tools=std::filesystem::path(path).parent_path(),bundled=tools/L"tiger-v2.tcd";
        std::filesystem::path root;
        length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",path,32768);
        if(length>=32768)throw std::runtime_error("User root override is too long");
        if(length)root=path;
        else {PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_NO_PACKAGE_REDIRECTION|KF_FLAG_DONT_VERIFY,nullptr,&local)))throw std::runtime_error("Cannot locate user data");root=std::filesystem::path(local)/L"Tigirl";CoTaskMemFree(local);}
        if(!root.is_absolute())throw std::runtime_error("User root must be absolute");
        auto source=root/L"码表",pinyin=root/L"拼音反查码表";
        auto names=tiger::schemaNames(root);
        auto configuration=tiger::readConfiguration(root/L"config.txt");
        auto name=tiger::currentSchemaSetting(configuration);if(name.empty())name=u"虎码字词";
        if(action==L"use")name={reinterpret_cast<const char16_t*>(value.data()),value.size()};
        if(action==L"recent")name=tiger::recentSchemaName(configuration,names);
        if(!tiger::validSchemaName(name))throw std::runtime_error("Invalid scheme name");
        if(action!=L"use" && std::find(names.begin(),names.end(),name)==names.end())name=names.front();
        auto prepare=[&](const std::u16string& selected) {
            if(std::find(names.begin(),names.end(),selected)==names.end())throw std::runtime_error("Scheme folder no longer exists");
            if(std::filesystem::is_directory(source/std::filesystem::path(selected))) {
                wchar_t culture[LOCALE_NAME_MAX_LENGTH]{};if(!GetUserDefaultLocaleName(culture,LOCALE_NAME_MAX_LENGTH))throw std::runtime_error("Cannot read locale");
                run(tools/L"Tigirl.Import.exe",{L"--ensure",(source/std::filesystem::path(selected)).wstring(),pinyin.wstring(),root.wstring(),std::filesystem::path(selected).wstring(),culture});
            }
            auto dictionary=tiger::Dictionary::Open(tiger::activeSchemaDictionaryPath(root,bundled,selected));
            tiger::UserStore store(dictionary,tiger::schemaJournalPath(root,selected));store.refresh();
        };
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(action==L"settings")showInputSettings(nullptr,root/L"config.txt");
        else if(action==L"folder"){std::filesystem::create_directories(source);openTarget(source);}
        else if(action==L"official")openTarget(L"https://github.com/lvyww/bime");
        else if(action==L"use" || action==L"reload" || action==L"recent" || action==L"initialize") {
            if(action==L"initialize")for(const auto& item:names)prepare(item);else prepare(name);
            tiger::selectSchemaConfiguration(root/L"config.txt",name);
        }
        else if(action==L"export") {
            auto dictionary=tiger::Dictionary::Open(tiger::activeSchemaDictionaryPath(root,bundled,name));
            tiger::UserStore store(dictionary,tiger::schemaJournalPath(root,name));auto lexicon=store.refresh();
            std::set<std::u16string> codes;
            for(std::uint32_t i=0;i<dictionary->count(tiger::Section::Main);++i)codes.emplace(dictionary->at(tiger::Section::Main,i).key);
            lexicon->visitUserEdits([&](auto code,const auto&){codes.emplace(code);});
            auto directory=root/L"码表导出";std::filesystem::create_directories(directory);
            GUID guid{};if(FAILED(CoCreateGuid(&guid)))throw std::runtime_error("Cannot name export");wchar_t unique[40];StringFromGUID2(guid,unique,40);
            auto output=directory/(std::filesystem::path(name).wstring()+L" "+unique+L".txt"),temporary=output;temporary+=L".tmp";
            std::ofstream file(temporary,std::ios::binary);file<<"\xef\xbb\xbf";
            for(const auto& code:codes){auto match=lexicon->find(tiger::Section::Main,code);if(!match.count)continue;file<<tiger::utf8(code);for(std::uint32_t i=0;i<match.count;++i)file<<' '<<tiger::utf8(tiger::packText(lexicon->value(match,i)));file<<"\r\n";}
            file.close();if(!file)throw std::runtime_error("Cannot write export");std::filesystem::rename(temporary,output);openTarget(directory);
        }
        LocalFree(argv);CoUninitialize();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';if(!quiet)MessageBoxA(nullptr,error.what(),"Tigirl",MB_OK|MB_ICONERROR);if(argv)LocalFree(argv);return 1;}
}
