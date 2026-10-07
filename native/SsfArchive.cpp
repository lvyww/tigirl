#include "SsfArchive.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_MALLOC
#define AES256 1
#define AES192 0
#define ECB 0
#define CTR 0
#define CBC 1
#ifdef _MSC_VER
#pragma warning(push,0)
#endif
extern "C" {
#include "third_party/miniz/miniz_tinfl.c"
#include "third_party/tiny-aes/aes.c"
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
namespace tiger::skin {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::uint16_t u16(const Bytes& b,std::size_t p){require(p<=b.size() && b.size()-p>=2,"Truncated SSF integer");return static_cast<std::uint16_t>(b[p]|(b[p+1]<<8));}
std::uint32_t u32(const Bytes& b,std::size_t p){require(p<=b.size() && b.size()-p>=4,"Truncated SSF integer");return b[p]|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
Bytes part(const Bytes& b,std::size_t p,std::size_t n){require(p<=b.size() && n<=b.size()-p,"Truncated SSF range");return {b.begin()+p,b.begin()+p+n};}
std::u16string utf16(const std::uint8_t* data,std::size_t size,bool big=false){
    require(size%2==0,"Odd UTF-16 length");std::u16string text;text.reserve(size/2);
    for(std::size_t i=0;i<size;i+=2)text.push_back(static_cast<char16_t>(big?(data[i]<<8)|data[i+1]:data[i]|(data[i+1]<<8)));
    for(std::size_t i=0;i<text.size();++i){const auto c=text[i];if(c>=0xd800 && c<=0xdbff){require(i+1<text.size() && text[i+1]>=0xdc00 && text[i+1]<=0xdfff,"Invalid UTF-16 surrogate");++i;}else require(c<0xdc00 || c>0xdfff,"Invalid UTF-16 surrogate");}
    return text;
}
std::u16string utf8(const std::uint8_t* data,std::size_t size){
    std::u16string text;
    for(std::size_t i=0;i<size;){std::uint32_t c=data[i++];int count=0;std::uint32_t minimum=0;
        if(c<0x80){}else if(c>=0xc2 && c<=0xdf){c&=31;count=1;minimum=0x80;}else if(c>=0xe0 && c<=0xef){c&=15;count=2;minimum=0x800;}else if(c>=0xf0 && c<=0xf4){c&=7;count=3;minimum=0x10000;}else throw std::runtime_error("Invalid UTF-8; use UTF-8 or UTF-16 skin.ini");
        require(static_cast<std::size_t>(count)<=size-i,"Truncated UTF-8");
        for(int j=0;j<count;++j){require((data[i]&0xc0)==0x80,"Invalid UTF-8 continuation");c=(c<<6)|(data[i++]&63);}
        require(c>=minimum && c<=0x10ffff && (c<0xd800 || c>0xdfff),"Invalid UTF-8 scalar");
        if(c<0x10000)text.push_back(static_cast<char16_t>(c));else{c-=0x10000;text.push_back(static_cast<char16_t>(0xd800+(c>>10)));text.push_back(static_cast<char16_t>(0xdc00+(c&1023)));}
    }return text;
}
Bytes inflate(const std::uint8_t* source,std::size_t packed,std::size_t expanded,bool zlib){
    require(expanded<=maxExpanded,"SSF decompression budget exceeded");
    Bytes out(std::max<std::size_t>(expanded,1));tinfl_decompressor state;tinfl_init(&state);
    std::size_t inSize=packed,outSize=expanded;
    const auto status=tinfl_decompress(&state,source,&inSize,out.data(),out.data(),&outSize,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF|(zlib?TINFL_FLAG_PARSE_ZLIB_HEADER:0));
    require(status==TINFL_STATUS_DONE && inSize==packed && outSize==expanded,"Invalid/trailing/oversized compressed SSF data");
    out.resize(expanded);return out;
}
void add(Files& files,std::u16string name,Bytes value){
    name=archivePath(name);require(files.emplace(std::move(name),std::move(value)).second,"Duplicate SSF filename");
}
// Decode the user's SSF v3 skin format; these constants are public format data.
Files skinV3(const Bytes& raw){
    require(raw.size()>=24 && u32(raw,4)==3,"Unsupported Skin version");
    require((raw.size()-8)%16==0,"Invalid SSF ciphertext size");
    const std::uint8_t key[32]={0x52,0x36,0x46,0x1a,0xd3,0x85,0x03,0x66,0x90,0x45,0x16,0x28,0x79,0x03,0x36,0x23,0xdd,0xbe,0x6f,0x03,0xff,0x04,0xe3,0xca,0xd5,0x7f,0xfc,0xa3,0x50,0xe4,0x9e,0xd9};
    const std::uint8_t iv[16]={0xe0,0x7a,0xad,0x35,0xe0,0x90,0xaa,0x03,0x8a,0x51,0xfd,0x05,0xdf,0x8c,0x5d,0x0f};
    Bytes decoded(raw.begin()+8,raw.end());AES_ctx aes{};AES_init_ctx_iv(&aes,key,iv);AES_CBC_decrypt_buffer(&aes,decoded.data(),decoded.size());
    const auto padding=decoded.back();require(padding>=1 && padding<=16 && padding<=decoded.size(),"Invalid SSF padding");
    require(std::all_of(decoded.end()-padding,decoded.end(),[&](auto b){return b==padding;}),"Invalid SSF padding");decoded.resize(decoded.size()-padding);
    const auto expanded=u32(decoded,0);require(expanded>=8 && expanded<=maxExpanded,"SSF expansion limit exceeded");
    auto data=inflate(decoded.data()+4,decoded.size()-4,expanded,true);
    require(u32(data,0)==expanded,"SSF container size mismatch");
    const auto table=u32(data,4);require(table>0 && table%4==0 && table/4<=4096 && table<=data.size()-8,"Invalid SSF table");
    std::size_t previous=8+table;Files files;
    for(std::size_t i=0;i<table/4;++i){const auto offset=u32(data,8+i*4);require(offset>=previous,"Overlapping SSF entries");
        const auto length=u32(data,offset);require(length>0 && length<=4096 && length%2==0,"Invalid SSF filename length");
        const auto name=part(data,std::size_t(offset)+4,length);const std::size_t sizeOffset=std::size_t(offset)+4+length;
        const auto size=u32(data,sizeOffset);require(size<=maxAsset,"SSF asset limit exceeded");
        add(files,utf16(name.data(),name.size()),part(data,sizeOffset+4,size));previous=sizeOffset+4+size;
    }
    require(previous==data.size(),"Trailing SSF records");return files;
}
Files zip(const Bytes& raw){
    require(raw.size()>=22,"Truncated ZIP SSF");
    const std::size_t first=raw.size()>65557?raw.size()-65557:0;std::size_t eocd=raw.size();
    for(std::size_t p=raw.size()-22;;--p){if(u32(raw,p)==0x06054b50 && u16(raw,p+20)==raw.size()-p-22){eocd=p;break;}if(p==first)break;}
    require(eocd!=raw.size(),"ZIP directory not found");
    const auto count=u16(raw,eocd+10);const auto directorySize=u32(raw,eocd+12),directoryOffset=u32(raw,eocd+16);
    require(u16(raw,eocd+4)==0 && u16(raw,eocd+6)==0 && u16(raw,eocd+8)==count,"Multidisk ZIP unsupported");
    require(count>0 && count<=4096 && directoryOffset<=eocd && directorySize==eocd-directoryOffset,"Invalid ZIP directory");
    std::size_t p=directoryOffset,expandedTotal=0;Files files;std::vector<std::pair<std::size_t,std::size_t>> regions;
    for(unsigned i=0;i<count;++i){require(p<=eocd && eocd-p>=46 && u32(raw,p)==0x02014b50,"Truncated ZIP entry");
        const auto flags=u16(raw,p+8),method=u16(raw,p+10),nameSize=u16(raw,p+28),extraSize=u16(raw,p+30),commentSize=u16(raw,p+32);
        const auto checksum=u32(raw,p+16),packed=u32(raw,p+20),expanded=u32(raw,p+24),attributes=u32(raw,p+38),local=u32(raw,p+42);
        require((flags&0x2041)==0 && (method==0 || method==8),"Encrypted/unsupported ZIP SSF");
        require(u16(raw,p+34)==0 && packed!=0xffffffffu && expanded!=0xffffffffu && local!=0xffffffffu,"ZIP64/multidisk SSF unsupported");
        require(nameSize>0 && nameSize<=4096 && 46u+std::size_t(nameSize)+extraSize+commentSize<=eocd-p,"Invalid ZIP filename");
        auto nameBytes=part(raw,p+46,nameSize);auto name=utf8(nameBytes.data(),nameBytes.size());
        const bool directory=!name.empty() && (name.back()==u'/' || name.back()==u'\\');if(directory)name.pop_back();name=archivePath(name);
        const auto type=(attributes>>16)&0170000u;require(type==0 || type==0100000u || type==0040000u,"ZIP links/special files rejected");
        require(local<directoryOffset && directoryOffset-local>=30 && u32(raw,local)==0x04034b50,"Invalid ZIP local header");
        const auto localName=u16(raw,local+26),localExtra=u16(raw,local+28);
        require(u16(raw,local+6)==flags && u16(raw,local+8)==method && localName==nameSize,"ZIP local/central mismatch");
        const std::size_t dataAt=std::size_t(local)+30+localName+localExtra;
        require(dataAt<=directoryOffset && packed<=directoryOffset-dataAt && part(raw,std::size_t(local)+30,localName)==nameBytes,"ZIP range/name mismatch");
        if(!(flags&8))require(u32(raw,local+14)==checksum && u32(raw,local+18)==packed && u32(raw,local+22)==expanded,"ZIP size/checksum header mismatch");
        require(expanded<=maxAsset && expanded<=maxExpanded-expandedTotal,"ZIP expansion limit exceeded");expandedTotal+=expanded;
        require(!directory || expanded==0,"Nonempty ZIP directory");
        Bytes bytes;if(method==0){require(packed==expanded,"Stored ZIP length mismatch");bytes=part(raw,dataAt,packed);}else bytes=inflate(raw.data()+dataAt,packed,expanded,false);
        require(crc32(bytes.data(),bytes.size())==checksum,"ZIP CRC mismatch");if(!directory)add(files,std::move(name),std::move(bytes));
        regions.emplace_back(local,dataAt+packed);p+=46+std::size_t(nameSize)+extraSize+commentSize;
    }
    require(p==eocd,"ZIP directory size mismatch");std::sort(regions.begin(),regions.end());
    for(std::size_t i=1;i<regions.size();++i)require(regions[i].first>=regions[i-1].second,"Overlapping ZIP members");
    return files;
}
}
std::u16string archivePath(std::u16string_view name){
    require(!name.empty() && name.size()<=2048,"Invalid SSF path length");std::u16string result(name);
    for(auto& c:result){require(c>=32 && std::u16string_view(u"<>:\"|?*").find(c)==std::u16string_view::npos,"Unsafe SSF path");if(c==u'\\')c=u'/';if(c>=u'A' && c<=u'Z')c+=32;}
    std::size_t at=0;while(at<result.size()){auto end=result.find(u'/',at);if(end==result.npos)end=result.size();auto p=std::u16string_view(result).substr(at,end-at);
        require(!p.empty() && p!=u"." && p!=u".." && p.back()!=u'.' && p.back()!=u' ',"Unsafe SSF path component");
        auto base=p.substr(0,p.find(u'.'));bool device=base==u"con" || base==u"prn" || base==u"aux" || base==u"nul";
        if(base.size()==4 && (base.substr(0,3)==u"com" || base.substr(0,3)==u"lpt"))device=(base[3]>=u'1' && base[3]<=u'9') || base[3]==u'¹' || base[3]==u'²' || base[3]==u'³';
        require(!device,"Reserved SSF path");if(end==result.size())break;at=end+1;require(at<result.size(),"Trailing SSF separator");
    }return result;
}
bool validSkinFile(std::u16string_view file) noexcept{try{const auto key=archivePath(file);return key.find(u'/')==key.npos && key.size()>4 && key.substr(key.size()-4)==u".ssf";}catch(...){return false;}}
std::u16string decodeText(const Bytes& bytes){
    require(bytes.size()<=1024u*1024u,"skin.ini exceeds limit");
    if(bytes.size()>=2 && bytes[0]==255 && bytes[1]==254)return utf16(bytes.data()+2,bytes.size()-2);
    if(bytes.size()>=2 && bytes[0]==254 && bytes[1]==255)return utf16(bytes.data()+2,bytes.size()-2,true);
    const std::size_t skip=bytes.size()>=3 && bytes[0]==239 && bytes[1]==187 && bytes[2]==191?3:0;return utf8(bytes.data()+skip,bytes.size()-skip);
}
std::uint32_t crc32(const std::uint8_t* bytes,std::size_t size){
    static const auto table=[] {std::array<std::uint32_t,256> values{};for(unsigned i=0;i<256;++i){auto c=std::uint32_t(i);for(int j=0;j<8;++j)c=(c&1)?0xedb88320u^(c>>1):c>>1;values[i]=c;}return values;}();
    std::uint32_t c=0xffffffffu;for(std::size_t i=0;i<size;++i)c=table[(c^bytes[i])&255]^(c>>8);return c^0xffffffffu;
}
Files decodeArchive(const Bytes& raw){
    require(raw.size()>=4 && raw.size()<=maxInput,"SSF input size limit");Files files;
    if(std::memcmp(raw.data(),"Skin",4)==0)files=skinV3(raw);else if(u32(raw,0)==0x04034b50)files=zip(raw);else throw std::runtime_error("Unsupported SSF signature");
    for(const auto& entry:files){std::size_t at=0;while((at=entry.first.find(u'/',at))!=entry.first.npos){require(files.find(entry.first.substr(0,at))==files.end(),"SSF file/directory collision");++at;}}
    require(files.find(u"skin.ini")!=files.end(),"SSF has no root skin.ini");return files;
}
}
