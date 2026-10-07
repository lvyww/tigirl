#define NOMINMAX
#include "SsfResources.h"
#include "../Text.h"
#include <wrl/client.h>
#include <shlobj.h>
#include <algorithm>
#include <mutex>
#include <set>
#include <stdexcept>
namespace tiger::skin {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Cannot decode SSF image");}
// Image decoding is memory-only. The catalog loader below reuses Tigirl's
// existing bounded reader; it accepts only a leaf name in the skin directory.
std::vector<std::uint32_t> decodeImage(IWICImagingFactory* factory,const Bytes& bytes,unsigned& width,unsigned& height){
    if(bytes.empty() || bytes.size()>maxAsset)throw std::runtime_error("Image input budget exceeded");
    ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));check(frame->GetSize(&width,&height));
    if(!width || !height || width>8192 || height>8192 || std::uint64_t(width)*height>16000000)throw std::runtime_error("Decoded SSF image budget exceeded");
    ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width)*height);
    check(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()*4),reinterpret_cast<BYTE*>(pixels.data())));return pixels;
}
Image loadImage(IWICImagingFactory* factory,const Bytes& bytes,std::uint64_t& budget){
    const auto animation=splitApng(bytes);Image image;
    if(animation.frames.empty()){image.frames.push_back(decodeImage(factory,bytes,image.width,image.height));image.delays.push_back(0);}
    else{
        image.width=animation.width;image.height=animation.height;image.plays=animation.plays;
        if(std::uint64_t(image.width)*image.height*animation.frames.size()*4>budget)throw std::runtime_error("Skin animation memory budget exceeded");
        std::vector<std::uint32_t> canvas(static_cast<std::size_t>(image.width)*image.height),previous;
        for(const auto& frame:animation.frames){unsigned width=0,height=0;const auto pixels=decodeImage(factory,frame.png,width,height);
            if(width!=frame.width || height!=frame.height)throw std::runtime_error("APNG decoded frame mismatch");
            if(frame.dispose==2)previous=canvas;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto source=pixels[static_cast<std::size_t>(y)*width+x];auto& target=canvas[static_cast<std::size_t>(frame.y+y)*image.width+frame.x+x];
                if(frame.blend==0)target=source;else{const auto inverse=255-(source>>24);std::uint32_t value=0;
                    for(unsigned shift:{0u,8u,16u,24u})value|=std::min(255u,((source>>shift)&255)+(((target>>shift)&255)*inverse+127)/255)<<shift;target=value;}}
            image.frames.push_back(canvas);image.delays.push_back(frame.delayMs);
            if(frame.dispose==1)for(unsigned y=0;y<height;++y)std::fill_n(canvas.begin()+static_cast<std::size_t>(frame.y+y)*image.width+frame.x,width,0u);
            else if(frame.dispose==2)canvas.swap(previous);
        }
    }
    const auto used=std::uint64_t(image.width)*image.height*image.frames.size()*4;if(used>budget)throw std::runtime_error("Skin decoded memory budget exceeded");budget-=used;return image;
}
std::shared_ptr<const Resources> readResources(const std::filesystem::path& file,IWICImagingFactory* factory){
    const auto bytes=tiger::readTextFileBytes(file); // Established 4 MiB runtime file limit.
    auto result=std::make_shared<Resources>();result->definition=parseDefinition(decodeArchive(Bytes(bytes.begin(),bytes.end())));std::set<std::u16string> used;
    bool decorations=false;for(const auto& entry:result->definition.layouts){used.insert(entry.second.image);decorations|=!entry.second.decorations.empty();}
    std::uint64_t budget=64u*1024u*1024u;for(const auto& name:used)result->images.emplace(name,loadImage(factory,result->definition.assets.at(name),budget));
    if(decorations)result->definition.warnings.push_back(u"自定义装饰定位尚未验证，当前只显示背景及背景动画。");
    result->definition.assets.clear();return result;
}
}
std::filesystem::path skinDirectory(){
    wchar_t overridePath[32768]{};
    const auto length=GetEnvironmentVariableW(L"NATIVE_TIGER_USER_ROOT",overridePath,32768);
    if(length>=32768)throw std::runtime_error("User root override is too long");
    if(length){
        const std::filesystem::path root(overridePath);
        if(!root.is_absolute())throw std::runtime_error("User root must be absolute");
        return root/L"皮肤";
    }
    // Match the code-table root even inside an AppContainer host.
    PWSTR local=nullptr;
    check(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_NO_PACKAGE_REDIRECTION|KF_FLAG_DONT_VERIFY,nullptr,&local));
    std::filesystem::path root;
    try { root=std::filesystem::path(local)/L"Tigirl"/L"皮肤"; }
    catch(...) { CoTaskMemFree(local); throw; }
    CoTaskMemFree(local);
    return root;
}
std::vector<std::u16string> skinFiles(const std::filesystem::path& directory){
    std::vector<std::u16string> names{u"默认.ssf"};std::error_code error;
    std::filesystem::directory_iterator it(directory,std::filesystem::directory_options::skip_permission_denied,error),end;
    for(unsigned scanned=0;!error && it!=end && scanned<4096;it.increment(error),++scanned){
        const auto name=it->path().filename().u16string();if(!validSkinFile(name))continue;
        const auto attributes=GetFileAttributesW(it->path().c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))continue;
        if(archivePath(name)==u"默认.ssf")continue;
        names.push_back(name);if(names.size()>=256)break;
    }
    std::sort(names.begin()+1,names.end(),[](const auto& a,const auto& b){return archivePath(a)<archivePath(b);});return names;
}
std::shared_ptr<const Resources> loadResources(const std::filesystem::path& directory,std::u16string_view name,IWICImagingFactory* factory,std::u16string_view revision){
    if(!validSkinFile(name)){auto failed=std::make_shared<Resources>();failed->error=u"无效皮肤文件名，已使用内置默认外观。";return failed;}
    const auto file=directory/std::filesystem::path(name);WIN32_FILE_ATTRIBUTE_DATA attributes{};
    const bool exists=GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&attributes)!=FALSE;
    std::wstring key=file.native()+L"|"+std::to_wstring(attributes.nFileSizeHigh)+L":"+std::to_wstring(attributes.nFileSizeLow)+L":"+std::to_wstring(attributes.ftLastWriteTime.dwHighDateTime)+L":"+std::to_wstring(attributes.ftLastWriteTime.dwLowDateTime);
    if(!revision.empty())key.append(reinterpret_cast<const wchar_t*>(revision.data()),revision.size());
    static std::mutex mutex;static std::map<std::wstring,std::shared_ptr<const Resources>> cache;
    std::lock_guard<std::mutex> guard(mutex);if(auto found=cache.find(key);found!=cache.end())return found->second;
    std::shared_ptr<const Resources> result;
    try{
        if(!exists || (attributes.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))throw std::runtime_error("Skin file missing or not a regular file");
        result=readResources(file,factory);
    }catch(const std::exception& error){auto failed=std::make_shared<Resources>();
        if(name!=u"默认.ssf" || exists){failed->error=u"皮肤加载失败，已使用内置默认外观：";for(const unsigned char c:std::string(error.what()))failed->error.push_back(static_cast<char16_t>(c));}
        result=failed;
    }
    if(cache.size()>=2)cache.clear();cache.emplace(std::move(key),result);return result;
}
}
