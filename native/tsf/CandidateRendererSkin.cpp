#define NOMINMAX
#include "CandidateRenderer.h"
#include "SkinRasterFilter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tiger::tsf {
namespace {
void check(HRESULT hr){if(FAILED(hr))throw hr;}
}
void CandidateRenderer::layoutSkin(const CandidatePresentation& presentation,float maxWidth){
    maxWidth=std::max(1.f,maxWidth);
    const auto* next=skinResources_->definition.choose(style_.vertical,skinPart_!=2,skinPart_!=1);
    if(!next)throw std::runtime_error("Skin layout unavailable");
    const auto* image=&skinResources_->images.at(next->image);
    if(image!=skinImage_){
        skinBitmap_.Reset();skinBitmapFrame_=skinRenderedFrame_=static_cast<std::size_t>(-1);
        skinRasters_.clear();skinRasterBytes_=0;
    }
    skinLayout_=next;skinImage_=image;
    layouts_.clear();rectangles_.clear();items_.clear();inks_.clear();annotations_.clear();
    const float fontSize=static_cast<float>(style_.fontSize);
    auto sample=makeLayout(u"中Mg",100000,std::ceil(fontSize*2));DWRITE_TEXT_METRICS line{};check(sample->GetMetrics(&line));
    const float row=std::ceil(std::max(fontSize,line.height)),gap=skinResources_->definition.candidateSpacing;
    float right=0,y=0;
    const auto add=[&](std::u16string_view text,float x,float atY,float rightPadding,bool item,std::uint32_t ink,std::uint32_t annotation){
        x=std::clamp(x,0.f,maxWidth-1);rightPadding=std::clamp(rightPadding,0.f,maxWidth-x-1);
        const float available=std::max(1.f,maxWidth-rightPadding-x);
        auto layout=makeLayout(text,100000,row);DWRITE_TEXT_METRICS metrics{};check(layout->GetMetrics(&metrics));
        const float width=std::min(available,std::max(1.f,metrics.widthIncludingTrailingWhitespace));check(layout->SetMaxWidth(width));
        const D2D1_RECT_F rect{x,atY,x+width,atY+row};layouts_.push_back(layout);rectangles_.push_back(rect);if(item)items_.push_back(rect);
        inks_.push_back(ink);annotation=std::min(annotation,static_cast<std::uint32_t>(text.size()));annotations_.push_back({annotation,static_cast<UINT32>(text.size())-annotation});
        right=std::max(right,rect.right+rightPadding);return width;
    };
    const auto& definition=skinResources_->definition;
    if(skinPart_!=2){
        const auto& m=next->code;y=static_cast<float>(m.top);
        if(!presentation.code.empty())add(presentation.code,static_cast<float>(m.left),y,static_cast<float>(m.right),false,definition.codeColor,static_cast<std::uint32_t>(presentation.code.size()));
        y+=row+static_cast<float>(m.bottom);
    }
    if(!presentation.placeholder.empty()){
        const auto& m=next->candidates;y+=static_cast<float>(m.top);
        add(presentation.placeholder,static_cast<float>(m.left),y,static_cast<float>(m.right),false,definition.codeColor,static_cast<std::uint32_t>(presentation.placeholder.size()));
        y+=row+static_cast<float>(m.bottom);
    }
    if(!presentation.items.empty()){
        const auto& m=next->candidates;y+=static_cast<float>(m.top);float x=static_cast<float>(m.left);bool any=false;
        for(std::size_t i=0;i<presentation.items.size();++i){
            if(!style_.vertical && any && x>=maxWidth-static_cast<float>(m.right))break;
            const auto annotation=i<presentation.annotationOffsets.size()?presentation.annotationOffsets[i]:static_cast<std::uint32_t>(presentation.items[i].size());
            const float width=add(presentation.items[i],x,y,static_cast<float>(m.right),true,i==0?definition.firstColor:definition.textColor,annotation);
            any=true;if(style_.vertical){y+=row;if(i+1<presentation.items.size())y+=definition.lineSpacing;}else x+=width+gap;
        }
        if(!style_.vertical && any)y+=row;y+=static_cast<float>(m.bottom);
    }
    if(skinPart_!=1 && presentation.items.empty() && presentation.placeholder.empty()){
        const auto& m=next->candidates;y+=static_cast<float>(m.top)+row+static_cast<float>(m.bottom);
    }
    // Preserve the authored canvas unless the skin opts into smaller bounds.
    // Never squeeze fixed end slices; content plus its insets grows the center.
    // Text coordinates retain the original insets on both axes.
    const unsigned authoredMinimum=next->minimumWidth>0?static_cast<unsigned>(next->minimumWidth):image->width;
    const float minWidth=static_cast<float>(std::max(authoredMinimum,static_cast<unsigned>(std::max(0,next->horizontal.before)+std::max(0,next->horizontal.after)+1)));
    const unsigned authoredHeight=next->minimumHeight>0?static_cast<unsigned>(next->minimumHeight):image->height;
    const float minHeight=static_cast<float>(std::max(authoredHeight,static_cast<unsigned>(std::max(0,next->vertical.before)+std::max(0,next->vertical.after)+1)));
    width_=std::max(1.f,std::min(maxWidth,std::max(minWidth,right)));height_=std::max(minHeight,y);
}
void CandidateRenderer::prepareSkinBitmap(){
    if(!skinImage_)return;
    const auto index=style_.skinAnimation?skin::animationFrame(skinImage_->delays,skinImage_->plays,GetTickCount64()-skinStarted_).index:0;
    float dpiX=96,dpiY=96;target_->GetDpi(&dpiX,&dpiY);
    // D2D 1.0's bilinear sampler only sees four texels, even for a 4:1 or
    // greater reduction. Profile E prefilters at 1.5 times the user's physical
    // font-scale/DPI resolution, capped at the original image dimensions.
    // Keep source pixels for small reductions/enlarging; text is drawn separately.
    const auto extent=[](UINT original,float dpi){return std::max(1u,static_cast<UINT>(std::ceil(original*std::min(1.,static_cast<double>(dpi)/96.))));};
    const UINT width=extent(skinImage_->width,dpiX*skinRasterOversample),height=extent(skinImage_->height,dpiY*skinRasterOversample);
    if(skinBitmap_ && skinBitmapFrame_==index && skinBitmapWidth_==width && skinBitmapHeight_==height)return;
    skinBitmap_.Reset();
    const auto& original=skinImage_->frames.at(index);
    const std::uint32_t* pixels=original.data();
    SkinRaster filtered;
    if(width!=skinImage_->width || height!=skinImage_->height){
        const auto found=std::find_if(skinRasters_.begin(),skinRasters_.end(),[&](const SkinRaster& r){return r.frame==index && r.width==width && r.height==height;});
        if(found!=skinRasters_.end()){
            std::rotate(found,found+1,skinRasters_.end());pixels=skinRasters_.back().pixels.data();
        }else{
            Microsoft::WRL::ComPtr<IWICBitmap> source;
            check(imaging_->CreateBitmapFromMemory(skinImage_->width,skinImage_->height,GUID_WICPixelFormat32bppPBGRA,
                skinImage_->width*4,static_cast<UINT>(original.size()*4),reinterpret_cast<BYTE*>(const_cast<std::uint32_t*>(original.data())),&source));
            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;check(imaging_->CreateBitmapScaler(&scaler));
            // Unlike ordinary cubic, this kernel widens with the reduction ratio.
            // Filtering premultiplied color and alpha together avoids dark/color
            // fringes from invisible RGB, including composited APNG frames.
            check(scaler->Initialize(source.Get(),width,height,WICBitmapInterpolationModeHighQualityCubic));
            filtered.frame=index;filtered.width=width;filtered.height=height;
            filtered.pixels.resize(static_cast<std::size_t>(width)*height);
            const auto bytes=filtered.pixels.size()*4;
            check(scaler->CopyPixels(nullptr,width*4,static_cast<UINT>(bytes),reinterpret_cast<BYTE*>(filtered.pixels.data())));
            // Cubic reconstruction may overshoot on translucent edges. Keep
            // every channel within alpha before giving the surface to D2D.
            for(auto& p:filtered.pixels){const auto a=p>>24;p=(a<<24)|(std::min((p>>16)&255,a)<<16)|(std::min((p>>8)&255,a)<<8)|std::min(p&255,a);}
            // Sharpen once on cache miss, in straight RGB, keeping alpha and
            // translucent edges unchanged. Text is not part of this bitmap.
            sharpenSkinRaster(filtered.pixels,width,height);
            pixels=filtered.pixels.data();
            constexpr std::size_t budget=16u*1024u*1024u,entries=8;
            if(bytes<=budget){
                while(!skinRasters_.empty() && (skinRasterBytes_+bytes>budget || skinRasters_.size()>=entries)){
                    skinRasterBytes_-=skinRasters_.front().pixels.size()*4;skinRasters_.erase(skinRasters_.begin());
                }
                skinRasterBytes_+=bytes;skinRasters_.push_back(std::move(filtered));pixels=skinRasters_.back().pixels.data();
            } // Unusually large results are uploaded once without retaining CPU pixels.
        }
    }
    // Bitmap DPI keeps source rectangles in their original authored coordinates.
    // Stretch bands, layout bounds and hit rectangles therefore do not change.
    const auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.f*width/skinImage_->width,96.f*height/skinImage_->height);
    check(target_->CreateBitmap(D2D1::SizeU(width,height),pixels,width*4,properties,&skinBitmap_));
    skinBitmapFrame_=index;skinBitmapWidth_=width;skinBitmapHeight_=height;
}
void CandidateRenderer::drawSkinBackground(float width,float height){
    const auto horizontal=skin::sliceAxis(skinImage_->width,width,skinLayout_->horizontal);
    const auto vertical=skin::sliceAxis(skinImage_->height,height,skinLayout_->vertical);
    float dpiX=96,dpiY=96;target_->GetDpi(&dpiX,&dpiY);
    const auto snap=[](float value,float dpi){return std::round(value*dpi/96.f)*96.f/dpi;};
    // Adjacent slices must share a physical pixel edge. Fractional edges blend
    // separately over transparency, leaving seams (or darkening translucent art).
    for(const auto& y:vertical)for(const auto& x:horizontal){
        const auto destination=D2D1::RectF(snap(x.destination,dpiX),snap(y.destination,dpiY),
            snap(x.destination+x.destinationLength,dpiX),snap(y.destination+y.destinationLength,dpiY));
        if(destination.right<=destination.left || destination.bottom<=destination.top)continue;
        const auto source=D2D1::RectF(x.source,y.source,x.source+x.sourceLength,y.source+y.sourceLength);
        target_->DrawBitmap(skinBitmap_.Get(),destination,1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
    }
}
bool CandidateRenderer::skinFrameChanged() const{
    if(codePane_)return (hasCodePane_ && codePane_->skinFrameChanged()) || (hasCandidatePane_ && candidatePane_->skinFrameChanged());
    return skinImage_ && style_.skinAnimation && skin::animationFrame(skinImage_->delays,skinImage_->plays,GetTickCount64()-skinStarted_).index!=skinRenderedFrame_;
}
UINT CandidateRenderer::skinDelay() const{
    if(codePane_){const auto a=hasCodePane_?codePane_->skinDelay():0,b=hasCandidatePane_?candidatePane_->skinDelay():0;return a && b?std::min(a,b):std::max(a,b);}
    if(!skinImage_ || !style_.skinAnimation)return 0;
    return skin::animationFrame(skinImage_->delays,skinImage_->plays,GetTickCount64()-skinStarted_).remainingMs;
}
std::u16string CandidateRenderer::skinStatus() const{
    if(codePane_)return codePane_->skinStatus();
    if(!skinResources_)return u"内置默认外观";
    if(!skinResources_->error.empty())return skinResources_->error;
    if(skinResources_->images.empty())return u"内置默认外观";
    const auto& d=skinResources_->definition;std::u16string result=d.name;
    if(!d.author.empty())result+=u" · "+d.author;
    for(const auto& warning:d.warnings){if(result.size()+warning.size()>600)break;result+=u"\n"+warning;}
    return result;
}
}
