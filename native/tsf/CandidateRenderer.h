#pragma once
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include "../CandidatePresentation.h"
#include "../CandidateTheme.h"
namespace tiger::tsf {
// Layout uses DIPs; the returned premultiplied BGRA surface uses physical pixels.
class CandidateRenderer final {
public:
    CandidateRenderer(const CandidateStyle&,const std::vector<std::filesystem::path>&);
    void layout(const CandidatePresentation&,float maxWidthDip);
    void render(UINT dpi,UINT selection,std::vector<std::uint32_t>& pixels);
    const std::vector<D2D1_RECT_F>& items() const { return items_; }
    float width() const { return width_; }
    float height() const { return height_; }
    UINT pixelWidth(UINT dpi) const;
    UINT pixelHeight(UINT dpi) const;
    bool privateFamily() const { return privateFamily_; }
private:
    using Layout=Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    Layout makeLayout(std::u16string_view,float,float);
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> outline(float inset) const;
    Microsoft::WRL::ComPtr<ID2D1Factory> drawing_;
    Microsoft::WRL::ComPtr<IDWriteFactory3> writing_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> imaging_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    Microsoft::WRL::ComPtr<IWICBitmap> surface_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    std::vector<Layout> layouts_;
    std::vector<D2D1_RECT_F> rectangles_,items_;
    CandidateStyle style_;
    float width_=1,height_=1;
    UINT surfaceWidth_=0,surfaceHeight_=0;
    bool privateFamily_=false;
};
}
