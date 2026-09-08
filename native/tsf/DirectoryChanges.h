#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>

namespace tiger::tsf {
// A level-triggered invalidation signal, not a stream of individual edits.
// The owning TSF thread can poll without blocking, then reload authoritative
// files. Rearm before reloading so writes during the reload remain observable.
// This object owns no worker thread and never calls TSF from another apartment.
class DirectoryChanges final {
    HANDLE handle_=INVALID_HANDLE_VALUE;
public:
    DirectoryChanges()=default;
    DirectoryChanges(const DirectoryChanges&)=delete;
    DirectoryChanges& operator=(const DirectoryChanges&)=delete;
    ~DirectoryChanges() { close(); }
    void close() noexcept {
        if(handle_!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(handle_);
        handle_=INVALID_HANDLE_VALUE;
    }
    HRESULT open(const std::filesystem::path& directory) {
        close();
        if(!directory.is_absolute())return E_INVALIDARG;
        handle_=FindFirstChangeNotificationW(directory.c_str(),TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|
            FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE);
        return handle_==INVALID_HANDLE_VALUE?HRESULT_FROM_WIN32(GetLastError()):S_OK;
    }
    HRESULT poll(bool& changed) noexcept {
        changed=false;
        if(handle_==INVALID_HANDLE_VALUE)return E_HANDLE;
        const auto result=WaitForSingleObject(handle_,0);
        if(result==WAIT_TIMEOUT)return S_OK;
        if(result!=WAIT_OBJECT_0) {
            const auto error=result==WAIT_FAILED?HRESULT_FROM_WIN32(GetLastError()):E_UNEXPECTED;
            close();return error;
        }
        if(!FindNextChangeNotification(handle_)) {
            const auto error=HRESULT_FROM_WIN32(GetLastError());
            close();return error;
        }
        changed=true;return S_OK;
    }
};
}
