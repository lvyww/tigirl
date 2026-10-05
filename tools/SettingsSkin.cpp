#define NOMINMAX
#include "SettingsSkin.h"
#include <stdexcept>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"windowscodecs.lib")
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Cannot render settings skin");}
D2D1_COLOR_F color(COLORREF c){return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f);}
}
void SettingsSkin::begin(HDC dc,const RECT& bounds,UINT dpi) {
    if(!factory_)check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory_.GetAddressOf()));
    auto load=[](int id,ComPtr<IWICBitmap>& image){
        auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
        if(!resource)throw std::runtime_error("Missing tiger cream ornaments");
        auto bytes=static_cast<BYTE*>(LockResource(LoadResource(module,resource)));
        ComPtr<IWICImagingFactory> wic;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));
        ComPtr<IWICStream> stream;check(wic->CreateStream(&stream));check(stream->InitializeFromMemory(bytes,SizeofResource(module,resource)));
        ComPtr<IWICBitmapDecoder> decoder;check(wic->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
        ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));
        ComPtr<IWICFormatConverter> converter;check(wic->CreateFormatConverter(&converter));check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        check(wic->CreateBitmapFromSource(converter.Get(),WICBitmapCacheOnLoad,&image));
    };
    if(!source_)load(14,source_);
    if(!detachedSource_)load(15,detachedSource_);
    if(!target_){
        auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));
        check(factory_->CreateDCRenderTarget(&properties,&target_));
        check(target_->CreateBitmapFromWicBitmap(source_.Get(),nullptr,&ornaments_));
        check(target_->CreateBitmapFromWicBitmap(detachedSource_.Get(),nullptr,&detached_));
    }
    check(target_->BindDC(dc,&bounds));target_->SetDpi(static_cast<float>(dpi),static_cast<float>(dpi));target_->BeginDraw();
}
void SettingsSkin::end(){auto hr=target_->EndDraw();if(hr==D2DERR_RECREATE_TARGET){ornaments_.Reset();detached_.Reset();target_.Reset();}else check(hr);}
void SettingsSkin::ornament(const D2D1_RECT_F& dst,const D2D1_RECT_F& src){
    auto size=ornaments_->GetSize();auto s=D2D1::RectF(src.left*size.width/640,src.top*size.height/480,src.right*size.width/640,src.bottom*size.height/480);
    target_->DrawBitmap(ornaments_.Get(),dst,1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,s);
}
void SettingsSkin::frame(HDC dc,const RECT& bounds,UINT dpi,bool contrast) {
    begin(dc,bounds,dpi);
    target_->Clear(contrast?color(GetSysColor(COLOR_BTNFACE)):D2D1::ColorF(0xfff3d9));
    ComPtr<ID2D1SolidColorBrush> brush;check(target_->CreateSolidColorBrush(contrast?color(GetSysColor(COLOR_WINDOWTEXT)):D2D1::ColorF(0x785638),&brush));
    const auto outline=D2D1::RoundedRect(D2D1::RectF(2,14,638,478),22,22);
    target_->DrawRoundedRectangle(outline,brush.Get(),2);
    if(!contrast){
        // Draw clean frame sections at their original scale. Decorations are independent sprites.
        for(auto r:{D2D1::RectF(0,0,164,56),D2D1::RectF(240,0,640,56),
            D2D1::RectF(0,56,16,380),D2D1::RectF(624,56,640,250),
            D2D1::RectF(624,292,640,480),D2D1::RectF(100,420,624,480)})ornament(r,r);
        ornament(D2D1::RectF(164,0,240,56),D2D1::RectF(310,0,386,56));
        // Replace the old edge-mounted paw with a clean straight border section.
        ornament(D2D1::RectF(624,250,640,292),D2D1::RectF(624,200,640,242));
        // Mirror the clean lower-right corner; the original lower-left includes the old tail.
        target_->SetTransform(D2D1::Matrix3x2F::Scale(-1,1,D2D1::Point2F(320,0)));
        ornament(D2D1::RectF(540,380,640,480),D2D1::RectF(540,380,640,480));
        target_->SetTransform(D2D1::Matrix3x2F::Identity());
        const auto draw=[&](const D2D1_RECT_F& dst,const D2D1_RECT_F& src){
            target_->DrawBitmap(detached_.Get(),dst,1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,src);
        };
        draw(D2D1::RectF(249,14,300,56),D2D1::RectF(66,194,625,652));
        draw(D2D1::RectF(6,416,54,474),D2D1::RectF(770,140,1249,710));
        draw(D2D1::RectF(66,438,89,462),D2D1::RectF(1418,298,1773,664));
        draw(D2D1::RectF(605,434,625,455),D2D1::RectF(1418,298,1773,664));
        // No opaque page card: the cream window surface and tiger border remain visible.
    }
    end();
}
void SettingsSkin::mascot(HDC dc,const RECT& bounds,UINT dpi,bool contrast) {
    if(contrast){FillRect(dc,&bounds,GetSysColorBrush(COLOR_BTNFACE));return;}
    begin(dc,bounds,dpi);target_->Clear(D2D1::ColorF(0xfff3d9));
    const float w=(bounds.right-bounds.left)*96.f/dpi,h=(bounds.bottom-bounds.top)*96.f/dpi;
    ornament(D2D1::RectF(0,0,w,h),D2D1::RectF(418,62,624,181));end();
}
HRGN SettingsSkin::region(UINT dpi) {
    auto p=[&](int n){return MulDiv(n,static_cast<int>(dpi),96);};
    HRGN result=CreateRoundRectRgn(0,p(14),p(640),p(480),p(46),p(46));
    for(int x:{2,586}){HRGN ear=CreateEllipticRgn(p(x),0,p(x+52),p(66));CombineRgn(result,result,ear,RGN_OR);DeleteObject(ear);}return result;
}

void SettingsSkin::surface(HDC dc,const RECT& bounds,UINT dpi,COLORREF fill,COLORREF border,float radius,float stroke) {
    if(bounds.right<=bounds.left || bounds.bottom<=bounds.top)return;
    begin(dc,bounds,dpi);target_->Clear(radius==0?color(fill):D2D1::ColorF(0xfff3d9));
    const float w=(bounds.right-bounds.left)*96.f/dpi,h=(bounds.bottom-bounds.top)*96.f/dpi;
    ComPtr<ID2D1SolidColorBrush> brush;check(target_->CreateSolidColorBrush(color(fill),&brush));
    auto rect=D2D1::RoundedRect(D2D1::RectF(stroke/2,stroke/2,w-stroke/2,h-stroke/2),radius,radius);
    target_->FillRoundedRectangle(rect,brush.Get());brush->SetColor(color(border));target_->DrawRoundedRectangle(rect,brush.Get(),stroke);end();
}
