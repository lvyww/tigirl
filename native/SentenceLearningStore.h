#pragma once
#include "SentenceLearning.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace tiger {
// V1 append journal. I/O belongs to resource/maintenance workers, never key
// previews. The same wire format is implemented by TigerClaw's C# store.
class SentenceLearningStore {
public:
    static constexpr std::uintmax_t maximumBytes=16*1024*1024;
    static constexpr std::size_t maximumEvents=10000;
private:
    std::filesystem::path path_;
    mutable std::mutex mutex_;
    std::shared_ptr<const SentenceLearningSnapshot> snapshot_=SentenceLearningSnapshot::build({});
    std::uintmax_t size_=static_cast<std::uintmax_t>(-1);
    std::filesystem::file_time_type stamp_{};
    std::int64_t lastRead_=0;
    struct FileLock {
#ifdef _WIN32
        HANDLE file=INVALID_HANDLE_VALUE;OVERLAPPED overlap{};
        explicit FileLock(const std::filesystem::path& path) {
            file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Open learning lock");
            if(!LockFileEx(file,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&overlap)){CloseHandle(file);file=INVALID_HANDLE_VALUE;throw std::runtime_error("Lock learning journal");}
        }
        ~FileLock(){if(file!=INVALID_HANDLE_VALUE){UnlockFileEx(file,0,1,0,&overlap);CloseHandle(file);}}
#else
        int file=-1;
        explicit FileLock(const std::filesystem::path& path) {
            file=::open(path.c_str(),O_CREAT|O_RDWR,0600);if(file<0)throw std::runtime_error("Open learning lock");
            int result;do{result=flock(file,LOCK_EX);}while(result<0 && errno==EINTR);
            if(result<0){::close(file);file=-1;throw std::runtime_error("Lock learning journal");}
        }
        ~FileLock(){if(file>=0){flock(file,LOCK_UN);::close(file);}}
#endif
        FileLock(const FileLock&)=delete;FileLock& operator=(const FileLock&)=delete;
    };
    static std::string hex(std::u16string_view text) {
        static constexpr char digits[]="0123456789abcdef";std::string s;s.reserve(text.size()*4);
        for(auto c:text)for(int shift=12;shift>=0;shift-=4)s+=digits[(c>>shift)&15];return s;
    }
    static bool unhex(std::string_view value,std::u16string& text) {
        if(value.size()%4 || value.size()>2048)return false;text.clear();
        for(std::size_t i=0;i<value.size();i+=4) {
            unsigned n=0;for(int j=0;j<4;j++) {
                char c=value[i+j];unsigned d=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:16;
                if(d==16)return false;n=(n<<4)|d;
            }text+=static_cast<char16_t>(n);
        }return text.empty() || learningCharacters(text)>0;
    }
    static std::uint32_t checksum(std::string_view s) {
        std::uint32_t crc=0xffffffff;
        for(unsigned char c:s){crc^=c;for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1)?0xedb88320:0);}return ~crc;
    }
    static std::string seal(std::string s) {
        return s+'\t'+std::to_string(checksum(s))+'\n';
    }
    std::string bytes() const {
        if(!std::filesystem::exists(path_))return {};
        auto n=std::filesystem::file_size(path_);if(n>maximumBytes)throw std::runtime_error("Learning journal exceeds 16 MiB; export/clear it with the maintenance tool");
        std::ifstream in(path_,std::ios::binary);if(!in)throw std::runtime_error("Read learning journal");
        std::string result(static_cast<std::size_t>(n),'\0');in.read(result.data(),static_cast<std::streamsize>(n));
        if(static_cast<std::uintmax_t>(in.gcount())!=n)throw std::runtime_error("Short learning journal read");return result;
    }
    struct Journal {std::vector<SentenceLearningEvent> events;std::unordered_set<std::string> seen;};
    static Journal parse(std::string_view data) {
        Journal state;std::unordered_set<std::string> removed;
        while(!data.empty()) {
            auto end=data.find('\n');if(end==data.npos)break;auto line=data.substr(0,end);data.remove_prefix(end+1);
            if(line.size()>8192)continue;auto crc=line.rfind('\t');if(crc==line.npos)continue;
            try{std::size_t n=0;auto value=std::stoul(std::string(line.substr(crc+1)),&n);
                if(n!=line.size()-crc-1 || value!=checksum(line.substr(0,crc)))continue;
            }catch(...){continue;}
            line=line.substr(0,crc);std::vector<std::string_view> fields;
            while(true){auto tab=line.find('\t');fields.push_back(line.substr(0,tab));if(tab==line.npos)break;line.remove_prefix(tab+1);}
            if(fields.size()<4 || fields[0]!="TCL1" || fields[2].empty() || fields[2].size()>128)continue;
            std::string id(fields[2]);if(state.seen.count(id))continue;
            std::int64_t time=0;try{std::size_t n=0;time=std::stoll(std::string(fields[3]),&n);if(n!=fields[3].size() || time<0)continue;}catch(...){continue;}
            if(fields[1]=="E" && fields.size()==8) {
                SentenceLearningEvent e;e.id=id;e.time=time;
                if(!unhex(fields[4],e.mode)||!unhex(fields[5],e.code)||!unhex(fields[6],e.text)||!unhex(fields[7],e.context)||
                   e.mode.empty()||e.time<0||e.code.empty()||e.code.size()>128||!learningStaticText(e.text)||learningCharacters(e.context)>2)continue;
                state.seen.insert(id);state.events.push_back(std::move(e));
            }else if(fields[1]=="U" && fields.size()==5 && fields[4].size()<=128) {
                state.seen.insert(id);removed.insert(std::string(fields[4]));
            }else if(fields[1]=="C" && fields.size()==4) {
                state.seen.insert(id);state.events.clear();removed.clear();
            }
        }
        auto& events=state.events;
        events.erase(std::remove_if(events.begin(),events.end(),[&](const auto& e){return removed.count(e.id);}),events.end());
        if(events.size()>maximumEvents)events.erase(events.begin(),events.end()-maximumEvents);
        return state;
    }
    void appendBytes(std::string data,std::string_view addition) {
        // Recover a torn tail under the SAME cross-process lock as the append.
        if(!data.empty() && data.back()!='\n') {
            auto end=data.rfind('\n');auto length=end==data.npos?0:end+1;
            std::filesystem::resize_file(path_,length);data.resize(length);
        }
        if(data.size()+addition.size()>maximumBytes)throw std::runtime_error("Learning journal full; normal input remains available");
        std::ofstream out(path_,std::ios::binary|std::ios::app);if(!out)throw std::runtime_error("Write learning journal");
        out.write(addition.data(),static_cast<std::streamsize>(addition.size()));out.flush();if(!out)throw std::runtime_error("Flush learning journal");
        out.close();
#ifdef _WIN32
        HANDLE file=CreateFileW(path_.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Open learning flush");
        const bool flushed=FlushFileBuffers(file)!=FALSE;CloseHandle(file);
        if(!flushed)throw std::runtime_error("Durable learning flush");
#else
        int file=::open(path_.c_str(),O_WRONLY);if(file<0)throw std::runtime_error("Open learning flush");
        int flushed;do{flushed=::fsync(file);}while(flushed<0 && errno==EINTR);
        ::close(file);if(flushed<0)throw std::runtime_error("Durable learning flush");
#endif
        // Failed or torn writes never enter the in-memory ranking snapshot.
        publish(parse(data+std::string(addition)).events);
    }
    void publish(const std::vector<SentenceLearningEvent>& events) {
        std::atomic_store(&snapshot_,SentenceLearningSnapshot::build(events));
        size_=std::filesystem::exists(path_)?std::filesystem::file_size(path_):0;
        if(size_)stamp_=std::filesystem::last_write_time(path_);lastRead_=learningNow();
    }
public:
    explicit SentenceLearningStore(std::filesystem::path path):path_(std::move(path)){}
    const std::filesystem::path& path()const{return path_;}
    std::shared_ptr<const SentenceLearningSnapshot> snapshot()const{return std::atomic_load(&snapshot_);}
    void refresh() {
        std::lock_guard<std::mutex> local(mutex_);
        if(!std::filesystem::exists(path_)){publish({});return;}
        auto size=std::filesystem::file_size(path_);auto stamp=std::filesystem::last_write_time(path_);
        if(size==size_ && stamp==stamp_ && learningNow()-lastRead_<60)return;
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));publish(parse(bytes()).events);
    }
    void confirm(const std::vector<SentenceLearningEvent>& events) {
        if(events.empty())return;std::lock_guard<std::mutex> local(mutex_);
        std::filesystem::create_directories(path_.parent_path());
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));auto data=bytes();auto state=parse(data);std::string addition;
        for(const auto& e:events) {
            if(e.id.empty()||e.id.size()>128||e.id.find_first_of("\t\r\n")!=std::string::npos || state.seen.count(e.id) || e.mode.empty() || e.mode.size()>512 ||
               e.time<0||e.code.empty()||e.code.size()>128||!learningStaticText(e.text)||(!e.context.empty()&&!learningCharacters(e.context))||learningCharacters(e.context)>2)continue;
            state.seen.insert(e.id);
            addition+=seal("TCL1\tE\t"+e.id+'\t'+std::to_string(e.time)+'\t'+hex(e.mode)+'\t'+hex(e.code)+'\t'+hex(e.text)+'\t'+hex(e.context));
        }
        if(addition.empty()){publish(state.events);return;}appendBytes(std::move(data),addition);
    }
    // Maintenance only. Undo records remove precisely one event, then replay
    // competitors; they do not delete a whole phrase or alter manual user words.
    std::vector<SentenceLearningEvent> entries() {
        std::lock_guard<std::mutex> local(mutex_);if(!std::filesystem::exists(path_))return {};
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));return parse(bytes()).events;
    }
    bool undoLast() {
        std::lock_guard<std::mutex> local(mutex_);if(!std::filesystem::exists(path_))return false;
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));auto data=bytes();auto state=parse(data);if(state.events.empty())return false;
        appendBytes(std::move(data),seal("TCL1\tU\t"+learningId()+'\t'+std::to_string(learningNow())+'\t'+state.events.back().id));return true;
    }
    void clear() {
        std::lock_guard<std::mutex> local(mutex_);std::filesystem::create_directories(path_.parent_path());
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));auto data=bytes();
        appendBytes(std::move(data),seal("TCL1\tC\t"+learningId()+'\t'+std::to_string(learningNow())));
    }
};
}
