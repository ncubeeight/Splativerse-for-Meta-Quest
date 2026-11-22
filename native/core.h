#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "splativerse-limits.h"

namespace sv {
constexpr size_t maxSplats = svlimits::maxPoints;
constexpr size_t maxFileBytes = svlimits::maxFileBytes;
constexpr float pi = 3.14159265358979323846f;
struct Vec3 { float x=0,y=0,z=0; };
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 unit(Vec3 v){float l=std::sqrt(dot(v,v));return l>1e-6f?v*(1/l):Vec3{0,0,-1};}
inline Vec3 yawRotate(Vec3 p,float a){return {std::cos(a)*p.x+std::sin(a)*p.z,p.y,-std::sin(a)*p.x+std::cos(a)*p.z};}
inline float deadzone(float x){return std::abs(x)<0.18f?0:std::copysign((std::abs(x)-0.18f)/0.82f,x);}
// GPU std430 layout; SH coefficients remain in the source coordinate basis.
struct alignas(16) Splat {
    float position[4]{};  // xyz, opacity
    float covA[4]{};      // xx, xy, xz, SH degree
    float covB[4]{};      // yy, yz, zz, unused
    float sh[16][4]{};
};
static_assert(sizeof(Splat)==304);
// Tight RGB storage: SH0–7 and SH8–15, without each vec4's unused fourth float.
constexpr size_t shFloatsPerHalf=8*3;
inline void packHarmonics(const Splat&s,float*low,float*high){
    for(size_t k=0;k<8;k++)for(size_t c=0;c<3;c++){low[k*3+c]=s.sh[k][c];high[k*3+c]=s.sh[k+8][c];}
}
enum class UpAxis { YDown=0, YUp=1, ZUp=2 };
struct Settings { float units=1; UpAxis up=UpAxis::YDown; };
inline Vec3 orient(Vec3 p,UpAxis up){
    if(up==UpAxis::YDown)return {p.x,-p.y,-p.z};
    if(up==UpAxis::ZUp)return {p.x,p.z,-p.y};
    return p;
}
// Portable View poses use decoded source coordinates, before units/up-axis transforms.
struct ViewpointPose { Vec3 position{},forward{0,0,-1}; };
inline Vec3 inverseOrient(Vec3 p,UpAxis up){
    if(up==UpAxis::ZUp)return {p.x,-p.z,p.y};
    return orient(p,up);
}
inline bool validViewpoint(const ViewpointPose&p){
    for(float f:{p.position.x,p.position.y,p.position.z,p.forward.x,p.forward.y,p.forward.z})
        if(!std::isfinite(f)||std::abs(f)>=1e12f)return false;
    return dot(p.forward,p.forward)>1e-10f;
}
inline float headingYaw(Vec3 forward,Vec3 right={1,0,0}){
    if(forward.x*forward.x+forward.z*forward.z<0.0001f)forward={right.z,0,-right.x};
    return std::atan2(-forward.x,-forward.z);
}
inline ViewpointPose captureViewpoint(Vec3 worldEye,float worldYaw,Settings settings){
    return {inverseOrient(worldEye*(1/settings.units),settings.up),
            unit(inverseOrient(yawRotate({0,0,-1},worldYaw),settings.up))};
}
inline Vec3 viewpointPosition(const ViewpointPose&p,Settings s){return orient(p.position,s.up)*s.units;}
inline float viewpointYaw(const ViewpointPose&p,Settings s){auto d=orient(p.forward,s.up);return d.x*d.x+d.z*d.z>1e-10f?std::atan2(-d.x,-d.z):0;}
struct SpatialBlock {uint32_t begin=0,end=0;Vec3 low{},high{};};
struct Scene { std::vector<Splat> splats; std::vector<uint32_t> previewIndices; Vec3 spawn{}; float spawnYaw=0; Settings settings; bool antialiased=false;
    // Index-only locality ordering preserves the original records and comparison IDs.
    std::vector<uint32_t> spatialOrder;std::vector<SpatialBlock> spatialBlocks;
};
void buildSpatialIndex(Scene&scene);
Scene loadScene(const std::string& path, Settings settings={});
// Both versions share the full scene's coordinates, spawn, and sort order.
Scene loadComparison(const std::string& fullPath,const std::string& previewPath,Settings settings={});
std::vector<uint32_t> matchSubset(const Scene& full,const Scene& preview);
void estimateSpawn(Scene& scene);
// radius=0 retains the full scan; other radii are in transformed world metres.
std::vector<uint32_t> sortBackToFront(const Scene& scene,Vec3 eye,Vec3 forward,float radius=0);
struct Navigation {
    Vec3 offset{}; float yaw=0; bool turnLatched=false;
    Vec3 point(Vec3 physical)const{return yawRotate(physical,yaw)+offset;}
    void reset(Vec3 physicalHead,float headYaw,Vec3 spawn,float spawnYaw){
        yaw=spawnYaw-headYaw; offset=spawn-yawRotate(physicalHead,yaw);turnLatched=false;
    }
    void update(float dt,Vec3 physicalHead,Vec3 headForward,float x,float y,float turn,float vertical,bool focused){
        if(!focused){turnLatched=false;return;}
        dt=std::clamp(dt,0.f,0.05f);
        Vec3 f=unit(yawRotate(headForward,yaw));
        Vec3 r=unit(Vec3{-f.z,0,f.x});
        float sx=deadzone(x),sy=deadzone(y), mag=std::sqrt(sx*sx+sy*sy);
        if(mag>1){sx/=mag;sy/=mag;}
        offset=offset+(f*sy+r*sx+Vec3{0,deadzone(vertical),0})*(1.5f*dt);
        if(std::abs(turn)<0.3f)turnLatched=false;
        if(!turnLatched&&std::abs(turn)>0.7f){
            Vec3 pivot=point(physicalHead);yaw-=std::copysign(pi/6,turn);
            offset=pivot-yawRotate(physicalHead,yaw);turnLatched=true;
        }
    }
};
}
