// Independent bounded WP03-A reference. No engine/shader mapping helper used.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Sample {uint32_t w=0,h=0;std::vector<uint16_t> mrt,hdr;std::vector<uint8_t> rgb;};
float half(uint16_t b) {const int e=(b>>10)&31,m=b&1023;if(e==31)throw std::runtime_error("Nonfinite half");return (b&32768?-1.f:1.f)*(e?std::ldexp(1.f+m/1024.f,e-15):std::ldexp(float(m),-24));}
float srgb(float x) {return x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f;}
std::vector<uint8_t> ppm(const std::string& file,uint32_t w,uint32_t h) {std::ifstream f(file,std::ios::binary);std::string magic;uint32_t iw=0,ih=0,max=0;f>>magic>>iw>>ih>>max;f.get();if(!f||magic!="P6"||iw!=w||ih!=h||max!=255)throw std::runtime_error("Invalid PPM: "+file);std::vector<uint8_t> b(size_t(w)*h*3);f.read(reinterpret_cast<char*>(b.data()),b.size());if(!f)throw std::runtime_error("Truncated PPM");return b;}
Sample read(const std::string& file,bool debug) {
    Sample s;std::ifstream f(file+".hdrbin",std::ios::binary);f.read(reinterpret_cast<char*>(&s.w),4);f.read(reinterpret_cast<char*>(&s.h),4);
    if(!f||!s.w||!s.h||uint64_t(s.w)*s.h>2048*2048)throw std::runtime_error("Invalid HDR: "+file);
    s.hdr.resize(size_t(s.w)*s.h*4);f.read(reinterpret_cast<char*>(s.hdr.data()),s.hdr.size()*2);if(!f)throw std::runtime_error("Truncated HDR");
    s.rgb=ppm(file+".ppm",s.w,s.h);const auto hardware=ppm(file+".srgb.ppm",s.w,s.h);
    for(size_t n=0;n<s.rgb.size();++n)if(std::abs(int(s.rgb[n])-hardware[n])>1)throw std::runtime_error("UNORM/SRGB mismatch >1/255");
    if(debug) {std::ifstream m(file+".mrtbin",std::ios::binary);uint32_t w=0,h=0;m.read(reinterpret_cast<char*>(&w),4);m.read(reinterpret_cast<char*>(&h),4);s.mrt.resize(size_t(s.w)*s.h*16);m.read(reinterpret_cast<char*>(s.mrt.data()),s.mrt.size()*2);if(!m||w!=s.w||h!=s.h)throw std::runtime_error("Invalid MRT");}
    return s;
}
void reference(const Sample& s,int view,bool clamp) {
    const size_t count=size_t(s.w)*s.h;size_t empty=0,visible=0,positionClamped=0,emissiveClamped=0,pbr=0,unlit=0;int maximum=0;
    for(size_t n=0;n<count;++n) {
        const int code=int(half(s.mrt[count*8+n*4+3]));std::array<float,3> ref{};
        if(!code)++empty;else {++visible;if(code==1)++pbr;if(code==2)++unlit;
            for(size_t c=0;c<3;++c) {
                if(std::abs(half(s.mrt[count*8+n*4+c]))>2.f)++positionClamped;
                if(half(s.mrt[count*12+n*4+c])>1.f)++emissiveClamped;
                switch(view) {
                case 1:ref[c]=half(s.mrt[n*4+c]);break;
                case 2:ref[c]=half(s.mrt[n*4+3]);break;
                case 3:ref[c]=half(s.mrt[count*4+n*4+c])*.5f+.5f;break;
                case 4:ref[c]=half(s.mrt[count*4+n*4+3]);break;
                case 5:ref[c]=(half(s.mrt[count*8+n*4+c])+2.f)/4.f;break;
                case 6:ref[c]=code==1?float(c==0):code==2?float(c==1):float(c!=1);break;
                case 7:ref[c]=half(s.mrt[count*12+n*4+c]);break;
                case 8:ref[c]=half(s.mrt[count*12+n*4+3]);break;
                default:throw std::runtime_error("Invalid view");
                }
            }
        }
        for(size_t c=0;c<3;++c)maximum=std::max(maximum,std::abs(int(s.rgb[n*3+c])-int(std::round(srgb(std::clamp(ref[c],0.f,1.f))*255))));
    }
    if(!empty||!visible||maximum>1)throw std::runtime_error("Independent reference/Empty failed");
    if(clamp&&(!positionClamped||!emissiveClamped))throw std::runtime_error("Clamp fixture does not exercise both limits");
    std::cout<<"GBUFFER_REFERENCE_OK view="<<view<<" empty="<<empty<<" pbr="<<pbr<<" unlit="<<unlit<<" max="<<maximum<<"/255 positionClamp="<<positionClamped<<" emissiveClamp="<<emissiveClamped<<'\n';
}
void compare(const Sample& a,const Sample& b,const Sample& debug,bool both) {
    if(a.w!=b.w||a.h!=b.h||a.w!=debug.w||a.h!=debug.h)throw std::runtime_error("Extent mismatch");
    const size_t count=size_t(a.w)*a.h;std::vector<int> errors;size_t pbr=0,unlit=0;int unlitMax=0;
    const auto code=[&](size_t n){return int(half(debug.mrt[count*8+n*4+3]));};
    for(uint32_t y=1;y+1<a.h;++y)for(uint32_t x=1;x+1<a.w;++x) {const size_t n=size_t(y)*a.w+x;const int c=code(n);if(c!=1&&c!=2)continue;
        bool interior=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(code(size_t(int(y)+dy)*a.w+int(x)+dx)!=c)interior=false;
        if(!interior)continue;if(c==1)++pbr;else ++unlit;
        for(size_t k=0;k<3;++k){const int e=std::abs(int(a.rgb[n*3+k])-b.rgb[n*3+k]);errors.push_back(e);if(c==2)unlitMax=std::max(unlitMax,e);}
    }
    if(errors.empty()||(both&&(!pbr||!unlit)))throw std::runtime_error("Empty eroded visibility ROI");
    double sum=0;for(int e:errors)sum+=e;std::sort(errors.begin(),errors.end());const double mae=sum/errors.size();const int p99=errors[size_t(.99*(errors.size()-1))];
    if(mae>1||p99>3||unlitMax>1)throw std::runtime_error("WP02 MAE/P99/Unlit thresholds failed");
    std::cout<<"CROSS_PATH_OK pbr="<<pbr<<" unlit="<<unlit<<" MAE="<<mae<<"/255 P99="<<p99<<"/255 unlitMax="<<unlitMax<<"/255\n";
}
}
int main(int argc,char** argv) {try {
    if(argc!=5)throw std::runtime_error("prefix width height semantic|clamp|mclaren required");
    const std::string prefix=argv[1],suffix="_"+std::string(argv[2])+"x"+argv[3],kind=argv[4];
    const auto load=[&](int step,bool debug){return read(prefix+"-step"+std::to_string(step)+suffix,debug);};
    auto debug=load(6,true);
    for(int step=0;step<6;step+=2)compare(load(step,false),load(step+1,false),debug,kind=="semantic");
    for(int view=1;view<=8;++view) {auto a=load(4+view*2,true),b=load(5+view*2,true);reference(a,view,kind=="clamp");reference(b,view,kind=="clamp");
        if(a.rgb!=b.rgb||a.mrt!=b.mrt)throw std::runtime_error("Exposure/Blend changed debug/MRT");
        if(kind=="clamp"&&a.hdr==b.hdr)throw std::runtime_error("Blend did not change SceneColor");
    }
    compare(load(22,false),load(23,false),debug,kind=="semantic");compare(load(24,false),load(23,false),debug,kind=="semantic");
    std::cout<<"KUENGINE_PATH_PIXEL_CHECKS_OK fixedROI=1px fixedTolerance=1/255 debugInvariant=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"PATH_CHECK_FAILED "<<e.what()<<'\n';return 1;}}
