// Bounded WP02 fixture checks, not a general image regression framework.
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
struct Image { uint32_t w=0,h=0; std::vector<uint8_t> rgb; std::vector<uint16_t> hdr; };
float half(uint16_t b) { int e=(b>>10)&31,f=b&1023; return (b&32768?-1.f:1.f)*(e?std::ldexp(1.f+f/1024.f,e-15):std::ldexp(float(f),-24)); }
Image read(const std::string& base) {
    Image i; std::ifstream f(base+".hdrbin",std::ios::binary);
    f.read(reinterpret_cast<char*>(&i.w),4); f.read(reinterpret_cast<char*>(&i.h),4);
    if (!f || !i.w || !i.h || uint64_t(i.w)*i.h>2048*2048) throw std::runtime_error("Invalid HDR header: "+base);
    i.hdr.resize(size_t(i.w)*i.h*4); f.read(reinterpret_cast<char*>(i.hdr.data()),i.hdr.size()*2);
    if (!f) throw std::runtime_error("Truncated HDR: "+base);
    std::ifstream p(base+".ppm",std::ios::binary); std::string magic; uint32_t w,h,maximum;
    p>>magic>>w>>h>>maximum; p.get();
    if (magic!="P6" || w!=i.w || h!=i.h || maximum!=255) throw std::runtime_error("Invalid PPM");
    i.rgb.resize(size_t(w)*h*3); p.read(reinterpret_cast<char*>(i.rgb.data()),i.rgb.size());
    if (!p) throw std::runtime_error("Truncated PPM");
    std::ifstream s(base+".srgb.ppm",std::ios::binary); s>>magic>>w>>h>>maximum; s.get();
    std::vector<uint8_t> rgb(i.rgb.size()); s.read(reinterpret_cast<char*>(rgb.data()),rgb.size());
    if (!s || magic!="P6" || maximum!=255 || w!=i.w || h!=i.h) throw std::runtime_error("Invalid SRGB probe");
    int maxDiff=0; for(size_t n=0;n<rgb.size();++n) maxDiff=std::max(maxDiff,std::abs(int(rgb[n])-i.rgb[n]));
    if(maxDiff>1) throw std::runtime_error("UNORM/manual vs SRGB/hardware encoding differs >1/255");
    std::cout<<"ENCODING_OK max="<<maxDiff<<"/255\n";
    return i;
}
template<class F> void roi(const Image& i,float x0,float y0,float x1,float y1,F f) {
    if (!(x0>=0 && y0>=0 && x1<=1 && y1<=1 && x0<x1 && y0<y1)
        || uint32_t(x0*i.w)+2>=uint32_t(x1*i.w) || uint32_t(y0*i.h)+2>=uint32_t(y1*i.h))
        throw std::runtime_error("Empty/out-of-bounds fixture ROI");
    // All permanent fixtures run with sidebar collapsed; erode 1px before comparison.
    for(uint32_t y=uint32_t(y0*i.h)+1;y<uint32_t(y1*i.h)-1;++y)
        for(uint32_t x=uint32_t(x0*i.w)+1;x<uint32_t(x1*i.w)-1;++x) f(size_t(y)*i.w+x);
}
void near(float actual,float ref) {
    if(!std::isfinite(actual) || std::abs(actual-ref)>.002f+.002f*std::abs(ref)) throw std::runtime_error("Linear analytic value mismatch: actual="+std::to_string(actual)+" ref="+std::to_string(ref));
}
float srgb(float c) { return c<=.0031308f?c*12.92f:1.055f*std::pow(c,1.f/2.4f)-.055f; }
void analytic(const Image& i,float exposure) {
    const std::array<std::array<float,3>,4> refs{{{1.1875f,.875f,.875f},{.5625f,1.125f,.25f},{.1f,.2f,.3f},{4,2,.5f}}};
    for(size_t k=0;k<4;++k) {
        float x=k%2?.7f:.2f,y=k/2?.7f:.2f;
        roi(i,x,y,x+.1f,y+.1f,[&](size_t n) {for(size_t c=0;c<3;++c) {
            near(half(i.hdr[n*4+c]),refs[k][c]);
            float value=refs[k][c]*exposure;
            int encoded=int(std::round(srgb(value/(1+value))*255));
            if(std::abs(int(i.rgb[n*3+c])-encoded)>1) throw std::runtime_error("Display analytic mismatch/double gamma");
        }});
    }
    std::cout<<"ANALYTIC_HDR_BLEND_OK tolerance=.002+.002*abs(ref) hdrMax=4\n";
}
void compare(const Image& a,const Image& b,bool normal) {
    if(a.w!=b.w || a.h!=b.h) throw std::runtime_error("Compare extent mismatch");
    std::vector<int> errors; int unlitMax=0;
    const auto test=[&](float x0,float y0,float x1,float y1,bool unlit) {roi(a,x0,y0,x1,y1,[&](size_t n){for(size_t c=0;c<3;++c){int d=std::abs(int(a.rgb[n*3+c])-b.rgb[n*3+c]);errors.push_back(d);if(unlit)unlitMax=std::max(unlitMax,d);}});};
    if(normal) {test(.22f,.30f,.45f,.70f,false);test(.55f,.30f,.78f,.70f,true);}
    else for(int k=0;k<4;++k){float x=k%2?.7f:.2f,y=k/2?.7f:.2f;test(x,y,x+.1f,y+.1f,true);}
    double sum=0;for(int e:errors)sum+=e;std::sort(errors.begin(),errors.end());
    double mae=sum/errors.size();int p99=errors[size_t(.99*(errors.size()-1))];
    if(mae>1 || p99>3 || unlitMax>1) throw std::runtime_error("Cross-path MAE/P99/Unlit threshold exceeded");
    std::cout<<"CROSS_PATH_OK MAE="<<mae<<"/255 P99="<<p99<<"/255 unlitMax="<<unlitMax<<"/255\n";
}
void response(const Image& a,const Image& b,bool environment) {
    double changed=0;size_t count=0; int invariant=0;
    roi(a,.22f,.3f,.45f,.7f,[&](size_t n){for(size_t c=0;c<3;++c){changed+=std::abs(int(a.rgb[n*3+c])-b.rgb[n*3+c]);++count;}});
    roi(a,.55f,.3f,.78f,.7f,[&](size_t n){for(size_t c=0;c<3;++c)invariant=std::max(invariant,std::abs(int(a.rgb[n*3+c])-b.rgb[n*3+c]));});
    if(changed/count<2 || invariant>1) throw std::runtime_error("Normal/environment response absent or Unlit changed");
    std::cout<<(environment?"ENVIRONMENT":"NORMAL_SCALE")<<"_RESPONSE_OK mean="<<changed/count<<" unlitMax="<<invariant<<"\n";
}
void semanticCompare(const Image& a,const Image& b,const std::string& base) {
    std::ifstream f(base+".codesbin",std::ios::binary);uint32_t w=0,h=0;
    f.read(reinterpret_cast<char*>(&w),4);f.read(reinterpret_cast<char*>(&h),4);
    if(w!=a.w || h!=a.h)throw std::runtime_error("Semantic visibility extent mismatch");
    std::vector<uint8_t> codes(size_t(w)*h);f.read(reinterpret_cast<char*>(codes.data()),codes.size());
    if(!f)throw std::runtime_error("Truncated semantic visibility");
    std::vector<int> errors;int unlitMax=0;size_t pbr=0,unlit=0;
    // Predetermined visibility ROI: valid model code and its 8 neighbors must
    // agree. This erodes geometry/cutout silhouettes by exactly one pixel.
    for(uint32_t y=1;y+1<h;++y)for(uint32_t x=1;x+1<w;++x) {
        size_t n=size_t(y)*w+x;auto code=codes[n];if(code!=1 && code!=2)continue;
        bool interior=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            if(codes[size_t(int(y)+dy)*w+int(x)+dx]!=code)interior=false;
        if(!interior)continue;if(code==1)++pbr;else ++unlit;
        for(size_t c=0;c<3;++c){int d=std::abs(int(a.rgb[n*3+c])-b.rgb[n*3+c]);errors.push_back(d);if(code==2)unlitMax=std::max(unlitMax,d);}
    }
    if(!pbr || !unlit || errors.empty())throw std::runtime_error("Empty semantic ROI");
    double sum=0;for(int e:errors)sum+=e;std::sort(errors.begin(),errors.end());
    double mae=sum/errors.size();int p99=errors[size_t(.99*(errors.size()-1))];
    if(mae>1 || p99>3 || unlitMax>1)throw std::runtime_error("Semantic cross-path threshold exceeded");
    std::cout<<"SEMANTIC_CROSS_PATH_OK pbr="<<pbr<<" unlit="<<unlit<<" MAE="<<mae<<"/255 P99="<<p99<<"/255 unlitMax="<<unlitMax<<"/255\n";
}
}
int main(int argc,char** argv) { try {
    if(argc<3) throw std::runtime_error("Mode/base required");
    std::string mode=argv[1]; auto a=read(argv[2]);
    if(mode=="analytic") analytic(a,argc>3?std::stof(argv[3]):1);
    else { if(argc<4)throw std::runtime_error("Second base required");auto b=read(argv[3]);
        if(a.w!=b.w || a.h!=b.h)throw std::runtime_error("Fixture extent mismatch");
        if(mode=="semantic")semanticCompare(a,b,argv[3]);
        else if(mode=="normal"||mode=="compare")compare(a,b,mode=="normal");
        else if(mode=="response"||mode=="environment")response(a,b,mode=="environment");
        else if(mode=="exposure") {if(a.hdr!=b.hdr)throw std::runtime_error("Exposure modified linear HDR");if(a.rgb==b.rgb)throw std::runtime_error("Exposure did not change Display");std::cout<<"EXPOSURE_ONLY_DISPLAY_OK\n";}
        else if(mode=="sky") {for(float x:{.22f,.55f})roi(a,x,.3f,x+.23f,.7f,[&](size_t n){for(size_t c=0;c<3;++c)near(half(a.hdr[n*4+c]),half(b.hdr[n*4+c]));});if(a.rgb==b.rgb)throw std::runtime_error("Sky toggle had no background response");std::cout<<"SKY_ONLY_BACKGROUND_OK\n";}
        else throw std::runtime_error("Unknown mode");
    }
    return 0;
}catch(const std::exception& e){std::cerr<<"HDR_CHECK_FAILED "<<e.what()<<'\n';return 1;} }
