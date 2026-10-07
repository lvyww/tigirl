#pragma once
#include "SsfArchive.h"
namespace tiger::skin {
struct PngFrame {
    Bytes png;
    std::uint32_t x=0,y=0,width=0,height=0,delayMs=0;
    std::uint8_t dispose=0,blend=0;
};
struct PngAnimation {
    std::uint32_t width=0,height=0,plays=0;
    std::vector<PngFrame> frames;
};
// Empty frames means an ordinary image. APNG is split into independently
// decodable PNG rectangles; disposal and compositing belong to the renderer.
PngAnimation splitApng(const Bytes&);
struct FrameTime {std::size_t index=0;std::uint32_t remainingMs=0;};
FrameTime animationFrame(const std::vector<std::uint32_t>& delays,std::uint32_t plays,std::uint64_t elapsedMs);
}
