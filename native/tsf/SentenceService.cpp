#include "Service.h"
#include "ManualTimer.h"
#include "../Settings.h"
#include "../SchemaCatalog.h"
#include "../Grapheme.h"
#include "../SentenceCache.h"
#include <algorithm>
namespace tiger::tsf {
void Service::refreshSentenceResources(std::u16string_view settings) {
    sentenceSettings_=parseSentenceSettings(settings);
    const bool wanted=!secure_ && lexicon_ &&
        sentenceSettings_.activeForSchema(schema_);
    if(!wanted) {
        sentenceLearningStore_.reset();sentenceLearningMode_.clear();
        if(sentenceResources_ || !sentenceSignature_.empty()){sentenceResources_.reset();sentenceSignature_.clear();++sentenceRevision_;dataDirty_=true;}
        sentenceRequestedSource_.reset();sentenceLoadedSource_.reset();
        if(sentenceWorker_)sentenceWorker_->cancel(0);return;
    }
    auto path=activeSchemaDictionaryPath(userRoot_,dictionaryPath_,schema_);
    auto model=sentenceSettings_.modelPath.empty()?dictionaryPath_.parent_path()/L"Models"/L"sentence-ngram-v2.bin":std::filesystem::path(sentenceSettings_.modelPath);
    const int common=sentenceSettings_.commonCharacterLimit;
    SentenceDecoderOptions options;options.emittedCharacterReward=2;options.wholeInputSingleCharacterReward=5;
    options.canonicalCodeReward=2;options.canonicalIsolationFactor=0;options.canonicalIsolationMinCodeLength=4;
    options.lexicalPriorWeight=.1;options.lexicalCandidateLimit=5;
    options.allowDuplicateSingleCharacters=sentenceSettings_.allowDuplicateSingleCharacters;
    options.preserveTruncatedEarlyCommitEvidence=true;
    const auto number=std::to_string(common);
    const auto signature=path.u16string()+u"\n"+model.u16string()+u"\n"+sentenceSettings_.fullCodeWhitelist+u"\n"+
        std::u16string(number.begin(),number.end())+(options.allowDuplicateSingleCharacters?u"1":u"0")+
        (sentenceSettings_.autoCommit?u"1":u"0")+(sentenceSettings_.selfLearning?u"1":u"0");
    if(signature==sentenceSignature_ && sentenceRequestedSource_ &&
       (sentenceRequestedSource_==lexicon_ || sentenceRequestedSource_->equivalent(*lexicon_)))return;
    if(!sentenceWorker_)sentenceWorker_=std::make_unique<SentenceWorker>();
    sentenceSignature_=signature;const auto revision=++sentenceRevision_;dataDirty_=true;
    // Keep the last complete snapshot usable until its replacement is ready,
    // including base-generation and sentence-schema changes. The disabled-schema
    // path above still clears resources immediately. Never mix a new ordinary
    // lexicon with an old sentence index while preparation is pending.
    sentenceRequestedSource_=lexicon_;
    auto source=lexicon_;auto journal=schemaJournalPath(userRoot_,schema_);
    auto helper=dictionaryPath_.parent_path()/L"Tigirl.Import.exe";auto cache=userRoot_/L"cache"/L"sentence";
    auto whitelist=sentenceSettings_.whitelist();
    auto learningPath=userRoot_/L"码表"/std::filesystem::path(schema_)/L".tigirl-learning-v1.log";
    auto learningMode=std::u16string(u"sentence-v1|dup=")+(options.allowDuplicateSingleCharacters?u"1":u"0")+
        u"|optimal="+std::u16string(number.begin(),number.end())+u"|whitelist="+learningConfigurationHash(sentenceSettings_.fullCodeWhitelist);
    const bool learningEnabled=sentenceSettings_.selfLearning;
    sentenceWorker_->submit(0,revision,[path,model,common,whitelist=std::move(whitelist),options,source,journal,helper,cache,learningPath,learningMode,learningEnabled] {
        SentenceCompletion result;result.source=source;
        if(source->editedCodes()) {
            auto prepared=prepareSentenceCache(helper,path,journal,cache,*source);
            result.resources=SentenceResources::Open(path,model,common,whitelist,options,prepared.path,prepared.revision);
        }else result.resources=SentenceResources::Open(path,model,common,whitelist,options);
        if(learningEnabled) {
            result.learningMode=learningMode;result.learningStore=std::make_shared<SentenceLearningStore>(learningPath);
            // Corrupt/unreadable learning data must not disable sentence input.
            try{result.learningStore->refresh();}catch(const std::exception&){}
        }
        return result;
    });
    if(sentenceTimer_)sentenceTimer_->schedule(10);
}
void Service::queueSentence(const std::shared_ptr<Context>& context) {
    auto request=context->engine.sentenceRequest();
    if(!request) {
        if(sentenceWorker_ && context->sentenceQueuedIdentity)sentenceWorker_->cancel(context->sentenceQueuedIdentity);
        context->sentenceDecoder.reset();context->sentenceQueuedIdentity=0;return;
    }
    if(!active_ || !sentenceWorker_ || !sentenceResources_)return;
    if(context->sentenceResourceRevision!=sentenceLoadedRevision_ || !context->sentenceDecoder) {
        context->sentenceDecoder=sentenceResources_->createDecoder();context->sentenceResourceRevision=sentenceLoadedRevision_;
        context->sentenceQueuedIdentity=0;
    }
    if(context->sentenceQueuedIdentity==request->session && context->sentenceQueuedGeneration==request->generation)return;
    context->sentenceQueuedIdentity=request->session;context->sentenceQueuedGeneration=request->generation;
    auto decoder=context->sentenceDecoder;auto ticket=*request;const bool evidence=sentenceSettings_.autoCommit;
    auto learning=sentenceLearningStore_;auto mode=sentenceLearningMode_;
    auto cancellation=std::make_shared<std::atomic<bool>>(false);
    sentenceWorker_->submit(ticket.session,ticket.generation,[decoder,ticket,evidence,learning,mode,cancellation] {
        SentenceCompletion result;result.ticket=ticket;
        if(learning)try{learning->refresh();}catch(const std::exception& e){result.error=e.what();}
        decoder->setLearning(learning?learning->snapshot():nullptr,mode);
        try{result.result=decoder->decode(ticket.raw,20,evidence,ticket.requiredPrefix,ticket.lockedPrefix,cancellation);
            decoder->retainCommittedHistory(ticket.raw,ticket.committedRaw);}
        catch(const SentenceDecodeCancelled&){return result;}
        catch(const std::exception& e){result.error=e.what();}
        return result;
    },cancellation);
    if(sentenceTimer_)sentenceTimer_->schedule(10);
}
void Service::completeSentenceNow(const std::shared_ptr<Context>& context,Engine& next) {
    auto ticket=next.sentenceRequest();if(!ticket || !sentenceResources_)return;
    if(context->sentenceResourceRevision!=sentenceLoadedRevision_ || !context->sentenceDecoder) {
        context->sentenceDecoder=sentenceResources_->createDecoder();context->sentenceResourceRevision=sentenceLoadedRevision_;
        context->sentenceQueuedIdentity=0;
    }
    // A synchronous selection must not wait for obsolete long-sentence work.
    // Confirmed learning writes use their separate FIFO and are never cancelled.
    if(sentenceWorker_)sentenceWorker_->cancel(ticket->session);
    context->sentenceQueuedIdentity=0;
    SentenceDecodeResult result;
    context->sentenceDecoder->setLearning(sentenceLearningStore_?sentenceLearningStore_->snapshot():nullptr,sentenceLearningMode_);
    try{result=context->sentenceDecoder->decode(ticket->raw,20,sentenceSettings_.autoCommit,ticket->requiredPrefix,ticket->lockedPrefix);
        context->sentenceDecoder->retainCommittedHistory(ticket->raw,ticket->committedRaw);}
    catch(const std::exception& e){report(e.what());}
    next.applySentenceResult(*ticket,std::move(result));
}
void Service::pollSentence() {
    if(!active_ || !sentenceWorker_)return;
    if(keyDepth_){if(sentenceTimer_)sentenceTimer_->schedule(10);return;}
    for(auto& completion:sentenceWorker_->take()) {
        if(completion.key==UINT64_MAX){if(!completion.error.empty())report(completion.error.c_str());continue;}
        if(completion.key==0) {
            pollDataChanges();
            if(completion.revision!=sentenceRevision_)continue;
            if(!completion.error.empty()){report(completion.error.c_str());sentenceSignature_.clear();continue;}
            if(completion.resources && (!completion.source || !lexicon_ || !completion.source->equivalent(*lexicon_))) {
                sentenceSignature_.clear();continue;
            }
            sentenceLoadedSource_=completion.source;
            sentenceLoadedRevision_=completion.revision;
            sentenceLearningStore_=std::move(completion.learningStore);sentenceLearningMode_=std::move(completion.learningMode);
            sentenceResources_=std::move(completion.resources);dataDirty_=true;pollDataChanges();continue;
        }
        for(auto& pair:contexts_) {
            auto current=pair.second;auto ticket=current->engine.sentenceRequest();
            if(!ticket || ticket->session!=completion.ticket.session)continue;
            if(!foreground_ || focused_.Get()!=current->context.Get())break;
            if(!completion.error.empty())report(completion.error.c_str());
            edit(current,TF_ES_ASYNCDONTCARE|TF_ES_READWRITE,[this,current,completion=std::move(completion)](TfEditCookie cookie) mutable {
                if(!active_ || !foreground_ || focused_.Get()!=current->context.Get() || state(current->context.Get(),false)!=current)return S_FALSE;
                auto next=current->engine;synchronizeEngine(next);
                if(!next.applySentenceResult(completion.ticket,std::move(completion.result)))return S_FALSE;
                // Original asynchronous completion only publishes candidates.
                // The next appended key consumes mature prefix evidence.
                return apply(current,std::move(next),KeyResult{},cookie);
            });
            break;
        }
    }
    if(sentenceTimer_ && sentenceWorker_->busy())sentenceTimer_->schedule(10);
}
}
