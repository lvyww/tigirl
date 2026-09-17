from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
user_store = (ROOT / "native/UserStore.cpp").read_text(encoding="utf-8")
ngram = (ROOT / "native/SentenceNgram.cpp").read_text(encoding="utf-8")
cache = (ROOT / "native/SentenceCache.h").read_text(encoding="utf-8")
registration = (ROOT / "SampleIME/Register.cpp").read_text(encoding="utf-8")


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


require("cache_->lexicon && cache_->stamp==observed" in user_store,
        "unchanged UserStore refresh still replays its journal")
require("cache_->stamp=file.stamp()" in user_store,
        "successful user-store commits do not refresh the cache identity")
require("!std::isfinite(score) || score<0" in ngram and "key<=previous" in ngram,
        "n-gram model does not reject non-finite/negative or unsorted records")
require("TerminateProcess(child.value,ERROR_TIMEOUT)" in cache and
        "WaitForSingleObject(child.value,5000)" in cache,
        "timed-out sentence helpers can survive the parent wait")
require("maximumRevisions=8" in cache and "LOCKFILE_FAIL_IMMEDIATELY" in cache and
        "remove_all(candidate.path" in cache,
        "sentence revision cache does not have bounded best-effort GC")

start = registration.index("BOOL RegisterCategories()")
end = registration.index("void UnregisterCategories()")
block = registration[start:end]
require("if (FAILED(hr))" in block and "return FALSE" in block,
        "RegisterCategories can still hide an earlier category failure")

print("PASS: review hardening policies are present.")
