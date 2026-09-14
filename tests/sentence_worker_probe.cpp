#include "../native/tsf/SentenceWorker.h"
#include <future>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace tiger::tsf;
int wmain() {
 try {
    auto worker=std::make_unique<SentenceWorker>();std::promise<void> entered,release,finished;auto gate=release.get_future().share();
    worker->submit(1,1,[&]{entered.set_value();gate.wait();return SentenceCompletion{};});
    if(entered.get_future().wait_for(std::chrono::seconds(3))!=std::future_status::ready)throw std::runtime_error("worker did not start");
    worker->submit(2,1,[]{throw std::runtime_error("obsolete queued task ran");return SentenceCompletion{};});
    worker->submit(2,2,[]{return SentenceCompletion{};});worker->cancel(1);release.set_value();
    const auto deadline=GetTickCount64()+3000;std::vector<SentenceCompletion> results;
    do {auto r=worker->take();for(auto& c:r)results.push_back(std::move(c));Sleep(1);}while(worker->busy() && GetTickCount64()<deadline);
    for(auto& c:worker->take())results.push_back(std::move(c));
    if(results.size()!=1 || results[0].key!=2 || results[0].revision!=2 || !results[0].error.empty())throw std::runtime_error("coalescing/cancel publication");
    // A superseded in-flight token is cancelled even when a caller reuses the
    // revision number; a cancelled half-result must never be published.
    auto token=std::make_shared<std::atomic<bool>>(false);std::promise<void> inFlight;
    worker->submit(90,1,[&]{inFlight.set_value();while(!token->load())Sleep(1);return SentenceCompletion{};},token);
    if(inFlight.get_future().wait_for(std::chrono::seconds(3))!=std::future_status::ready)throw std::runtime_error("cancellable work did not start");
    worker->submit(90,1,[]{SentenceCompletion c;c.result.rawCode=u"new";return c;},std::make_shared<std::atomic<bool>>(false));
    const auto cancelDeadline=GetTickCount64()+3000;results.clear();
    do{for(auto& c:worker->take())results.push_back(std::move(c));Sleep(1);}while(worker->busy() && GetTickCount64()<cancelDeadline);
    for(auto& c:worker->take())results.push_back(std::move(c));
    if(!token->load() || results.size()!=1 || results.front().result.rawCode!=u"new")throw std::runtime_error("in-flight cancellation publication");
    std::promise<void> started,exit;auto exitGate=exit.get_future().share();auto done=finished.get_future();
    worker->submit(3,1,[&]{started.set_value();exitGate.wait();finished.set_value();return SentenceCompletion{};});
    started.get_future().wait();
    std::atomic<int> persisted{0};std::promise<void> drained;
    if(!worker->submitConfirmed([&]{++persisted;}) || !worker->submitConfirmed([&]{++persisted;drained.set_value();}))throw std::runtime_error("confirmed write rejected");
    auto before=GetTickCount64();worker.reset();
    if(GetTickCount64()-before>200)throw std::runtime_error("teardown blocked on worker");exit.set_value();
    if(done.wait_for(std::chrono::seconds(3))!=std::future_status::ready)throw std::runtime_error("detached worker did not finish");
    if(drained.get_future().wait_for(std::chrono::seconds(3))!=std::future_status::ready || persisted!=2)throw std::runtime_error("confirmed writes cancelled during teardown");
    Sleep(20);std::cout<<"{\"status\":\"passed\",\"coalescing\":true,\"cancel\":true,\"nonblocking_close\":true}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
