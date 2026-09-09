#define NOMINMAX
#include "CandidateRenderer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace tiger::tsf {
namespace {
void checked(HRESULT hr) { if(FAILED(hr)) throw hr; }
std::wstring wide(std::u16string_view s) { return {reinterpret_cast<const wchar_t*>(s.data()),s.size()}; }
D2D1_COLOR_F color(std::uint32_t c) { return D2D1::ColorF((c>>16&255)/255.f,(c>>8&255)/255.f,(c&255)/255.f,(c>>24)/255.f); }
}
CandidateRenderer::CandidateRenderer(const CandidateStyle& style,const std::vector<std::filesystem::path>& files):style_(style) {
    checked(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawing_.GetAddressOf()));
    checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(writing_.GetAddressOf())));
    checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&imaging_)));
    Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> builder;checked(writing_->CreateFontSetBuilder(&builder));
    for(const auto& path:files) {
        Microsoft::WRL::ComPtr<IDWriteFontFile> file;checked(writing_->CreateFontFileReference(path.c_str(),nullptr,&file));
        BOOL supported=FALSE;DWRITE_FONT_FILE_TYPE fileType{};DWRITE_FONT_FACE_TYPE faceType{};UINT32 faces=0;
        checked(file->Analyze(&supported,&fileType,&faceType,&faces));
        if(!supported)continue;
        for(UINT32 face=0;face<faces;++face) {
            Microsoft::WRL::ComPtr<IDWriteFontFaceReference> reference;
            checked(writing_->CreateFontFaceReference(path.c_str(),nullptr,face,DWRITE_FONT_SIMULATIONS_NONE,&reference));
            checked(builder->AddFontFaceReference(reference.Get()));
        }
    }
    Microsoft::WRL::ComPtr<IDWriteFontSet> set;checked(builder->CreateFontSet(&set));
    checked(writing_->CreateFontCollectionFromFontSet(set.Get(),&collection_));
    auto family=wide(style.font);if(!family.empty() && family.front()==L'#')family.erase(0,1);
    if(family.empty())family=L"Microsoft YaHei UI";
    UINT32 index=0;BOOL exists=FALSE;checked(collection_->FindFamilyName(family.c_str(),&index,&exists));privateFamily_=exists!=FALSE;
    checked(writing_->CreateTextFormat(family.c_str(),privateFamily_?collection_.Get():nullptr,DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,static_cast<float>(style.fontSize),L"zh-CN",&format_));
    checked(format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    checked(format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;checked(writing_->CreateEllipsisTrimmingSign(format_.Get(),&ellipsis));
    checked(format_->SetTrimming(&trimming,ellipsis.Get()));
}
CandidateRenderer::Layout CandidateRenderer::makeLayout(std::u16string_view text,float width,float height) {
    Layout result;const auto value=wide(text);
    checked(writing_->CreateTextLayout(value.data(),static_cast<UINT32>(value.size()),format_.Get(),width,height,&result));
    return result;
}
void CandidateRenderer::layout(const CandidatePresentation& presentation,float maxWidth) {
    maxWidth=std::max(1.f,maxWidth);
    const float size=static_cast<float>(style_.fontSize);
    const bool codeOnly=presentation.codeOnly;
    const float left=codeOnly?static_cast<float>(std::nearbyint(size*.4f)):(style_.vertical?12.f:8.f);
    const float rightPadding=codeOnly?left:8.f,top=codeOnly?2.f:8.f,bottom=codeOnly?2.f:(style_.vertical?7.f:8.f);
    const float border=static_cast<float>(candidateTheme(style_.theme).borderWidth);
    const float row=std::ceil(size*(!codeOnly && style_.vertical?1.5f:1.f));
    const float minimum=codeOnly?0.f:std::ceil(size*(style_.vertical?3.76f:2.88f)+15.f);
    const auto measure=[&](std::u16string_view text) {
        auto layout=makeLayout(text,100000,row);DWRITE_TEXT_METRICS m{};checked(layout->GetMetrics(&m));return m.widthIncludingTrailingWhitespace;
    };
    float x=left+border,y=top+border,right=x;
    layouts_.clear();rectangles_.clear();items_.clear();
    const auto add=[&](std::u16string_view text,bool item) {
        auto textLayout=makeLayout(text,100000,row);
        DWRITE_TEXT_METRICS metrics{};checked(textLayout->GetMetrics(&metrics));
        const float available=std::max(1.f,maxWidth-rightPadding-border-x);
        const float width=std::min(std::max(1.f,metrics.widthIncludingTrailingWhitespace),available);
        checked(textLayout->SetMaxWidth(width));
        D2D1_RECT_F rect{x,y,x+width,y+row};
        layouts_.push_back(textLayout);rectangles_.push_back(rect);if(item)items_.push_back(rect);
        right=std::max(right,rect.right);
        if(style_.vertical && !codeOnly)y+=row;else x+=width;
    };
    if(!presentation.code.empty()) {
        add(presentation.code,false);
        if(!style_.vertical && !presentation.items.empty())x+=measure(std::u16string(presentation.code.size()<7?7-presentation.code.size():0,u' '));
    }
    for(std::size_t i=0;i<presentation.items.size();++i) {
        if(!style_.vertical && i)x+=measure(u"  ");
        // Original horizontal text does not wrap. Keep off-screen items out of hit testing.
        if(!style_.vertical && x>=maxWidth-rightPadding-border)break;
        add(presentation.items[i],true);
    }
    width_=std::min(maxWidth,std::max(minimum+2*border,right+rightPadding+border));
    height_=y+bottom+border+((style_.vertical && !codeOnly)?0:row);
}
UINT CandidateRenderer::pixelWidth(UINT dpi) const { return static_cast<UINT>(std::max(1.,std::ceil(width_*dpi/96.))); }
UINT CandidateRenderer::pixelHeight(UINT dpi) const { return static_cast<UINT>(std::max(1.,std::ceil(height_*dpi/96.))); }
Microsoft::WRL::ComPtr<ID2D1PathGeometry> CandidateRenderer::outline(float inset) const {
    const auto& theme=candidateTheme(style_.theme);
    const float left=inset,top=inset,right=std::max(inset,width_-inset),bottom=std::max(inset,height_-inset);
    float r[4];for(int i=0;i<4;++i)r[i]=std::clamp(static_cast<float>(theme.corners[i])-inset,0.f,std::max(0.f,std::min(right-left,bottom-top)/2));
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> path;checked(drawing_->CreatePathGeometry(&path));
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;checked(path->Open(&sink));
    sink->BeginFigure(D2D1::Point2F(left+r[0],top),D2D1_FIGURE_BEGIN_FILLED);
    const auto arc=[&](float x,float y,float radius){if(radius>0)sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x,y),D2D1::SizeF(radius,radius),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));else sink->AddLine(D2D1::Point2F(x,y));};
    sink->AddLine(D2D1::Point2F(right-r[1],top));arc(right,top+r[1],r[1]);
    sink->AddLine(D2D1::Point2F(right,bottom-r[2]));arc(right-r[2],bottom,r[2]);
    sink->AddLine(D2D1::Point2F(left+r[3],bottom));arc(left,bottom-r[3],r[3]);
    sink->AddLine(D2D1::Point2F(left,top+r[0]));arc(left+r[0],top,r[0]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);checked(sink->Close());return path;
}
void CandidateRenderer::render(UINT dpi,UINT selected,std::vector<std::uint32_t>& pixels) {
    if(dpi<48 || dpi>960)throw std::invalid_argument("Invalid candidate DPI");
    const UINT width=pixelWidth(dpi),height=pixelHeight(dpi);
    if(static_cast<std::uint64_t>(width)*height>16000000)throw std::length_error("Candidate surface exceeds limit");
    if(!target_ || width!=surfaceWidth_ || height!=surfaceHeight_) {
        target_.Reset();surface_.Reset();
        checked(imaging_->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,&surface_));
        const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),static_cast<float>(dpi),static_cast<float>(dpi));
        checked(drawing_->CreateWicBitmapRenderTarget(surface_.Get(),properties,&target_));surfaceWidth_=width;surfaceHeight_=height;
    }
    target_->SetDpi(static_cast<float>(dpi),static_cast<float>(dpi));
    // Subpixel RGB coverage is unsuitable for per-pixel transparent windows.
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;checked(target_->CreateSolidColorBrush(D2D1::ColorF(0,0.f),&brush));
    const auto& theme=candidateTheme(style_.theme);auto shape=outline(0),border=outline(static_cast<float>(theme.borderWidth)/2);
    Microsoft::WRL::ComPtr<ID2D1Layer> clip;checked(target_->CreateLayer(&clip));
    target_->BeginDraw();target_->Clear(D2D1::ColorF(0,0.f));
    target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),shape.Get()),clip.Get());
    brush->SetColor(color(theme.background));target_->FillRectangle(D2D1::RectF(0,0,width_,height_),brush.Get());
    // The first candidate is the default choice, not a visually highlighted row.
    if(selected>0 && selected<items_.size()){brush->SetColor(color(theme.selection));target_->FillRectangle(items_[selected],brush.Get());}
    brush->SetColor(color(theme.border));target_->DrawGeometry(border.Get(),brush.Get(),static_cast<float>(theme.borderWidth));
    brush->SetColor(color(theme.foreground));
    for(std::size_t i=0;i<layouts_.size();++i)target_->DrawTextLayout(D2D1::Point2F(rectangles_[i].left,rectangles_[i].top),layouts_[i].Get(),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    target_->PopLayer();const auto hr=target_->EndDraw();
    if(FAILED(hr)){target_.Reset();surface_.Reset();checked(hr);}
    pixels.resize(static_cast<std::size_t>(width)*height);
    checked(surface_->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data())));
}
}
