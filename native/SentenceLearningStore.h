#pragma once
#include "SentenceLearning.h"
#include "EditableText.h"
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
// Editable UTF-8 TSV journal. I/O belongs to resource/maintenance workers, never key previews.
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
    bool missing_=false;
    SentenceLearningAccumulator accumulator_;
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
    static bool readField(std::string_view value,std::u16string& text) {
        try{text=editableValue(value);}catch(...){return false;}
        return text.size()<=512 && (text.empty() || learningCharacters(text)>0);
    }
    static std::string seal(std::string s) {return s+'\n';}
    std::string bytes() const {
        if(!std::filesystem::exists(path_))return {};
        auto n=std::filesystem::file_size(path_);if(n>maximumBytes)throw std::runtime_error("Learning journal exceeds 16 MiB; export/clear it with the maintenance tool");
        std::ifstream in(path_,std::ios::binary);if(!in)throw std::runtime_error("Read learning journal");
        std::string result(static_cast<std::size_t>(n),'\0');in.read(result.data(),static_cast<std::streamsize>(n));
        if(static_cast<std::uintmax_t>(in.gcount())!=n)throw std::runtime_error("Short learning journal read");return result;
    }
    struct Journal {std::vector<SentenceLearningEvent> events;std::unordered_set<std::string> seen,removed;};
    std::string parsedBytes_;
    std::shared_ptr<const Journal> parsedJournal_;
    std::shared_ptr<const Journal> journal(const std::string& data) {
        // Verify actual bytes, not just mtime/size: replacement and cross-process
        // edits cannot reuse stale replay state. Only parsing is skipped here.
        if(parsedJournal_ && data==parsedBytes_)return parsedJournal_;
        auto parsed=std::make_shared<Journal>(parse(data));
        if(data.size()<=1024*1024){parsedBytes_=data;parsedJournal_=parsed;}
        else {std::string().swap(parsedBytes_);parsedJournal_.reset();}
        return parsed;
    }
    static Journal parse(std::string_view data,bool limitWindow=true) {
        Journal state;auto& removed=state.removed;
        if(data.substr(0,3)=="\xef\xbb\xbf")data.remove_prefix(3);
        while(!data.empty()) {
            auto end=data.find('\n');auto line=data.substr(0,end);
            if(end==data.npos)data={};else data.remove_prefix(end+1);
            if(!line.empty() && line.back()=='\r')line.remove_suffix(1);
            if(line.empty() || line.front()=='#')continue;
            if(line.size()>8192)throw std::runtime_error("Learning text row too long");
            std::vector<std::string_view> fields;
            while(true){auto tab=line.find('\t');fields.push_back(line.substr(0,tab));if(tab==line.npos)break;line.remove_prefix(tab+1);}
            if(fields.size()<4 || fields[0]!="TCL2" || fields[2].empty() || fields[2].size()>128)throw std::runtime_error("Invalid learning text row");
            std::string id(fields[2]);if(state.seen.count(id))continue;
            std::int64_t time=0;try{std::size_t n=0;time=std::stoll(std::string(fields[3]),&n);if(n!=fields[3].size() || time<0)throw std::runtime_error("Invalid learning text row");}catch(...){throw std::runtime_error("Invalid learning timestamp");}
            if(fields[1]=="E" && fields.size()==8) {
                SentenceLearningEvent e;e.id=id;e.time=time;
                if(!readField(fields[4],e.mode)||!readField(fields[5],e.code)||!readField(fields[6],e.text)||!readField(fields[7],e.context)||
                   e.mode.empty()||e.time<0||e.code.empty()||e.code.size()>128||!learningStaticText(e.text)||learningCharacters(e.context)>2)throw std::runtime_error("Invalid learning text row");
                state.seen.insert(id);state.events.push_back(std::move(e));
            }else if(fields[1]=="U" && fields.size()==5 && fields[4].size()<=128) {
                state.seen.insert(id);removed.insert(std::string(fields[4]));
            }else if(fields[1]=="C" && fields.size()==4) {
                state.seen.insert(id);state.events.clear();removed.clear();
            }else throw std::runtime_error("Invalid learning operation");
        }
        auto& events=state.events;
        events.erase(std::remove_if(events.begin(),events.end(),[&](const auto& e){return removed.count(e.id);}),events.end());
        if(limitWindow && events.size()>maximumEvents)events.erase(events.begin(),events.end()-maximumEvents);
        return state;
    }
    void appendBytes(std::string data,std::string_view addition,const Journal* appended=nullptr) {
        // Validate before appending, including a final row without a newline.
        // Keep user edits intact; malformed data must never be silently replaced.
        if(!appended)journal(data);
        std::string separated;
        if(!data.empty() && data.back()!='\n'){separated="\n";separated+=addition;addition=separated;}
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
        data+=addition;
        if(appended) {
            auto next=std::make_shared<Journal>(*appended);
            // Parse only new E records, using exactly the journal validator. This
            // retains malformed-field rejection and unknown-target tombstones.
            const auto delta=parse(addition,false);next->seen.insert(delta.seen.begin(),delta.seen.end());
            for(const auto& e:delta.events)if(!next->removed.count(e.id))next->events.push_back(e);
            if(next->events.size()>maximumEvents)next->events.erase(next->events.begin(),next->events.end()-maximumEvents);
            if(data.size()<=1024*1024){parsedBytes_=std::move(data);parsedJournal_=next;}
            else {std::string().swap(parsedBytes_);parsedJournal_.reset();}
            publish(next->events);
        }else publish(journal(data)->events);
    }
    void publish(const std::vector<SentenceLearningEvent>& events) {
        // Pointer identity is the decoder's cache revision. Reuse an unchanged
        // empty snapshot (missing, empty, cleared or entirely invalid journal).
        const auto current=snapshot();
        if(!events.empty() || !current->empty()) {
            auto next=accumulator_.update(events);
            if(!current->empty() || !next->empty())std::atomic_store(&snapshot_,std::move(next));
        }
        missing_=!std::filesystem::exists(path_);
        size_=missing_?0:std::filesystem::file_size(path_);
        stamp_=missing_?std::filesystem::file_time_type{}:std::filesystem::last_write_time(path_);
        lastRead_=learningNow();
    }
public:
    explicit SentenceLearningStore(std::filesystem::path path):path_(std::move(path)){}
    const std::filesystem::path& path()const{return path_;}
    std::shared_ptr<const SentenceLearningSnapshot> snapshot()const{return std::atomic_load(&snapshot_);}
    void refresh() {
        std::lock_guard<std::mutex> local(mutex_);
        if(!std::filesystem::exists(path_)){if(!missing_)publish({});return;}
        auto size=std::filesystem::file_size(path_);auto stamp=std::filesystem::last_write_time(path_);
        if(!missing_ && size==size_ && stamp==stamp_ && learningNow()-lastRead_<60)return;
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));publish(journal(bytes())->events);
    }
    void confirm(const std::vector<SentenceLearningEvent>& events) {
        if(events.empty())return;std::lock_guard<std::mutex> local(mutex_);
        std::filesystem::create_directories(path_.parent_path());
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));auto data=bytes();auto original=journal(data);auto state=*original;std::string addition;
        for(const auto& e:events) {
            if(e.id.empty()||e.id.size()>128||e.id.find_first_of("\t\r\n")!=std::string::npos || state.seen.count(e.id) || e.mode.empty() || e.mode.size()>512 || !learningCharacters(e.mode) || !learningCharacters(e.code) ||
               e.time<0||e.code.empty()||e.code.size()>128||!learningStaticText(e.text)||(!e.context.empty()&&!learningCharacters(e.context))||learningCharacters(e.context)>2)continue;
            state.seen.insert(e.id);
            addition+=seal("TCL2\tE\t"+e.id+'\t'+std::to_string(e.time)+'\t'+editableField(e.mode)+'\t'+editableField(e.code)+'\t'+editableField(e.text)+'\t'+editableField(e.context));
        }
        if(addition.empty()){publish(state.events);return;}appendBytes(std::move(data),addition,original.get());
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
        appendBytes(std::move(data),seal("TCL2\tU\t"+learningId()+'\t'+std::to_string(learningNow())+'\t'+state.events.back().id));return true;
    }
    void clear() {
        std::lock_guard<std::mutex> local(mutex_);std::filesystem::create_directories(path_.parent_path());
        FileLock lock(std::filesystem::path(path_.u16string()+u".lock"));auto data=bytes();
        appendBytes(std::move(data),seal("TCL2\tC\t"+learningId()+'\t'+std::to_string(learningNow())));
    }
};
}
