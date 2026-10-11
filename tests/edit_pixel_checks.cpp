#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
float half(uint16_t b){int e=(b>>10)&31,m=b&1023;const float sign=b&32768?-1.f:1.f;if(e==31)return m?std::numeric_limits<float>::quiet_NaN():sign*std::numeric_limits<float>::infinity();return sign*(e?std::ldexp(1.f+m/1024.f,e-15):std::ldexp(float(m),-24));}
struct Image {uint32_t w=0,h=0;std::vector<uint16_t> bytes;float at(size_t plane,size_t n,size_t c)const{return half(bytes.at(plane*size_t(w)*h*4+n*4+c));}};
Image load(const std::string& path,size_t planes){Image out;std::ifstream f(path,std::ios::binary);require(bool(f),"Missing edit image");f.read(reinterpret_cast<char*>(&out.w),4);f.read(reinterpret_cast<char*>(&out.h),4);require(out.w&&out.h&&uint64_t(out.w)*out.h<=2048*2048,"Invalid extent");out.bytes.resize(size_t(out.w)*out.h*4*planes);f.read(reinterpret_cast<char*>(out.bytes.data()),out.bytes.size()*2);require(bool(f),"Incomplete edit image");return out;}
bool near(float a,float b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=.002f+.002f*std::abs(b);}
float display(float c){c=std::max(0.f,c);c=c/(1+c);return c<=.0031308f?12.92f*c:1.055f*std::pow(c,1.f/2.4f)-.055f;}
bool interior(const Image& mrt,size_t n){const size_t x=n%mrt.w,y=n/mrt.w;if(x==0||y==0||x+1==mrt.w||y+1==mrt.h)return false;const int code=int(mrt.at(2,n,3));if(!code)return false;for(auto d:{-int(mrt.w),-1,1,int(mrt.w)})if(int(mrt.at(2,size_t(int(n)+d),3))!=code)return false;return true;}
void comparePaths(const Image& f,const Image& d,const Image& mrt,bool needsUnlit=false){require(f.w==d.w&&f.h==d.h,"Path extents differ");std::vector<float> errors;float sum=0,unlit=0;size_t unlitSamples=0;for(size_t n=0;n<size_t(f.w)*f.h;++n)if(interior(mrt,n))for(size_t c=0;c<3;++c){float e=std::abs(display(f.at(0,n,c))-display(d.at(0,n,c)));errors.push_back(e);sum+=e;if(int(mrt.at(2,n,3))==2){unlit=std::max(unlit,e);++unlitSamples;}}require(!errors.empty()&&(!needsUnlit||unlitSamples>60),"Empty path/Unlit ROI");std::sort(errors.begin(),errors.end());const auto p99=errors[size_t(.99*(errors.size()-1))];require(sum/errors.size()<=1.f/255&&p99<=3.f/255&&unlit<=1.f/255,"Edited paths violate fixed WP02 ROI/tolerances");std::cout<<"EDIT_PATH_METRICS samples="<<errors.size()<<" mae="<<sum/errors.size()<<" p99="<<p99<<" unlitMax="<<unlit<<" unlitSamples="<<unlitSamples<<'\n';}

// Independent fixed fixture reference. No engine/SceneData resolver or shared
// request constants are linked into this checker. The captured VP/viewport are
// observations; expected model, geometry and material values are defined here.
constexpr size_t sampleCount=29;
struct Capture {std::array<uint32_t,4> roi{};std::array<float,112> matrices{};};
Capture capture(const std::string& prefix,size_t step){Capture out;const auto name=prefix+"-step"+std::to_string(step);std::ifstream r(name+".roi",std::ios::binary),m(name+".matrices",std::ios::binary);r.read(reinterpret_cast<char*>(out.roi.data()),16);m.read(reinterpret_cast<char*>(out.matrices.data()),sizeof(out.matrices));require(bool(r)&&bool(m),"Missing viewport/snapshot matrix evidence");return out;}
std::array<float,16> model(size_t step,size_t instance){
    // Initial fixture bounds [-3.3,-.6,0]..[3.3,2.2,0], center (0,.8,0).
    const float fit=3.f/std::sqrt(6.6f*6.6f+2.8f*2.8f);
    const bool edited=step>=(instance?14u:7u);const float angle=edited?(instance?-11.f:9.f)*3.14159265358979323846f/180.f:0.f;
    const float sx=edited?(instance?1.05f:.9f):1.f,sy=edited?(instance?.9f:.95f):1.f;
    const float y=edited?(instance?-.2f:.2f):0.f;
    return {fit*std::cos(angle)*sx,fit*std::sin(angle)*sx,0,0,-fit*std::sin(angle)*sy,fit*std::cos(angle)*sy,0,0,0,0,fit,0,fit*(instance?1.8f:-1.8f),fit*(y-.8f),0,1};
}
struct Material {std::array<float,3> base,emissive;float metallic,roughness;int code;};
Material reference(size_t step,size_t instance,size_t slot){
    Material out{slot?std::array<float,3>{.1f,.3f,.7f}:std::array<float,3>{.6f,.2f,.1f},{0,0,0},slot?.1f:.2f,slot?.7f:.5f,1};
    if(slot!=instance)return out;
    const size_t start=instance?8:1;
    if(step>=start)out.base=instance?std::array<float,3>{.8f,.15f,.6f}:std::array<float,3>{.15f,.65f,.25f};
    if(step>=start+1)out.metallic=instance?.7f:.85f;
    if(step>=start+2)out.roughness=instance?.25f:.2f;
    if(step>=start+3)out.emissive=instance?std::array<float,3>{.05f,.1f,.3f}:std::array<float,3>{.1f,.2f,.05f};
    if(step==start+4||(instance&&step>=17&&step<=19))out.code=2;
    return out;
}
struct Vertex {float x,y,w;std::array<float,3> world;};
using Triangle=std::array<Vertex,3>;
Triangle triangle(const Capture& capture,size_t step,size_t draw){
    const auto matrix=model(step,draw/3);const size_t primitive=draw%3;
    const float ox=primitive==0?-.8f:primitive==1?.8f:0.f,oy=primitive==2?1.5f:0.f;
    const std::array<std::array<float,2>,3> vertices{{{-.7f+ox,-.6f+oy},{.7f+ox,-.6f+oy},{ox,.7f+oy}}};
    Triangle out{};
    for(size_t v=0;v<3;++v){const auto& local=vertices[v];auto& dst=out[v];for(size_t c=0;c<3;++c)dst.world[c]=matrix[c]*local[0]+matrix[4+c]*local[1]+matrix[12+c];std::array<float,4> clip{};for(size_t c=0;c<4;++c)clip[c]=capture.matrices[c]*dst.world[0]+capture.matrices[4+c]*dst.world[1]+capture.matrices[8+c]*dst.world[2]+capture.matrices[12+c];require(clip[3]>0,"Reference fixture behind camera");dst.w=clip[3];dst.x=capture.roi[0]+(clip[0]/clip[3]+1.f)*.5f*capture.roi[2];dst.y=capture.roi[1]+(clip[1]/clip[3]+1.f)*.5f*capture.roi[3];}
    return out;
}
float cross(float ax,float ay,float bx,float by){return ax*by-ay*bx;}
bool inside(const Triangle& t,float x,float y,std::array<float,3>* weights=nullptr){
    const float area=cross(t[1].x-t[0].x,t[1].y-t[0].y,t[2].x-t[0].x,t[2].y-t[0].y);require(std::abs(area)>1,"Empty reference triangle");std::array<float,3> b{};
    for(size_t i=0;i<3;++i){const auto& a=t[(i+1)%3];const auto& c=t[(i+2)%3];const float edge=cross(c.x-a.x,c.y-a.y,x-a.x,y-a.y);b[i]=edge/area;if(b[i]<=0||std::abs(edge)/std::hypot(c.x-a.x,c.y-a.y)<=1.f)return false;}
    if(weights){float sum=0;for(size_t i=0;i<3;++i){b[i]/=t[i].w;sum+=b[i];}for(float& value:b)value/=sum;*weights=b;}return true;
}
template<class F>size_t visit(const Triangle& t,const Image& image,F check){
    const int x0=std::max(0,int(std::floor(std::min({t[0].x,t[1].x,t[2].x})))),x1=std::min(int(image.w),int(std::ceil(std::max({t[0].x,t[1].x,t[2].x}))));
    const int y0=std::max(0,int(std::floor(std::min({t[0].y,t[1].y,t[2].y})))),y1=std::min(int(image.h),int(std::ceil(std::max({t[0].y,t[1].y,t[2].y}))));size_t count=0;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){std::array<float,3> b{};if(inside(t,x+.5f,y+.5f,&b)){check(size_t(y)*image.w+x,b);++count;}}return count;
}
void validateReference(const Image& mrt,const Capture& state,size_t step){
    for(size_t draw=0;draw<6;++draw){const auto expected=model(step,draw/3);for(size_t c=0;c<16;++c)require(std::isfinite(state.matrices[16+draw*16+c])&&std::abs(state.matrices[16+draw*16+c]-expected[c])<=.00001f,"Snapshot T/R/S differs from independent expected model");const auto geometry=triangle(state,step,draw);const auto material=reference(step,draw/3,draw%3==2?1:0);const auto count=visit(geometry,mrt,[&](size_t n,const auto& bary){require(int(mrt.at(2,n,3))==material.code,"Independent reference geometry/ShadingModel missing");for(size_t c=0;c<3;++c){require(near(mrt.at(0,n,c),material.base[c])&&near(mrt.at(3,n,c),material.emissive[c]),"Independent scalar reference mismatch");float position=0;for(size_t v=0;v<3;++v)position+=bary[v]*geometry[v].world[c];require(near(mrt.at(2,n,c),position),"GPU world position differs from independent transformed triangle");}require(near(mrt.at(0,n,3),material.metallic)&&near(mrt.at(1,n,3),material.roughness),"PBR/Unlit lost independent metallic/roughness reference");});require(count>20,"Empty independent primitive ROI");}
}
}
int main(int argc,char** argv){try{
    require(argc==2,"Expected evidence prefix");const std::string prefix=argv[1];
    std::vector<Image> hdr;std::vector<Capture> states;for(size_t step=0;step<sampleCount;++step){hdr.push_back(load(prefix+"-step"+std::to_string(step)+".hdrbin",2));states.push_back(capture(prefix,step));}
    for(size_t step=0;step<sampleCount;++step){const auto& roi=states[step].roi;require(roi[2]>2&&roi[3]>2&&uint64_t(roi[0])+roi[2]<=hdr[step].w&&uint64_t(roi[1])+roi[3]<=hdr[step].h,"Invalid captured viewport");for(size_t c=0;c<16;++c)require(std::isfinite(states[step].matrices[c]),"Non-finite captured projection");for(size_t n=0;n<size_t(hdr[step].w)*hdr[step].h;++n)for(size_t plane=0;plane<2;++plane)for(size_t c=0;c<3;++c)require(std::isfinite(hdr[step].at(plane,n,c)),"Non-finite HDR/probe result");}
    for(size_t step=0;step<sampleCount;++step)for(size_t draw=0;draw<6;++draw){const auto expected=model(step,draw/3);for(size_t c=0;c<16;++c)require(std::isfinite(states[step].matrices[16+draw*16+c])&&std::abs(states[step].matrices[16+draw*16+c]-expected[c])<=.00001f,"Cross-path/resize model state differs from independent TRS reference");}
    // Every Deferred capture checks all six primitives against explicit fixed
    // scalar/model/world-position references, including both complete TRS edits.
    for(size_t step=0;step<sampleCount;++step)if(step!=15&&step!=18&&step!=24)validateReference(load(prefix+"-step"+std::to_string(step)+".mrtbin",4),states[step],step);
    size_t checked=0;
    for(size_t step=1;step<sampleCount;++step){if(!((step<=14)||step==17||step==20))continue;auto previous=load(prefix+"-step"+std::to_string(step-1)+".mrtbin",4),current=load(prefix+"-step"+std::to_string(step)+".mrtbin",4);require(current.w==previous.w&&current.h==previous.h,"Edit comparison extent changed");size_t affected=0,unchanged=0;float hdrChange=0;
        const size_t selectedInstance=step>=8?1:0;const bool trs=step==7||step==14;
        for(size_t draw=0;draw<6;++draw){const size_t instance=draw/3,slot=draw%3==2?1:0;const bool selected=instance==selectedInstance&&(trs||slot==selectedInstance);const auto geometry=triangle(states[step],step,draw);
            const size_t count=visit(geometry,current,[&](size_t n,const auto&){if(selected){for(size_t c=0;c<3;++c)hdrChange=std::max(hdrChange,std::abs(hdr[step].at(0,n,c)-hdr[step-1].at(0,n,c)));}else{for(size_t p=0;p<4;++p)for(size_t c=0;c<4;++c)require(near(current.at(p,n,c),previous.at(p,n,c)),"Unselected MRT slot/previously edited instance changed");for(size_t c=0;c<3;++c)require(near(hdr[step].at(0,n,c),hdr[step-1].at(0,n,c)),"Unselected HDR pixels changed");}});
            require(count>20,"Empty per-primitive affected/unselected ROI");if(selected)affected+=count;else unchanged+=count;
            if(selected&&trs)visit(triangle(states[step-1],step-1,draw),previous,[&](size_t n,const auto&){for(size_t c=0;c<3;++c)hdrChange=std::max(hdrChange,std::abs(hdr[step].at(0,n,c)-hdr[step-1].at(0,n,c)));});
        }
        require(affected>20&&unchanged>20,"Empty affected/unselected ROI");require(hdrChange>.003f,"Edit had no visible HDR response");checked+=affected;std::cout<<"EDIT_OPERATION_OK step="<<step<<" instance="<<selectedInstance<<" trs="<<trs<<" affected="<<affected<<" unchanged="<<unchanged<<" hdrChange="<<hdrChange<<'\n';
    }
    comparePaths(hdr[15],hdr[16],load(prefix+"-step16.mrtbin",4));
    comparePaths(hdr[18],hdr[19],load(prefix+"-step19.mrtbin",4),true);
    comparePaths(hdr[24],hdr[23],load(prefix+"-step23.mrtbin",4));
    // Probe renders the actual scene viewport. Outside it the Runtime clear
    // color is intentionally retained; the ROI is captured from FrameData,
    // never inferred from black/nonblack output.
    for(size_t step=23;step<=26;++step){const auto& roi=states[step].roi;require(roi[2]>2&&roi[3]>2&&roi[0]+roi[2]<=hdr[step].w&&roi[1]+roi[3]<=hdr[step].h,"Invalid fixed viewport ROI");for(uint32_t y=roi[1]+1;y+1<roi[1]+roi[3];++y)for(uint32_t x=roi[0]+1;x+1<roi[0]+roi[2];++x)for(size_t c=0;c<3;++c)require(hdr[step].at(1,size_t(y)*hdr[step].w+x,c)==0.f,"Clear GPU fallback is not black");}
    for(size_t step:{21u,27u}){float maximum=0;for(size_t n=0;n<size_t(hdr[step].w)*hdr[step].h;++n)for(size_t c=0;c<3;++c)maximum=std::max(maximum,hdr[step].at(1,n,c));require(maximum>0,"Imported environment probe stayed black");}
    require(hdr[21].bytes==hdr[22].bytes,"Failed HDR load changed GPU output");
    require(hdr[27].bytes==hdr[28].bytes,"Failed instance edit changed GPU output");
    require(hdr[26].w!=hdr[23].w||hdr[26].h!=hdr[23].h,"Clear lifecycle did not cover real resize");
    std::cout<<"KUENGINE_EDIT_PIXELS_OK edits=16 instances=2 samples=29 affected="<<checked<<" roi=independent-fixture-1px-eroded maeLimit=1/255 p99Limit=3/255 clearBlack=1 failedRetained=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"EDIT_PIXEL_FAILURE: "<<e.what()<<'\n';return 1;}}
