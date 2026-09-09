#define NOMINMAX
#include "FontChooser.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <commctrl.h>
namespace {
void checked(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Cannot prepare font preview");}
bool equal(std::wstring_view a,std::wstring_view b) {
    return CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;
}
std::wstring name(IDWriteLocalizedStrings* names,const wchar_t* locale) {
    UINT32 index=0;BOOL found=FALSE;checked(names->FindLocaleName(locale,&index,&found));
    if(!found)return {};
    UINT32 length=0;checked(names->GetStringLength(index,&length));std::wstring value(length+1,L'\0');
    checked(names->GetString(index,value.data(),length+1));value.resize(length);return value;
}
std::wstring firstName(IDWriteLocalizedStrings* names) {
    if(!names->GetCount())return {};
    UINT32 length=0;checked(names->GetStringLength(0,&length));std::wstring value(length+1,L'\0');
    checked(names->GetString(0,value.data(),length+1));value.resize(length);return value;
}
D2D1_COLOR_F color(int role) {
    const auto c=GetSysColor(role);return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f);
}
}
FontChooser::FontChooser(const std::filesystem::path& directory,double size):size_(std::clamp(size,3.,200.)) {
    checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(writing_.GetAddressOf())));
    checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawing_.GetAddressOf()));
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> builder;checked(writing_->CreateFontSetBuilder(&builder));
    std::error_code error;
    for(std::filesystem::directory_iterator it(directory,error),end;!error && it!=end;it.increment(error)) {
        if(!equal(it->path().extension().wstring(),L".ttf"))continue;
        Microsoft::WRL::ComPtr<IDWriteFontFile> file;
        if(FAILED(writing_->CreateFontFileReference(it->path().c_str(),nullptr,&file)))continue;
        BOOL supported=FALSE;DWRITE_FONT_FILE_TYPE type{};DWRITE_FONT_FACE_TYPE faceType{};UINT32 faces=0;
        if(FAILED(file->Analyze(&supported,&type,&faceType,&faces)) || !supported)continue;
        for(UINT32 face=0;face<faces;++face) {
            Microsoft::WRL::ComPtr<IDWriteFontFaceReference> reference;
            if(SUCCEEDED(writing_->CreateFontFaceReference(it->path().c_str(),nullptr,face,DWRITE_FONT_SIMULATIONS_NONE,&reference)))
                checked(builder->AddFontFaceReference(reference.Get()));
        }
    }
    Microsoft::WRL::ComPtr<IDWriteFontSet> set;checked(builder->CreateFontSet(&set));
    checked(writing_->CreateFontCollectionFromFontSet(set.Get(),&bundled_));
    auto collect=[&](IDWriteFontCollection* collection,bool bundled) {
        std::vector<Item> result;
        for(UINT32 i=0;i<collection->GetFontFamilyCount();++i) {
            Microsoft::WRL::ComPtr<IDWriteFontFamily> family;checked(collection->GetFontFamily(i,&family));
            Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;checked(family->GetFamilyNames(&names));
            auto zh=name(names.Get(),L"zh-cn"),en=name(names.Get(),L"en-us");
            auto display=!zh.empty()?zh:en;
            if(display.empty()){if(bundled)continue;display=firstName(names.Get());}
            if(display.empty())continue;
            auto canonical=!en.empty()?en:display;
            Item item{bundled?L"#"+display:display,canonical,{},bundled};
            item.aliases.push_back(bundled?L"#"+canonical:canonical);
            result.push_back(std::move(item));
        }
        if(!bundled)std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
            return CompareStringOrdinal(a.family.c_str(),-1,b.family.c_str(),-1,TRUE)==CSTR_LESS_THAN;
        });
        for(auto& item:result)if(std::none_of(items_.begin(),items_.end(),[&](const auto& prior){return equal(prior.label,item.label);}))items_.push_back(std::move(item));
    };
    collect(bundled_.Get(),true);
    Microsoft::WRL::ComPtr<IDWriteFontCollection> system;checked(writing_->GetSystemFontCollection(&system,FALSE));collect(system.Get(),false);
    if(items_.empty())throw std::runtime_error("No fonts available");
}
void FontChooser::attach(HWND combo,std::wstring_view initial) {
    combo_=combo;SendMessageW(combo_,CB_RESETCONTENT,0,0);
    for(const auto& item:items_)if(SendMessageW(combo_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(item.label.c_str()))<0)throw std::runtime_error("Cannot populate font list");
    if(!select(initial))SendMessageW(combo_,CB_SETCURSEL,0,0);
    SendMessageW(combo_,CB_SETMINVISIBLE,10,0);scale(GetDpiForWindow(combo_));
}
bool FontChooser::select(std::wstring_view value) {
    for(std::size_t i=0;i<items_.size();++i) {
        const auto& item=items_[i];
        if(equal(item.label,value) || std::any_of(item.aliases.begin(),item.aliases.end(),[&](const auto& alias){return equal(alias,value);})) {
            SendMessageW(combo_,CB_SETCURSEL,i,0);InvalidateRect(combo_,nullptr,TRUE);return true;
        }
    }
    return false;
}
std::wstring FontChooser::selected() const {
    const auto index=SendMessageW(combo_,CB_GETCURSEL,0,0);
    return index>=0 && static_cast<std::size_t>(index)<items_.size()?items_[static_cast<std::size_t>(index)].label:L"";
}
void FontChooser::scale(UINT dpi) {
    dpi_=dpi;
    if(combo_) {
        SendMessageW(combo_,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),MulDiv(22,static_cast<int>(dpi),96));
        SendMessageW(combo_,CB_SETITEMHEIGHT,0,static_cast<LPARAM>(std::ceil((size_*1.5+6)*dpi/96.)));
        InvalidateRect(combo_,nullptr,TRUE);
    }
}
void FontChooser::measure(MEASUREITEMSTRUCT& item) const {item.itemHeight=static_cast<UINT>(std::ceil((size_*1.5+6)*dpi_/96.));}
void FontChooser::draw(const DRAWITEMSTRUCT& item) {
    if(item.itemID==UINT_MAX || item.itemID>=items_.size())return;
    const auto& font=items_[item.itemID];
    if(!target_) {
        const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));
        checked(drawing_->CreateDCRenderTarget(&properties,&target_));
    }
    checked(target_->BindDC(item.hDC,&item.rcItem));target_->SetDpi(static_cast<float>(dpi_),static_cast<float>(dpi_));
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    checked(writing_->CreateTextFormat(font.family.c_str(),font.bundled?bundled_.Get():nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,static_cast<float>(size_),L"zh-CN",&format));
    checked(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));checked(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    const bool selected=(item.itemState&ODS_SELECTED)!=0;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;checked(target_->CreateSolidColorBrush(color(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT),&brush));
    const float width=(item.rcItem.right-item.rcItem.left)*96.f/dpi_,height=(item.rcItem.bottom-item.rcItem.top)*96.f/dpi_;
    target_->BeginDraw();target_->Clear(color(selected?COLOR_HIGHLIGHT:COLOR_WINDOW));
    target_->DrawTextW(font.label.c_str(),static_cast<UINT32>(font.label.size()),format.Get(),D2D1::RectF(6,0,std::max(6.f,width-4),height),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    const auto hr=target_->EndDraw();if(hr==D2DERR_RECREATE_TARGET)target_.Reset();else checked(hr);
    if(item.itemState&ODS_FOCUS){RECT focus=item.rcItem;DrawFocusRect(item.hDC,&focus);}
}
