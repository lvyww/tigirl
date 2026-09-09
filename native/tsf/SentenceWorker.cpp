#include "SentenceWorker.h"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
namespace tiger::tsf {
struct SentenceWorker::State {
    struct Task {std::uint64_t revision;std::function<SentenceCompletion()> work;};
    std::mutex mutex;std::condition_variable ready;bool closed=false,running=false;
    std::map<std::uint64_t,Task> tasks;
    std::deque<std::uint64_t> order;
    std::map<std::uint64_t,std::uint64_t> latest;
    std::map<std::uint64_t,SentenceCompletion> completed;
};
struct SentenceWorker::Launch {std::shared_ptr<State> state;HMODULE module;};
SentenceWorker::SentenceWorker():state_(std::make_shared<State>()) {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&run),&module))throw std::runtime_error("Retain sentence worker module");
    auto launch=new Launch{state_,module};auto thread=CreateThread(nullptr,0,run,launch,0,nullptr);
    if(!thread){delete launch;FreeLibrary(module);throw std::runtime_error("Start sentence worker");}CloseHandle(thread);
}
SentenceWorker::~SentenceWorker() {
    {std::lock_guard<std::mutex> lock(state_->mutex);state_->closed=true;state_->tasks.clear();state_->order.clear();state_->completed.clear();state_->latest.clear();}
    state_->ready.notify_one();
}
void SentenceWorker::submit(std::uint64_t key,std::uint64_t revision,std::function<SentenceCompletion()> work) {
    {std::lock_guard<std::mutex> lock(state_->mutex);if(state_->closed)return;
     if(!state_->tasks.count(key))state_->order.push_back(key);
     state_->latest[key]=revision;state_->completed.erase(key);state_->tasks.insert_or_assign(key,State::Task{revision,std::move(work)});}
    state_->ready.notify_one();
}
bool SentenceWorker::busy() const {std::lock_guard<std::mutex> lock(state_->mutex);return state_->running || !state_->tasks.empty() || !state_->completed.empty();}
void SentenceWorker::cancel(std::uint64_t key) {
    std::lock_guard<std::mutex> lock(state_->mutex);state_->tasks.erase(key);state_->latest.erase(key);state_->completed.erase(key);
    auto& order=state_->order;order.erase(std::remove(order.begin(),order.end(),key),order.end());
}
std::vector<SentenceCompletion> SentenceWorker::take() {
    std::lock_guard<std::mutex> lock(state_->mutex);std::vector<SentenceCompletion> results;
    for(auto& item:state_->completed)results.push_back(std::move(item.second));state_->completed.clear();return results;
}
DWORD WINAPI SentenceWorker::run(void* parameter) {
    auto launch=static_cast<Launch*>(parameter);HMODULE module=launch->module;
    {
        auto state=std::move(launch->state);delete launch;
        for(;;) {
            std::uint64_t key=0;State::Task task;
            {std::unique_lock<std::mutex> lock(state->mutex);state->ready.wait(lock,[&]{return state->closed || !state->order.empty();});
             if(state->closed)break;key=state->order.front();state->order.pop_front();
             auto found=state->tasks.find(key);if(found==state->tasks.end())continue;task=std::move(found->second);state->tasks.erase(found);state->running=true;}
            SentenceCompletion result;
            try{result=task.work();}catch(const std::exception& e){result.error=e.what();}catch(...){result.error="Sentence worker failed";}
            result.key=key;result.revision=task.revision;
            {std::lock_guard<std::mutex> lock(state->mutex);state->running=false;auto found=state->latest.find(key);
             if(!state->closed && found!=state->latest.end() && found->second==task.revision)state->completed.insert_or_assign(key,std::move(result));}
        }
    }
    // No C++ object with DLL-defined destructors survives this point.
    FreeLibraryAndExitThread(module,0);
}
}
