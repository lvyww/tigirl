#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include "SchemaCatalog.h"
#include "Settings.h"
#include "ConfigStore.h"
#include "InputSettings.h"
#include "OrdinalCase.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <thread>

namespace {
enum {Schema=101,Use,Refresh,Version,Versions,Restore,Source,BrowseSource,Pinyin,BrowsePinyin,Name,Import,Update,Status,Current,InputSettings,CompactUser,RecoverUser};
constexpr UINT Completed=WM_APP+1;
std::wstring wide(std::u16string_view text){return {reinterpret_cast<const wchar_t*>(text.data()),text.size()};}
std::wstring controlText(HWND control) {
    std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(control))+1,L'\0');
    text.resize(static_cast<std::size_t>(GetWindowTextW(control,text.data(),static_cast<int>(text.size()))));return text;
}
std::wstring quote(std::wstring_view argument) {
    std::wstring text=L"\"";std::size_t slashes=0;
    for(wchar_t c:argument) {
        if(c==L'\\'){++slashes;continue;}
        text.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;text+=c;
    }
    text.append(slashes*2,L'\\');return text+L'"';
}
struct Result {int action;DWORD code=1;std::wstring output;};
struct Handle {HANDLE value=nullptr;~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
Result execute(int action,const std::filesystem::path& executable,const std::vector<std::wstring>& arguments) {
    Result result{action,1,{}};
    std::wstring command=quote(executable.wstring());for(const auto& a:arguments)command+=L" "+quote(a);
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};Handle input,output;
    if(!CreatePipe(&input.value,&output.value,&security,0) || !SetHandleInformation(input.value,HANDLE_FLAG_INHERIT,0)) {
        result.output=L"无法创建操作通道。";return result;
    }
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=output.value;startup.hStdError=output.value;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) {
        result.output=L"无法启动工具，请确认安装目录中包含 schema_select.exe 和 lexicon_import.exe。";return result;
    }
    Handle child{process.hProcess},thread{process.hThread};CloseHandle(output.value);output.value=nullptr;
    std::string bytes;char buffer[4096];DWORD count=0;
    while(ReadFile(input.value,buffer,sizeof(buffer),&count,nullptr) && count) {
        if(bytes.size()<65536)bytes.append(buffer,std::min<std::size_t>(count,65536-bytes.size()));
    }
    WaitForSingleObject(child.value,INFINITE);GetExitCodeProcess(child.value,&result.code);
    const int length=MultiByteToWideChar(CP_UTF8,0,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
    result.output.resize(static_cast<std::size_t>(length));
    MultiByteToWideChar(CP_UTF8,0,bytes.data(),static_cast<int>(bytes.size()),result.output.data(),length);
    return result;
}
struct App {
    struct LayoutItem {HWND control;int x,y,width,height;};
    std::vector<LayoutItem> layout;
    struct VersionItem {std::wstring id,label;ULONGLONG timestamp=0;};
    std::vector<VersionItem> versionItems;
    HWND window=nullptr;HFONT font=nullptr;std::filesystem::path root,bundled,tools;
    std::thread worker;bool busy=false,closing=false,test=false,testClose=false,testRestore=false,testNewest=false;int exitCode=0;
    HWND item(int id){return GetDlgItem(window,id);}
    void scale(UINT dpi,bool resize=true) {
        auto px=[&](int value){return MulDiv(value,static_cast<int>(dpi),96);};
        HFONT next=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(!next)throw std::runtime_error("Cannot create manager font");
        for(const auto& entry:layout) {
            SendMessageW(entry.control,WM_SETFONT,reinterpret_cast<WPARAM>(next),FALSE);
            SetWindowPos(entry.control,nullptr,px(entry.x),px(entry.y),px(entry.width),px(entry.height),SWP_NOZORDER|SWP_NOACTIVATE);
        }
        if(font)DeleteObject(font);font=next;
        if(resize) {
            RECT bounds{0,0,px(620),px(510)};
            if(!AdjustWindowRectExForDpi(&bounds,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,
                static_cast<DWORD>(GetWindowLongPtrW(window,GWL_EXSTYLE)),GetDpiForWindow(window)))throw std::runtime_error("Cannot size manager window");
            SetWindowPos(window,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
        InvalidateRect(window,nullptr,TRUE);
    }
    void verifyLayout() {
        const auto actualDpi=GetDpiForWindow(window);
        for(UINT dpi:{96u,144u,192u}) {
            scale(dpi);RECT client{};GetClientRect(window,&client);
            LOGFONTW description{};GetObjectW(font,sizeof(description),&description);
            if(description.lfHeight!=-MulDiv(16,static_cast<int>(dpi),96))throw std::runtime_error("Manager font did not scale");
            for(const auto& entry:layout) {
                RECT bounds{};GetWindowRect(entry.control,&bounds);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&bounds),2);
                if(bounds.left<0 || bounds.top<0 || bounds.right>client.right || bounds.bottom>client.bottom)
                    throw std::runtime_error("Manager control exceeds client area: id="+std::to_string(GetDlgCtrlID(entry.control))+
                        " dpi="+std::to_string(dpi)+" bounds="+std::to_string(bounds.left)+","+std::to_string(bounds.top)+","+
                        std::to_string(bounds.right)+","+std::to_string(bounds.bottom)+" client="+std::to_string(client.right)+","+std::to_string(client.bottom));
            }
        }
        scale(actualDpi);
    }
    void status(const std::wstring& text){SetWindowTextW(item(Status),text.c_str());}
    void refresh() {
        const auto previous=controlText(item(Schema));std::vector<std::u16string> names{u"虎码字词"};
        if(std::filesystem::is_directory(root/L"schemas")) for(const auto& entry:std::filesystem::directory_iterator(root/L"schemas")) {
            auto name=entry.path().filename().u16string();if(entry.is_directory() && tiger::validSchemaName(name))names.push_back(std::move(name));
        }
        std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return tiger::ordinalCompareIgnoreCase(a,b)<0;});
        names.erase(std::unique(names.begin(),names.end(),[](const auto& a,const auto& b){return tiger::ordinalCompareIgnoreCase(a,b)==0;}),names.end());
        SendMessageW(item(Schema),CB_RESETCONTENT,0,0);
        for(const auto& name:names)SendMessageW(item(Schema),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name).c_str()));
        auto selected=SendMessageW(item(Schema),CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(previous.c_str()));
        SendMessageW(item(Schema),CB_SETCURSEL,selected==CB_ERR?0:static_cast<WPARAM>(selected),0);
        auto current=tiger::currentSchemaSetting(tiger::readConfiguration(root/L"config.txt"));
        if(current.empty())current=u"虎码字词";
        SetWindowTextW(item(Current),(L"当前使用："+wide(current)).c_str());
        SendMessageW(item(Version),CB_RESETCONTENT,0,0);versionItems.clear();
    }
    void versions(const std::wstring& output) {
        SendMessageW(item(Version),CB_RESETCONTENT,0,0);versionItems.clear();
        const auto directory=root/L"schemas"/controlText(item(Schema));
        std::wstring current;
        try {
            const auto descriptor=directory/L"current.txt";
            current=std::filesystem::exists(descriptor)?wide(tiger::configurationValue(tiger::readConfiguration(descriptor),u"generation")):L"legacy";
        } catch(const std::exception&) {} // Retained versions remain useful for repair.
        std::size_t at=0;
        while(at<output.size()) {
            const auto end=output.find(L'\n',at);auto id=output.substr(at,end-at);
            if(!id.empty() && id.back()==L'\r')id.pop_back();
            const std::u16string key(reinterpret_cast<const char16_t*>(id.data()),id.size());
            if(id==L"legacy" || tiger::validSchemaGeneration(key)) {
                VersionItem entry{id,L"",0};
                if(id==L"legacy")entry.label=L"最初导入的版本";
                else {
                    WIN32_FILE_ATTRIBUTE_DATA attributes{};FILETIME local{};SYSTEMTIME date{};
                    if(GetFileAttributesExW(tiger::schemaGenerationPath(directory,key).c_str(),GetFileExInfoStandard,&attributes) &&
                        FileTimeToLocalFileTime(&attributes.ftLastWriteTime,&local) && FileTimeToSystemTime(&local,&date)) {
                        entry.timestamp=(static_cast<ULONGLONG>(attributes.ftLastWriteTime.dwHighDateTime)<<32)|attributes.ftLastWriteTime.dwLowDateTime;
                        wchar_t time[40];swprintf_s(time,L"%04u-%02u-%02u %02u:%02u:%02u",static_cast<unsigned>(date.wYear),static_cast<unsigned>(date.wMonth),static_cast<unsigned>(date.wDay),static_cast<unsigned>(date.wHour),static_cast<unsigned>(date.wMinute),static_cast<unsigned>(date.wSecond));
                        entry.label=time;
                    } else entry.label=L"时间未知";
                    entry.label+=L"  ("+id.substr(0,8)+L")";
                }
                if(id==current)entry.label+=L" · 当前";
                versionItems.push_back(std::move(entry));
            }
            if(end==std::wstring::npos)break;at=end+1;
        }
        std::stable_sort(versionItems.begin(),versionItems.end(),[](const auto& a,const auto& b){return a.timestamp>b.timestamp;});
        for(const auto& version:versionItems)SendMessageW(item(Version),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(version.label.c_str()));
        SendMessageW(item(Version),CB_SETCURSEL,0,0);
        status(versionItems.empty()?L"未找到可恢复的完整版本。":L"版本按时间从新到旧排列，选择后可恢复。");
    }
    void enabled(bool value) {
        for(int id:{Schema,Use,Refresh,Version,Versions,Restore,Source,BrowseSource,Pinyin,BrowsePinyin,Name,Import,Update,InputSettings,CompactUser,RecoverUser})EnableWindow(item(id),value);
    }
    std::wstring chooseUserBackup(const std::wstring& schema) {
        if(test) {
            wchar_t path[32768]{};const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_TEST_BACKUP",path,32768);
            if(!length || length>=32768)throw std::runtime_error("Missing isolated backup fixture");
            return path;
        }
        IFileOpenDialog* raw=nullptr;
        if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&raw))))
            throw std::runtime_error("Cannot open backup picker");
        const auto release=[](IFileOpenDialog* value){value->Release();};
        std::unique_ptr<IFileOpenDialog,decltype(release)> dialog(raw,release);
        dialog->SetTitle(L"选择用户词库备份（仅恢复缺失的词库）");
        COMDLG_FILTERSPEC types[]={{L"用户词库备份",L"*.old"}};
        dialog->SetFileTypes(1,types);
        DWORD options=0;dialog->GetOptions(&options);
        dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST);
        const auto directory=schema==L"虎码字词"?root/L"user":root/L"schemas"/schema;
        IShellItem* folder=nullptr;
        if(SUCCEEDED(SHCreateItemFromParsingName(directory.c_str(),nullptr,IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);folder->Release();
        }
        const auto shown=dialog->Show(window);
        if(shown==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};
        if(FAILED(shown))throw std::runtime_error("Backup picker failed");
        IShellItem* selected=nullptr;
        if(FAILED(dialog->GetResult(&selected)))throw std::runtime_error("Cannot read selected backup");
        PWSTR path=nullptr;const auto result=selected->GetDisplayName(SIGDN_FILESYSPATH,&path);selected->Release();
        if(FAILED(result))throw std::runtime_error("Cannot read backup path");
        std::wstring value(path);CoTaskMemFree(path);return value;
    }
    void browse(int id) {
        IFileOpenDialog* dialog=nullptr;
        if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))return;
        DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);
        if(SUCCEEDED(dialog->Show(window))) {
            IShellItem* selected=nullptr;if(SUCCEEDED(dialog->GetResult(&selected))) {
                PWSTR path=nullptr;if(SUCCEEDED(selected->GetDisplayName(SIGDN_FILESYSPATH,&path))){SetWindowTextW(item(id),path);CoTaskMemFree(path);}selected->Release();
            }
        }
        dialog->Release();
    }
    void action(int id) {
        if(busy)return;
        if(id==InputSettings){
            const bool saved=showInputSettings(window,root/L"config.txt",test);
            if(test && saved)showInputSettings(window,root/L"config.txt",2);
            status(saved?L"输入设置已保存，重新聚焦输入框后生效。":L"已取消输入设置。");
            if(test){exitCode=saved?0:1;DestroyWindow(window);}return;
        }
        if(id==Refresh){refresh();status(L"方案列表已刷新。");return;}
        if(id==BrowseSource || id==BrowsePinyin){browse(id==BrowseSource?Source:Pinyin);return;}
        const auto schema=controlText(item(Schema));std::vector<std::wstring> arguments{root.wstring(),bundled.wstring()};
        auto executable=tools/L"schema_select.exe";
        if(id==Use)arguments.push_back(schema);
        else if(id==CompactUser){arguments.push_back(L"--compact-user");arguments.push_back(schema);}
        else if(id==RecoverUser){
            const auto backup=chooseUserBackup(schema);
            if(backup.empty()){status(L"已取消恢复。");return;}
            arguments.insert(arguments.end(),{L"--recover-user",schema,backup});
        }
        else if(id==Versions){arguments.push_back(L"--versions");arguments.push_back(schema);}
        else if(id==Restore) {
            const auto selected=SendMessageW(item(Version),CB_GETCURSEL,0,0);
            if(selected==CB_ERR || static_cast<std::size_t>(selected)>=versionItems.size()){status(L"请先读取并选择一个版本。");return;}
            arguments.insert(arguments.end(),{L"--restore",schema,versionItems[static_cast<std::size_t>(selected)].id});
        } else if(id==Import || id==Update) {
            const auto source=controlText(item(Source)),pinyin=controlText(item(Pinyin)),name=id==Import?controlText(item(Name)):schema;
            if(source.empty() || pinyin.empty() || name.empty()){status(L"请填写码表目录、拼音目录和方案名称。");return;}
            wchar_t culture[LOCALE_NAME_MAX_LENGTH]{};
            if(!GetUserDefaultLocaleName(culture,LOCALE_NAME_MAX_LENGTH)){status(L"无法读取当前区域设置。");return;}
            arguments={id==Import?L"--schema":L"--update",source,pinyin,root.wstring(),name,culture};executable=tools/L"lexicon_import.exe";
        } else return;
        busy=true;enabled(false);status(L"正在处理，请稍候……");
        try {worker=std::thread([this,id,executable,arguments] {
            auto result=std::make_unique<Result>();
            try {*result=execute(id,executable,arguments);}catch(...){result->action=id;result->output=L"操作失败。";}
            if(PostMessageW(window,Completed,0,reinterpret_cast<LPARAM>(result.get())))result.release();
        });} catch(...) {busy=false;enabled(true);throw;}
        // Exercise the real close handler while work is active, before another
        // queued message can complete it. Only enabled in marked test roots.
        if(testClose)SendMessageW(window,WM_CLOSE,0,0);
    }
    ~App(){if(worker.joinable())worker.join();if(font)DeleteObject(font);}
};
void create(App& app) {
    auto control=[&](int id,const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h) {
        HWND child=CreateWindowExW(type==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,app.window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
        if(!child)throw std::runtime_error("Cannot create manager control");app.layout.push_back({child,x,y,w,h});
    };
    control(Current,L"STATIC",L"",0,18,15,600,25);
    control(0,L"STATIC",L"方案",0,18,52,80,25);control(Schema,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,110,48,370,240);
    control(Refresh,L"BUTTON",L"刷新",WS_TABSTOP,490,48,110,28);
    control(Use,L"BUTTON",L"使用方案",WS_TABSTOP,110,86,140,30);control(Versions,L"BUTTON",L"读取保留版本",WS_TABSTOP,265,86,150,30);
    control(0,L"STATIC",L"保留版本",0,18,134,85,25);control(Version,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,110,130,370,240);
    control(Restore,L"BUTTON",L"恢复版本",WS_TABSTOP,490,130,110,28);
    control(0,L"STATIC",L"码表目录",0,18,191,90,25);control(Source,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,110,187,370,28);control(BrowseSource,L"BUTTON",L"浏览…",WS_TABSTOP,490,187,110,28);
    control(0,L"STATIC",L"拼音目录",0,18,231,90,25);control(Pinyin,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,110,227,370,28);control(BrowsePinyin,L"BUTTON",L"浏览…",WS_TABSTOP,490,227,110,28);
    control(0,L"STATIC",L"新方案名称",0,18,271,90,25);control(Name,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,110,267,370,28);
    control(Import,L"BUTTON",L"导入为新方案",WS_TABSTOP,110,307,160,32);control(Update,L"BUTTON",L"更新选中方案",WS_TABSTOP,285,307,170,32);
    control(InputSettings,L"BUTTON",L"输入设置…",WS_TABSTOP,490,86,110,30);
    control(CompactUser,L"BUTTON",L"整理词库",WS_TABSTOP,490,307,110,32);
    control(RecoverUser,L"BUTTON",L"恢复词库…",WS_TABSTOP,110,347,160,32);
    control(0,L"STATIC",L"仅恢复缺失的用户词库。",0,285,352,315,25);
    control(Status,L"STATIC",L"选择方案后点击“使用方案”。导入新方案后可在上方列表中选择。",0,18,395,582,100);
    app.scale(GetDpiForWindow(app.window));app.refresh();
}
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);app->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(window,message,w,l);
    try {
        if(message==WM_DPICHANGED) {
            const auto rect=reinterpret_cast<const RECT*>(l);
            SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
            app->scale(HIWORD(w),false);return 0;
        }
        if(message==WM_COMMAND && HIWORD(w)==BN_CLICKED){app->action(LOWORD(w));return 0;}
        if(message==WM_COMMAND && LOWORD(w)==Schema && HIWORD(w)==CBN_SELCHANGE){SendMessageW(app->item(Version),CB_RESETCONTENT,0,0);app->versionItems.clear();return 0;}
        if(message==Completed) {
            std::unique_ptr<Result> result(reinterpret_cast<Result*>(l));app->worker.join();app->busy=false;app->enabled(true);
            if(result->code==0 && result->action==Versions) {
                app->versions(result->output);
                if(app->testRestore) {
                    const auto found=app->testNewest?app->versionItems.begin():std::find_if(app->versionItems.begin(),app->versionItems.end(),[](const auto& v){return v.id==L"legacy";});
                    if(found==app->versionItems.end() || (app->testNewest && found->id==L"legacy"))throw std::runtime_error("Expected retained version was not listed");
                    if(found->label==found->id)throw std::runtime_error("Version display was not formatted");
                    SendMessageW(app->item(Version),CB_SETCURSEL,static_cast<WPARAM>(found-app->versionItems.begin()),0);
                    app->testRestore=false;PostMessageW(window,WM_COMMAND,Restore,0);return 0;
                }
            } else if(result->code==0 && result->action==RecoverUser) {
                if(result->output.find(L"\"changed\":true")!=std::wstring::npos)
                    app->status(L"用户词库已从所选备份恢复，备份文件仍保留。");
                else if(result->output.find(L"\"changed\":false")!=std::wstring::npos)
                    app->status(L"用户词库已经存在，未覆盖当前数据。");
                else throw std::runtime_error("Unexpected recovery tool response");
            } else if(result->code==0 && result->action==CompactUser) {
                if(result->output.find(L"\"changed\":true")!=std::wstring::npos)
                    app->status(L"选中方案的用户词库已整理，词条和排序保持不变，原日志已备份。");
                else if(result->output.find(L"\"changed\":false")!=std::wstring::npos)
                    app->status(L"用户词库无需整理或正在使用，可稍后重试。");
                else throw std::runtime_error("Unexpected maintenance tool response");
            } else if(result->code==0){app->refresh();app->status(L"操作完成。输入窗口重新获得焦点后会读取方案变化。");}
            else app->status(L"操作失败：\r\n"+result->output);
            if(app->test || app->closing){app->exitCode=result->code==0?0:1;DestroyWindow(window);}return 0;
        }
        if(message==WM_CLOSE){if(app->busy){app->closing=true;app->status(L"操作完成后将关闭窗口。");}else DestroyWindow(window);return 0;}
        if(message==WM_DESTROY){PostQuitMessage(app->exitCode);return 0;}
    } catch(const std::exception& error){app->status(L"无法读取或更新方案，请检查目录和配置文件。");if(app->test){std::cerr<<error.what()<<'\n';app->exitCode=1;DestroyWindow(window);}return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);App app;
    const bool interactiveLaunch=argc==1 || (argc==2 && std::wstring_view(argv[1])==L"--settings") ||
        (argc==3 && std::wstring_view(argv[1]).rfind(L"--",0)!=0);
    try {
        const bool directSettings=argc==2 && std::wstring_view(argv[1])==L"--settings";
        if(argc!=1 && argc!=3 && argc!=5 && !directSettings)throw std::runtime_error("schema_manager [<absolute-user-root> <absolute-bundled-dictionary>]");
        wchar_t path[32768];const auto moduleLength=GetModuleFileNameW(nullptr,path,32768);
        if(!moduleLength || moduleLength>=32768)throw std::runtime_error("Cannot locate manager");app.tools=std::filesystem::path(path).parent_path();
        const bool defaultTest=argc==3 && std::wstring_view(argv[1])==L"--test-default";
        if(argc==1 || defaultTest || directSettings) {
            const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",path,32768);
            if(length>=32768)throw std::runtime_error("User root override is too long");
            if(length)app.root=path;
            else {
                PWSTR local=nullptr;
                if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))throw std::runtime_error("Cannot locate user configuration");
                app.root=std::filesystem::path(local)/L"NativeTiger";CoTaskMemFree(local);
            }
            app.bundled=app.tools/L"tiger-v2.tcd";
        } else {app.root=argv[1];app.bundled=argv[2];}
        const std::wstring mode=defaultTest?L"--test-select":argc==5?argv[3]:L"";
        const wchar_t* testName=defaultTest?argv[2]:argc==5?argv[4]:L"";
        app.test=mode==L"--test-recover-user" || mode==L"--test-compact" || mode==L"--test-settings" || mode==L"--test-select" || mode==L"--test-select-close" || mode==L"--test-import" || mode==L"--test-update" || mode==L"--test-restore" || mode==L"--test-restore-newest";
        app.testClose=mode==L"--test-select-close";app.testNewest=mode==L"--test-restore-newest";app.testRestore=mode==L"--test-restore" || app.testNewest;
        if(!app.root.is_absolute() || !app.bundled.is_absolute() || (argc==5 && !app.test))throw std::runtime_error("Invalid manager arguments");
        if(app.test && !std::filesystem::exists(app.root/L".schema-manager-test"))throw std::runtime_error("Test requires an isolated marked root");
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        WNDCLASSW type{};type.hInstance=instance;type.lpfnWndProc=procedure;type.lpszClassName=L"NativeTigerSchemaManager";type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&type);
        HWND window=CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"原生虎码 · 方案管理",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,640,510,nullptr,nullptr,instance,&app);
        if(!window)throw std::runtime_error("Cannot create manager window");create(app);
        if(directSettings){showInputSettings(window,app.root/L"config.txt");DestroyWindow(window);}
        else if(app.test) {
            app.verifyLayout();
            if(mode!=L"--test-import") {
                auto selected=SendMessageW(app.item(Schema),CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(testName));
                if(selected==CB_ERR)throw std::runtime_error("Test schema not listed");
                SendMessageW(app.item(Schema),CB_SETCURSEL,static_cast<WPARAM>(selected),0);
            }
            SetWindowTextW(app.item(Source),(app.root/L"test-source").c_str());
            SetWindowTextW(app.item(Pinyin),(app.root/L"test-pinyin").c_str());SetWindowTextW(app.item(Name),testName);
            PostMessageW(window,WM_COMMAND,mode==L"--test-recover-user"?RecoverUser:mode==L"--test-compact"?CompactUser:mode==L"--test-settings"?InputSettings:mode==L"--test-import"?Import:mode==L"--test-update"?Update:app.testRestore?Versions:Use,0);
        } else ShowWindow(window,show);
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0)if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
        LocalFree(argv);CoUninitialize();return app.exitCode;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        if(interactiveLaunch) {
            const int length=MultiByteToWideChar(CP_UTF8,0,error.what(),-1,nullptr,0);
            std::wstring detail(static_cast<std::size_t>(length),L'\0');
            if(length>0)MultiByteToWideChar(CP_UTF8,0,error.what(),-1,detail.data(),length);
            if(!detail.empty())detail.pop_back();
            const auto message=L"无法打开管理程序。请检查用户配置目录和安装文件。\n\n"+detail;
            MessageBoxW(nullptr,message.c_str(),L"原生虎码 · 无法启动",MB_OK|MB_ICONERROR);
        }
        if(argv)LocalFree(argv);return 1;
    }
}
