#pragma once
#include <windows.h>
#include <dwrite_3.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <vector>
class FontChooser {
public:
    struct Item {
        std::wstring label,family;
        std::vector<std::wstring> aliases;
        bool bundled=false;
    };
    FontChooser(const std::filesystem::path& directory,double size);
    void attach(HWND combo,std::wstring_view initial);
    bool select(std::wstring_view value);
    std::wstring selected() const;
    void scale(UINT dpi);
    void draw(const DRAWITEMSTRUCT& item);
    void measure(MEASUREITEMSTRUCT& item) const;
    const std::vector<Item>& items() const {return items_;}
    double previewSize() const {return size_;}
private:
    std::vector<Item> items_;
    Microsoft::WRL::ComPtr<IDWriteFactory3> writing_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> bundled_;
    Microsoft::WRL::ComPtr<ID2D1Factory> drawing_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
    HWND combo_=nullptr;
    UINT dpi_=96;
    double size_=14;
};
