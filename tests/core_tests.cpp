#include "core.h"
#include "load-spz.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <zlib.h>
using namespace sv;
void visibilityTests();
void viewpointTests();
bool near(float a,float b,float e=0.001f){return std::abs(a-b)<e;}
void rejects(const std::string&path){bool failed=false;try{loadScene(path);}catch(const std::exception&){failed=true;}assert(failed);}
int main(int argc,char**argv){
    visibilityTests();
    viewpointTests();
    assert(argc>=2);std::string root=argv[1];
    auto room=loadScene(root+"/app/src/main/assets/Demo.ply");
    auto compressed=loadScene(root+"/tests/Demo.spz");
    assert(room.splats.size()==24841&&compressed.splats.size()==room.splats.size());
    for(size_t i=0;i<room.splats.size();i++)for(int j=0;j<3;j++)assert(near(room.splats[i].position[j],compressed.splats[i].position[j],0.002f));
    assert(std::isfinite(room.spawn.x)&&near(room.spawn.y,1.65f,0.2f));
    auto mm=loadScene(root+"/app/src/main/assets/Demo.ply",{0.001f,UpAxis::YDown});
    assert(near(mm.splats[100].position[0],room.splats[100].position[0]*0.001f));
    assert(near(mm.splats[100].covA[0],room.splats[100].covA[0]*0.000001f,1e-10f));
    auto indices=sortBackToFront(room,{0,0,0},{0,0,-1});
    for(size_t i=1;i<indices.size();i++)assert(room.splats[indices[i-1]].position[2]<=room.splats[indices[i]].position[2]);
    Scene subset;for(auto i:{3,17,100,2000})subset.splats.push_back(room.splats[i]);
    assert((matchSubset(room,subset)==std::vector<uint32_t>{3,17,100,2000}));
    auto unchanged=room.splats[17];subset.splats[1].sh[0][0]+=0.125f;
    bool differentRejected=false;try{matchSubset(room,subset);}catch(const std::exception&){differentRejected=true;}assert(differentRejected);
    subset.splats[1]=unchanged;std::reverse(subset.splats.begin(),subset.splats.end());
    bool orderRejected=false;try{matchSubset(room,subset);}catch(const std::exception&){orderRejected=true;}assert(orderRejected);
    bool sameRejected=false;try{matchSubset(room,room);}catch(const std::exception&){sameRejected=true;}assert(sameRejected);
    Navigation n;Vec3 physical{1,1.7f,2};n.reset(physical,0,{10,2,20},0);
    assert(near(n.point(physical).x,10)&&near(n.point(physical).z,20));
    n.update(0.01f,physical,{0,0,-1},0,0,1,0,true);
    assert(near(n.point(physical).x,10)&&near(n.point(physical).z,20)&&near(n.yaw,-pi/6));
    n.update(0.01f,physical,{0,0,-1},0,0,1,0,true);assert(near(n.yaw,-pi/6));
    auto old=n.offset;n.update(0.03f,physical,{0,0,-1},1,1,1,1,false);assert(near(n.offset.x,old.x)&&near(n.offset.z,old.z));
    Navigation a,b;for(int i=0;i<72;i++)a.update(1.f/72,{}, {0,0,-1},0,1,0,0,true);
    for(int i=0;i<90;i++)b.update(1.f/90,{}, {0,0,-1},0,1,0,0,true);assert(near(a.offset.z,-1.5f)&&near(a.offset.z,b.offset.z));
    std::filesystem::path temp=std::filesystem::temp_directory_path()/"splativerse-core-tests";std::filesystem::create_directories(temp);
    auto bad=(temp/"bad.ply").string();{std::ofstream f(bad);f<<"ply\nformat ascii 1.0\nelement vertex "<<(maxSplats+1)<<"\nend_header\n";}rejects(bad);
    {std::ofstream f(bad);f<<"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nend_header\n0\n";}rejects(bad);
    auto splat=(temp/"one.splat").string();{std::ofstream f(splat,std::ios::binary);float v[]={1,2,3,0.1f,0.2f,0.3f};f.write((char*)v,sizeof(v));uint8_t c[]={255,0,128,255,255,128,128,128};f.write((char*)c,8);}
    auto one=loadScene(splat);assert(one.splats.size()==1&&near(one.splats[0].position[1],-2)&&near(one.splats[0].covB[2],0.09f));
    {std::ofstream f(splat,std::ios::app);f<<'x';}rejects(splat);
    auto ascii=(temp/"sh3.ply").string();
    {std::ofstream f(ascii);f<<"ply\nformat ascii 1.0\nelement vertex 1\n";
        for(auto name:{"x","y","z","scale_0","scale_1","scale_2","rot_0","rot_1","rot_2","rot_3","opacity","f_dc_0","f_dc_1","f_dc_2"})f<<"property float "<<name<<'\n';
        for(int i=0;i<45;i++)f<<"property float f_rest_"<<i<<'\n';
        f<<"end_header\n1 2 3 0 0 0 1 0 0 0 0 0 0 0 ";for(int i=0;i<45;i++)f<<i*0.01<<' ';f<<'\n';}
    auto harmonic=loadScene(ascii,{1,UpAxis::ZUp});auto& h=harmonic.splats[0];
    assert(near(h.position[0],1)&&near(h.position[1],3)&&near(h.position[2],-2));
    assert(h.covA[3]==3&&near(h.position[3],0.5f));
    assert(near(h.sh[1][0],0)&&near(h.sh[1][1],0.15f)&&near(h.sh[15][2],0.44f));
    // A declared full scan must not silently become a partial scan.
    {std::ifstream in(ascii);std::string contents((std::istreambuf_iterator<char>(in)),{});contents=contents.substr(0,contents.find("end_header")+11);std::ofstream out(bad);out<<contents;}rejects(bad);
    auto brokenSpz=(temp/"truncated.spz").string();{std::ifstream in(root+"/tests/Demo.spz",std::ios::binary);char bytes[32];in.read(bytes,32);std::ofstream out(brokenSpz,std::ios::binary);out.write(bytes,32);}rejects(brokenSpz);
    auto bomb=(temp/"oversized.spz").string();{gzFile out=gzopen(bomb.c_str(),"wb");assert(out);std::array<char,65536>zero{};for(int i=0;i<530;i++)assert(gzwrite(out,zero.data(),zero.size())==int(zero.size()));assert(gzclose(out)==Z_OK);}rejects(bomb);
    // A valid SH3 scan exceeds both the old 200k point and 32 MiB inflation limits.
    // Every coefficient is populated so all three harmonic bands are checked.
    const size_t largeCount=530409;
    spz::PackedGaussians packed;packed.numPoints=largeCount;packed.shDegree=3;packed.fractionalBits=12;packed.antialiased=true;
    packed.positions.resize(largeCount*9,0);packed.alphas.resize(largeCount,255);
    packed.colors.resize(largeCount*3,128);packed.scales.resize(largeCount*3,128);
    packed.rotations.resize(largeCount*4,0);packed.sh.resize(largeCount*45,144);
    std::ostringstream packedStream;spz::serializePackedGaussians(packed,&packedStream);
    std::string raw=packedStream.str();assert(raw.size()==16+65*largeCount&&raw.size()>32*1024*1024);
    auto validSpz=(temp/"large-aa.spz").string();
    auto writeGzip=[&](const std::string&bytes){gzFile out=gzopen(validSpz.c_str(),"wb");assert(out);assert(gzwrite(out,bytes.data(),unsigned(bytes.size()))==int(bytes.size()));assert(gzclose(out)==Z_OK);};
    writeGzip(raw);
    {auto scene=loadScene(validSpz);assert(scene.antialiased&&scene.splats.size()==largeCount);
      for(auto index:{size_t(0),largeCount-1}){const auto&g=scene.splats[index];assert(near(g.position[3],1)&&g.covA[3]==3);assert(g.covA[0]>0&&std::isfinite(g.covA[0]));for(int k=1;k<16;k++)for(int c=0;c<3;c++)assert(near(std::abs(g.sh[k][c]),0.125f));}}
    assert(!room.antialiased&&!compressed.antialiased);
    // Header failures must be rejected before allocating a declared huge payload.
    auto header=raw.substr(0,16);
    for(auto field:std::vector<std::pair<size_t,uint8_t>>{{0,0},{4,4},{12,4},{13,25},{14,2}}){auto corrupt=header;corrupt[field.first]=field.second;writeGzip(corrupt);rejects(validSpz);}
    for(uint32_t count:{0u,uint32_t(maxSplats+1),0xffffffffu}){auto corrupt=header;std::memcpy(corrupt.data()+8,&count,4);writeGzip(corrupt);rejects(validSpz);}
    writeGzip(raw.substr(0,raw.size()-1));rejects(validSpz);
    writeGzip(raw+"x");rejects(validSpz);
    writeGzip(raw);{std::fstream f(validSpz,std::ios::binary|std::ios::in|std::ios::out);f.seekg(-8,std::ios::end);char c;f.read(&c,1);c^=1;f.seekp(-8,std::ios::end);f.write(&c,1);}rejects(validSpz);
    writeGzip(raw);{std::ofstream out(validSpz,std::ios::binary|std::ios::app);out<<'x';}rejects(validSpz);
    Scene aaSubset;aaSubset.antialiased=true;aaSubset.splats.push_back(room.splats[0]);
    bool aaMismatchRejected=false;try{matchSubset(room,aaSubset);}catch(const std::exception&){aaMismatchRejected=true;}assert(aaMismatchRejected);
    for(int arg=2;arg<argc;arg++){
      uint8_t headerBytes[16];gzFile in=gzopen(argv[arg],"rb");assert(in&&gzread(in,headerBytes,16)==16);gzclose(in);
      uint32_t count;std::memcpy(&count,headerBytes+8,4);auto scene=loadScene(argv[arg]);
      assert(scene.splats.size()==count&&scene.antialiased==bool(headerBytes[14]&1));
      for(const auto&g:scene.splats){for(float f:g.position)assert(std::isfinite(f));for(float f:g.covA)assert(std::isfinite(f));for(float f:g.covB)assert(std::isfinite(f));for(const auto&band:g.sh)for(float f:band)assert(std::isfinite(f));}
      std::cout<<"PASS corpus: "<<argv[arg]<<"; splats="<<count<<"; antialiased="<<scene.antialiased<<std::endl;
    }
    std::filesystem::remove_all(temp);
    std::cout<<"PASS: comparison and AA mismatch rejection; demo PLY/SPZ parity; unit/covariance conversion; depth sorting; spawn; navigation; malformed/oversized/truncated inputs; SPLAT decoding; SH3 channel layout; Z-up; 530409-splat AA SPZ above 32 MiB; bounded inflation; invalid headers, CRC, payload sizes and trailing data\n";
}
