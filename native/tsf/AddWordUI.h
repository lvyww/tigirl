#pragma once
#include <windows.h>
#include <functional>
#include "../AddWord.h"
#include "../UserStore.h"
namespace tiger::tsf {
// Posted requests open after the TSF edit session unwinds. The modal dialog
// supplies its own keyboard navigation loop; it never runs inside a text lock.
class AddWordUI final : public std::enable_shared_from_this<AddWordUI> {
public:
    static std::shared_ptr<AddWordUI> create(HINSTANCE module,UserStore store,std::shared_ptr<const Dictionary> dictionary,
        std::function<void(std::shared_ptr<const Lexicon>)> saved);
    ~AddWordUI();
    void request(std::u16string history);
    void request(std::vector<std::u16string> history);
    void close();
private:
    AddWordUI(HINSTANCE module,UserStore store,std::shared_ptr<const Dictionary> dictionary,
        std::function<void(std::shared_ptr<const Lexicon>)> saved);
    static LRESULT CALLBACK dispatch(HWND,UINT,WPARAM,LPARAM);
    static INT_PTR CALLBACK dialog(HWND,UINT,WPARAM,LPARAM);
    void initialize();
    void history(int change);
    void changed();
    void save(bool keep);
    bool queueRequest();
    HINSTANCE module_;
    HWND queue_=nullptr,dialog_=nullptr;
    HFONT font_=nullptr;
    UserStore store_;
    std::shared_ptr<const Dictionary> dictionary_;
    std::function<void(std::shared_ptr<const Lexicon>)> saved_;
    std::wstring className_;
    std::vector<std::u16string> history_;
    std::u16string pendingHistory_;
    std::vector<std::u16string> pendingElements_;
    std::size_t historyLength_=2;
    bool pending_=false,closed_=false;
};
}
