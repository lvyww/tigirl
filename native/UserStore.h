#pragma once
#include "Lexicon.h"

namespace tiger {
// Editable UTF-8 TSV operations in 码表/<schema>/用户调整.txt. Refresh at activation/focus or a file
// notification, never for every input key. Commit rebases on the latest journal
// under an OS file lock, so simultaneous hosts cannot overwrite each other.
// The stable .lock sidecar is acquired before the journal lock. Never remove
// the sidecar while a user store can be active. Windows publication additionally
// excludes open journal handles using sharing modes.
class UserStore final {
public:
    UserStore(std::shared_ptr<const Dictionary> dictionary, std::filesystem::path journal);
    std::shared_ptr<const Lexicon> refresh() const;
    std::shared_ptr<const Lexicon> commit(const std::vector<UserChange>& changes) const;
    // Produce an equivalent editable text image under the journal
    // lock. Does not replace the live file: callers need a separate publication
    // protocol that also coordinates existing readers and writers.
    std::vector<unsigned char> checkpoint() const;
#ifdef _WIN32
    // Explicit maintenance only. False means busy or no size reduction.
    // Preserves an old-journal backup on successful publication; on failure
    // leaves recovery artifacts and reports their path. Not called per key.
    bool compact() const;
    // Restore a selected journal-scoped backup only when the live path is
    // absent. Never replaces an existing/recreated live journal or the backup.
    bool restoreCheckpoint(const std::filesystem::path& backup) const;
#endif
private:
    struct Cache;
    std::vector<unsigned char> makeCheckpoint(std::vector<unsigned char> original) const;
    static std::shared_ptr<Lexicon> decode(const std::vector<unsigned char>& bytes,
        std::shared_ptr<const Dictionary> dictionary,std::size_t& validLength);
    std::shared_ptr<const Dictionary> dictionary_;
    std::filesystem::path journal_;
    std::shared_ptr<Cache> cache_;
};
}
