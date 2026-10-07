#pragma once
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <array>
#include <memory>
#include "../CandidatePresentation.h"
#include "../CandidateTheme.h"
#include "SsfResources.h"
namespace tiger::tsf {
// Layout uses DIPs; the returned premultiplied BGRA surface uses physical pixels.
class CandidateRenderer final {
public:
    CandidateRenderer(const CandidateStyle&,const std::vector<std::filesystem::path>&,const std::filesystem::path& skinRoot={},int skinPart=0);
    bool skinFrameChanged() const;
    UINT skinDelay() const;
    std::u16string skinStatus() const;
    bool imageSkin() const { return skinImage_!=nullptr || (codePane_ && (codePane_->imageSkin() || candidatePane_->imageSkin())); }
    void layout(const CandidatePresentation&,float maxWidthDip);
    void render(UINT dpi,UINT selection,std::vector<std::uint32_t>& pixels,const SIZE* frameSize=nullptr);
    const std::vector<D2D1_RECT_F>& items() const { return scale_==1.f?items_:scaledItems_; }
    float width() const { return width_*scale_; }
    float height() const { return height_*scale_; }
    UINT pixelWidth(UINT dpi) const;
    UINT pixelHeight(UINT dpi) const;
    bool privateFamily() const { return privateFamily_; }
private:
    int skinPart_=0;
    std::unique_ptr<CandidateRenderer> codePane_,candidatePane_;
    bool hasCodePane_=false,hasCandidatePane_=false;
    float splitOffset_=0;
    using Layout=Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    void layoutSkin(const CandidatePresentation&,float);
    void drawSkinBackground(float,float);
    void prepareSkinBitmap();
    std::shared_ptr<const skin::Resources> skinResources_;
    const skin::Layout* skinLayout_=nullptr;
    const skin::Image* skinImage_=nullptr;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> skinBitmap_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> annotationBrush_;
    std::vector<std::uint32_t> inks_;
    std::vector<DWRITE_TEXT_RANGE> annotations_;
    std::uint64_t skinStarted_=0;
    std::size_t skinBitmapFrame_=static_cast<std::size_t>(-1),skinRenderedFrame_=static_cast<std::size_t>(-1);
    Layout makeLayout(std::u16string_view,float,float);
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> outline(float inset,float width,float height) const;
    Microsoft::WRL::ComPtr<ID2D1Factory> drawing_;
    Microsoft::WRL::ComPtr<IDWriteFactory3> writing_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> imaging_;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    Microsoft::WRL::ComPtr<IWICBitmap> surface_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<ID2D1Layer> clip_;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> shape_,border_;
    std::vector<Layout> layouts_;
    std::vector<D2D1_RECT_F> rectangles_,items_,scaledItems_;
    CandidateStyle style_;
    float width_=1,height_=1,scale_=1;
    UINT surfaceWidth_=0,surfaceHeight_=0;
    float geometryWidth_=-1,geometryHeight_=-1;
    std::array<float,8> spaceWidths_{};
    std::array<bool,8> spaceMeasured_{};
    bool privateFamily_=false;
};
}
