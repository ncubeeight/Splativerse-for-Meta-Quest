#include "core.h"
#include <cassert>
#include <cstring>
#include <numeric>
#include <random>
#include <limits>

namespace {
std::vector<uint32_t> bruteForce(const sv::Scene&s,sv::Vec3 eye,sv::Vec3 forward,float radius){
    std::vector<uint32_t> expected;
    for(uint32_t i=0;i<s.splats.size();i++){
        const auto&p=s.splats[i].position;
        float x=p[0]-eye.x,y=p[1]-eye.y,z=p[2]-eye.z;
        if(radius==0||x*x+y*y+z*z<=radius*radius)expected.push_back(i);
    }
    auto depth=[&](uint32_t i){const auto&p=s.splats[i].position;return (p[0]-eye.x)*forward.x+(p[1]-eye.y)*forward.y+(p[2]-eye.z)*forward.z;};
    std::sort(expected.begin(),expected.end(),[&](uint32_t a,uint32_t b){return depth(a)==depth(b)?a<b:depth(a)>depth(b);});return expected;
}
}
void visibilityTests(){
    sv::Scene scene;std::mt19937 random(314159);std::uniform_real_distribution<float> coordinate(-500,500);
    for(int i=0;i<5000;i++){sv::Splat p;p.position[0]=coordinate(random);p.position[1]=coordinate(random);p.position[2]=coordinate(random);scene.splats.push_back(p);}
    // Exact boundary, identical points, distant outliers and zero-width axes.
    for(sv::Vec3 p:std::vector<sv::Vec3>{{30,0,0},{-30,0,0},{0,0,0},{0,0,0},{1e6,0,0},{-1e6,0,0}}){sv::Splat s;s.position[0]=p.x;s.position[1]=p.y;s.position[2]=p.z;scene.splats.push_back(s);}
    auto originals=scene.splats;sv::buildSpatialIndex(scene);
    assert(std::memcmp(originals.data(),scene.splats.data(),originals.size()*sizeof(sv::Splat))==0);
    auto permutation=scene.spatialOrder;std::sort(permutation.begin(),permutation.end());for(size_t i=0;i<permutation.size();i++)assert(permutation[i]==i);
    for(int sample=0;sample<24;sample++){
        sv::Vec3 eye=sample?sv::Vec3{coordinate(random),coordinate(random),coordinate(random)}:sv::Vec3{};
        sv::Vec3 forward=sv::unit({coordinate(random),coordinate(random),coordinate(random)});
        for(float radius:{0.f,30.f,60.f,120.f,360.f})assert(sv::sortBackToFront(scene,eye,forward,radius)==bruteForce(scene,eye,forward,radius));
    }
    assert(sv::sortBackToFront(scene,{3e6,3e6,3e6},{0,0,-1},30).empty());
    auto boundary=sv::sortBackToFront(scene,{}, {0,0,-1},30);assert(std::find(boundary.begin(),boundary.end(),5000)!=boundary.end()&&std::find(boundary.begin(),boundary.end(),5001)!=boundary.end());
    // Degenerate clouds must work, including a zero-live region followed by re-entry.
    sv::Scene flat;flat.splats.resize(1025);sv::buildSpatialIndex(flat);
    assert(sv::sortBackToFront(flat,{100,0,0},{0,0,-1},30).empty());
    assert(sv::sortBackToFront(flat,{}, {0,0,-1},30).size()==1025);
    flat.splats.clear();sv::buildSpatialIndex(flat);assert(flat.spatialOrder.empty()&&flat.spatialBlocks.empty());
    bool rejected=false;try{sv::sortBackToFront(scene,{}, {},std::numeric_limits<float>::quiet_NaN());}catch(...){rejected=true;}assert(rejected);
    // World-unit conversion scales both selection center and radius consistently.
    auto scaled=scene;for(auto&p:scaled.splats)for(int c=0;c<3;c++)p.position[c]*=0.01f;sv::buildSpatialIndex(scaled);
    assert(sv::sortBackToFront(scaled,{1,2,3},{0,0,-1},1.2f)==sv::sortBackToFront(scene,{100,200,300},{0,0,-1},120));
    // Both SH halves retain every RGB float bit-for-bit, including coefficient 8.
    sv::Splat colors;for(int k=0;k<16;k++)for(int c=0;c<4;c++)colors.sh[k][c]=float(k*4+c)/127.f;
    float low[sv::shFloatsPerHalf],high[sv::shFloatsPerHalf];sv::packHarmonics(colors,low,high);
    for(int k=0;k<16;k++)for(int c=0;c<3;c++){float actual=k<8?low[k*3+c]:high[(k-8)*3+c];assert(std::memcmp(&actual,&colors.sh[k][c],sizeof(float))==0);}
}
