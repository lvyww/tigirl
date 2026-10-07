#pragma once
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

// Settings-only resources; no dependency from the real candidate window.
class SettingsSkin {
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IWICBitmap> source_,detachedSource_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> ornaments_,detached_;
    UINT contourDpi_=0;
    std::vector<std::uint32_t> contourData_;
    void loadOrnaments();
    void begin(HDC dc,const RECT& bounds,UINT dpi);
    void end();
    void ornament(const D2D1_RECT_F& destination,const D2D1_RECT_F& source);
public:
    void surface(HDC dc,const RECT& bounds,UINT dpi,COLORREF fill,COLORREF border,float radius=7,float stroke=1);
    void frame(HDC dc,const RECT& bounds,UINT dpi,bool contrast);
    void mascot(HDC dc,const RECT& bounds,UINT dpi,bool contrast);
    HRGN region(UINT dpi,bool contrast=false);
};
