#define NOMINMAX
#include "CandidateRenderer.h"
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
    if(image!=skinImage_){skinBitmap_.Reset();skinBitmapFrame_=skinRenderedFrame_=static_cast<std::size_t>(-1);}
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
    // Insets refer to the authored canvas. Shrinking that canvas while keeping
    // text at its original coordinates moves text outside the painted panel.
    // Grow for content, then apply global scaling to both art and text.
    const float minWidth=static_cast<float>(std::max(image->width,static_cast<unsigned>(std::max(0,next->horizontal.before)+std::max(0,next->horizontal.after)+1)));
    const float minHeight=static_cast<float>(std::max(image->height,static_cast<unsigned>(std::max(0,next->vertical.before)+std::max(0,next->vertical.after)+1)));
    width_=std::max(1.f,std::min(maxWidth,std::max(minWidth,right)));height_=std::max(minHeight,y);
}
void CandidateRenderer::prepareSkinBitmap(){
    if(!skinImage_)return;
    const auto index=style_.skinAnimation?skin::animationFrame(skinImage_->delays,skinImage_->plays,GetTickCount64()-skinStarted_).index:0;
    if(skinBitmap_ && skinBitmapFrame_==index)return;
    skinBitmap_.Reset();
    const auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
    check(target_->CreateBitmap(D2D1::SizeU(skinImage_->width,skinImage_->height),skinImage_->frames.at(index).data(),skinImage_->width*4,properties,&skinBitmap_));skinBitmapFrame_=index;
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
