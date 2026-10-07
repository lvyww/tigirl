#include "SsfAnimation.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace tiger::skin {
namespace {
void need(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
std::uint32_t be(const Bytes& b,std::size_t p){need(p<=b.size() && b.size()-p>=4,"Truncated PNG integer");return (std::uint32_t(b[p])<<24)|(std::uint32_t(b[p+1])<<16)|(std::uint32_t(b[p+2])<<8)|b[p+3];}
void put(Bytes& b,std::uint32_t n){b.push_back(static_cast<std::uint8_t>(n>>24));b.push_back(static_cast<std::uint8_t>(n>>16));b.push_back(static_cast<std::uint8_t>(n>>8));b.push_back(static_cast<std::uint8_t>(n));}
void chunk(Bytes& b,const char* type,const Bytes& bytes){put(b,static_cast<std::uint32_t>(bytes.size()));const auto start=b.size();b.insert(b.end(),type,type+4);b.insert(b.end(),bytes.begin(),bytes.end());put(b,crc32(b.data()+start,b.size()-start));}
}
PngAnimation splitApng(const Bytes& data){
    static constexpr std::uint8_t signature[]={137,80,78,71,13,10,26,10};PngAnimation result;
    if(data.size()<8 || std::memcmp(data.data(),signature,8))return result;
    Bytes header,shared,packed;PngFrame current;bool control=false,animated=false,idat=false,ended=false,firstBeforeIdat=false;
    std::uint32_t count=0,sequence=0;std::size_t at=8;unsigned chunks=0;
    const auto finish=[&]{
        need(control && !packed.empty(),"Empty APNG frame");current.png.assign(signature,signature+8);Bytes ihdr;
        put(ihdr,current.width);put(ihdr,current.height);ihdr.insert(ihdr.end(),header.begin()+8,header.end());chunk(current.png,"IHDR",ihdr);
        current.png.insert(current.png.end(),shared.begin(),shared.end());chunk(current.png,"IDAT",packed);chunk(current.png,"IEND",{});
        result.frames.push_back(std::move(current));current=PngFrame{};packed.clear();control=false;
    };
    while(at<data.size()){
        need(++chunks<=50000 && data.size()-at>=12,"Truncated/oversized PNG chunk table");const auto length=be(data,at);need(length<=maxAsset && length<=data.size()-at-12,"PNG chunk length limit");
        const auto type=be(data,at+4);const std::size_t p=at+8;need(crc32(data.data()+at+4,std::size_t(length)+4)==be(data,p+length),"PNG CRC mismatch");
        if(type==0x49484452){need(at==8 && length==13,"Invalid PNG IHDR");header.assign(data.begin()+p,data.begin()+p+length);result.width=be(data,p);result.height=be(data,p+4);
            need(result.width>0 && result.height>0 && result.width<=8192 && result.height<=8192 && std::uint64_t(result.width)*result.height<=16000000,"PNG image limit");}
        else {need(!header.empty(),"PNG has no IHDR");
            if(type==0x6163544c){need(!animated && !idat && length==8,"Invalid APNG acTL");animated=true;count=be(data,p);result.plays=be(data,p+4);need(count>0 && count<=256,"APNG frame count limit");need(std::uint64_t(result.width)*result.height*count*4<=64u*1024u*1024u,"APNG decoded memory limit");}
            else if(type==0x6663544c){need(animated && length==26 && be(data,p)==sequence++,"Invalid APNG frame sequence");if(control)finish();need(result.frames.size()<count,"Too many APNG frames");
                current.width=be(data,p+4);current.height=be(data,p+8);current.x=be(data,p+12);current.y=be(data,p+16);
                need(current.width>0 && current.height>0 && current.x<=result.width && current.width<=result.width-current.x && current.y<=result.height && current.height<=result.height-current.y,"APNG frame outside canvas");
                const unsigned numerator=(data[p+20]<<8)|data[p+21],denominator=(data[p+22]<<8)|data[p+23];current.delayMs=std::max(20u,(numerator*1000u+(denominator?denominator:100u)/2)/(denominator?denominator:100u));
                current.dispose=data[p+24];current.blend=data[p+25];need(current.dispose<=2 && current.blend<=1,"Invalid APNG frame operation");
                if(!idat){need(result.frames.empty() && current.x==0 && current.y==0 && current.width==result.width && current.height==result.height,"Invalid APNG default frame");firstBeforeIdat=true;}control=true;
            }
            else if(type==0x49444154){if(animated && firstBeforeIdat){need(control && result.frames.empty(),"Unexpected APNG IDAT");packed.insert(packed.end(),data.begin()+p,data.begin()+p+length);}idat=true;}
            else if(type==0x66644154){need(animated && idat && control && length>=4 && be(data,p)==sequence++,"Invalid APNG frame data");packed.insert(packed.end(),data.begin()+p+4,data.begin()+p+length);}
            else if(type==0x504c5445 || type==0x74524e53){need(!idat && shared.size()+length+12<=4096,"Invalid/oversized PNG palette");shared.insert(shared.end(),data.begin()+at,data.begin()+p+length+4);}
            else if(type==0x49454e44){need(length==0 && idat && p+4==data.size(),"Invalid PNG ending");ended=true;if(control)finish();at=p+4;break;}
        }
        at=p+length+4;
    }
    need(ended && at==data.size(),"PNG has no IEND");if(animated)need(result.frames.size()==count,"APNG frame count mismatch");return result;
}
FrameTime animationFrame(const std::vector<std::uint32_t>& delays,std::uint32_t plays,std::uint64_t elapsed){
    if(delays.size()<2)return {};std::uint64_t duration=0;for(auto delay:delays)duration+=std::max(20u,delay);
    if(plays && elapsed/duration>=plays)return {delays.size()-1,0};auto within=elapsed%duration;
    for(std::size_t i=0;i<delays.size();++i){auto delay=std::max(20u,delays[i]);if(within<delay)return {i,static_cast<std::uint32_t>(delay-within)};within-=delay;}return {};
}
}
