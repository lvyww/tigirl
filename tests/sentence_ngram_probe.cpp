#include "../native/SentenceNgram.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
std::u16string token(const std::string& value){
    if(value=="-")return {};if(value.size()%4)throw std::runtime_error("Invalid UTF16 query");
    std::u16string result;for(std::size_t i=0;i<value.size();i+=4)result+=static_cast<char16_t>(std::stoul(value.substr(i,4),nullptr,16));return result;
}
int probe(const std::filesystem::path& modelPath,const std::filesystem::path& queries){
    try {
        auto model=tiger::SentenceNgram::Open(modelPath);
        if(model!=tiger::SentenceNgram::Open(modelPath))throw std::runtime_error("Model mapping not reused in process");
        std::ifstream input(queries);if(!input)throw std::runtime_error("Cannot read queries");
        std::string a,b,c;int include;std::cout<<std::setprecision(17);
        while(input>>a>>b>>c>>include) {
            const auto x=token(a),y=token(b),z=token(c);
            std::cout<<model->logProbability(x,y,z,include!=0)<<' '<<model->hasObservedBigram(y,z)<<'\n';
        }
        if(!input.eof())throw std::runtime_error("Malformed query row");return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){return argc==3?probe(argv[1],argv[2]):2;}
#else
int main(int argc,char** argv){return argc==3?probe(argv[1],argv[2]):2;}
#endif
