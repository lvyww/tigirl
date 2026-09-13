#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../SentenceResources.h"
#include "../SentenceSession.h"
#include "../SentenceLearningStore.h"
#include "../Lexicon.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace tiger::tsf {
struct SentenceCompletion {
    std::uint64_t key=0,revision=0;
    std::shared_ptr<const SentenceResources> resources;
    std::shared_ptr<const Lexicon> source;
    std::shared_ptr<SentenceLearningStore> learningStore;
    std::u16string learningMode;
    SentenceDecodeTicket ticket;
    SentenceDecodeResult result;
    std::string error;
};
// One serial worker per service. Tasks capture native values/shared resources,
// never COM interfaces, HWNDs or Service pointers. Latest queued task wins per
// key. Destruction cancels publication without blocking the TSF apartment.
class SentenceWorker {
public:
    SentenceWorker();
    ~SentenceWorker();
    void submit(std::uint64_t key,std::uint64_t revision,std::function<SentenceCompletion()> work);
    void cancel(std::uint64_t key);
    // Confirmed document writes are FIFO, not latest-wins. Accepted writes drain
    // before the retained worker DLL is released, even when the service closes.
    bool submitConfirmed(std::function<void()> work);
    bool busy() const;
    std::vector<SentenceCompletion> take();
private:
    struct State;
    struct Launch;
    static DWORD WINAPI run(void*);
    std::shared_ptr<State> state_;
};
}
