#include "core.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace sv;
static bool close(Vec3 a,Vec3 b){return dot(a-b,a-b)<1e-7f;}
void viewpointTests(){
    for(auto axis:{UpAxis::YDown,UpAxis::YUp,UpAxis::ZUp})for(float units:{1.f,.01f,.001f})for(float heading:{-2.8f,0.f,1.2f}){
        Settings settings{units,axis};Vec3 source{123.25f,-750.5f,39.f};Vec3 target=orient(source,axis)*units;
        ViewpointPose saved=captureViewpoint(target,heading,settings);assert(validViewpoint(saved));
        assert(dot(saved.position-source,saved.position-source)<1e-6f);
        assert(close(viewpointPosition(saved,settings),target));
        for(Vec3 head: {Vec3{0,1.7f,0},Vec3{-3,1.1f,7}})for(float headYaw:{-2.f,.4f,2.5f}){
            Navigation nav;nav.reset(head,headYaw,viewpointPosition(saved,settings),viewpointYaw(saved,settings));
            assert(close(nav.point(head),target));
            assert(close(yawRotate({0,0,-1},nav.yaw+headYaw),yawRotate({0,0,-1},heading)));
            // Restore changes only yaw + translation; physical vertical displacement is retained.
            assert(close(nav.point(head+Vec3{0,.2f,0}),target+Vec3{0,.2f,0}));
            auto again=captureViewpoint(nav.point(head),nav.yaw+headYaw,settings);
            assert(close(again.forward,saved.forward));
        }
    }
    assert(std::abs(headingYaw({0,1,0},{0,0,-1})-pi/2)<1e-5f);
    assert(std::abs(headingYaw({.00001f,1,0},{0,0,-1})-pi/2)<1e-5f);
    assert(viewpointYaw({{0,0,0},{0,1,0}},{1,UpAxis::YUp})==0);
    assert(!validViewpoint({{0,0,0},{0,0,0}}));
    assert(!validViewpoint({{std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,-1}}));
    assert(!validViewpoint({{1e12f,0,0},{0,0,-1}}));
    std::cout<<"PASS: saved View source/world roundtrips for all up axes and scales; tracked-eye restore; heading fallback; invalid poses\n";
}
