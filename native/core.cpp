#include "core.h"
#include "load-spz.h"
#include <cstring>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace sv {
namespace {
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
float sigmoid(float x){return x>=0?1/(1+std::exp(-x)):std::exp(x)/(1+std::exp(x));}
struct Raw { Vec3 p{},scale{1,1,1};float q[4]{1,0,0,0};float alpha=1;float sh[16][4]{};int degree=0; };
Splat encode(const Raw& r,Settings settings){
    Splat s{};Vec3 p=orient(r.p,settings.up)*settings.units;
    require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})<1e7,"Invalid or excessively large position.");
    require(std::isfinite(r.alpha)&&r.alpha>=0&&r.alpha<=1,"Invalid opacity.");
    s.position[0]=p.x;s.position[1]=p.y;s.position[2]=p.z;s.position[3]=r.alpha;
    float qn=0;for(float v:r.q)qn+=v*v;
    require(std::isfinite(qn)&&qn>1e-12,"Invalid Gaussian rotation.");
    float w=r.q[0]/std::sqrt(qn),x=r.q[1]/std::sqrt(qn),y=r.q[2]/std::sqrt(qn),z=r.q[3]/std::sqrt(qn);
    Vec3 c[3]={{1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)},
               {2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x)},
               {2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)}};
    float scales[]={r.scale.x,r.scale.y,r.scale.z};
    for(int i=0;i<3;i++){
        require(std::isfinite(scales[i])&&scales[i]>0&&scales[i]*settings.units<1e5,"Invalid Gaussian scale.");
        c[i]=orient(c[i],settings.up)*(scales[i]*settings.units);
    }
    for(auto v:c){s.covA[0]+=v.x*v.x;s.covA[1]+=v.x*v.y;s.covA[2]+=v.x*v.z;s.covB[0]+=v.y*v.y;s.covB[1]+=v.y*v.z;s.covB[2]+=v.z*v.z;}
    s.covA[3]=float(r.degree);
    for(int i=0;i<16;i++)for(int j=0;j<3;j++){
        require(std::isfinite(r.sh[i][j])&&std::abs(r.sh[i][j])<1e6,"Invalid spherical harmonic color.");s.sh[i][j]=r.sh[i][j];
    }
    return s;
}
size_t typeSize(const std::string&t){
    if(t=="char"||t=="uchar"||t=="int8"||t=="uint8")return 1;
    if(t=="short"||t=="ushort"||t=="int16"||t=="uint16")return 2;
    if(t=="int"||t=="uint"||t=="int32"||t=="uint32"||t=="float"||t=="float32")return 4;
    if(t=="double"||t=="float64")return 8;
    throw std::runtime_error("Unsupported PLY type: "+t);
}
template<class T> double readValue(std::istream&f){T v{};f.read(reinterpret_cast<char*>(&v),sizeof(v));require(bool(f),"Truncated scan data.");return double(v);}
double scalar(std::istream&f,const std::string&t,bool ascii){
    if(ascii){double v;require(bool(f>>v)&&std::isfinite(v),"Invalid ASCII PLY value.");return v;}
    if(t=="float"||t=="float32")return readValue<float>(f);
    if(t=="double"||t=="float64")return readValue<double>(f);
    if(t=="uchar"||t=="uint8")return readValue<uint8_t>(f);
    if(t=="char"||t=="int8")return readValue<int8_t>(f);
    if(t=="ushort"||t=="uint16")return readValue<uint16_t>(f);
    if(t=="short"||t=="int16")return readValue<int16_t>(f);
    if(t=="uint"||t=="uint32")return readValue<uint32_t>(f);
    return readValue<int32_t>(f);
}
void ply(std::ifstream&f,Scene&s){
    std::string line;std::getline(f,line);require(line=="ply"||line=="ply\r","Not a PLY file.");
    bool ascii=false,format=false,vertex=false,ended=false;size_t count=0,headerBytes=0;
    std::vector<std::pair<std::string,std::string>> props;
    while(std::getline(f,line)){
        headerBytes+=line.size();require(headerBytes<65536,"PLY header is too large.");
        std::istringstream in(line);std::string token;in>>token;
        if(token=="format"){std::string v;in>>v;require(v=="ascii"||v=="binary_little_endian","Use ASCII or little-endian PLY.");ascii=v=="ascii";format=true;}
        if(token=="element"){
            std::string name;uint64_t n;require(bool(in>>name>>n),"Malformed PLY element.");
            if(name=="vertex"){require(props.empty()&&count==0,"Duplicate vertex element.");count=n;vertex=true;}
            else {require(count>0||n==0,"PLY vertex element must come first.");vertex=false;}
        }
        if(token=="property"&&vertex){std::string type,name;in>>type>>name;require(type!="list","List properties on Gaussian vertices are unsupported.");typeSize(type);props.push_back({name,type});require(props.size()<=128,"Too many PLY properties.");}
        if(token=="end_header"){ended=true;break;}
    }
    require(format&&ended&&count>0,"Missing PLY header or empty scan.");
    require(count<=maxSplats,"This prototype accepts up to 1,000,000 splats; the source was not reduced.");
    std::map<std::string,size_t> index;
    for(size_t i=0;i<props.size();i++)require(index.emplace(props[i].first,i).second,"Duplicate PLY property.");
    for(auto name:{"x","y","z","scale_0","scale_1","scale_2","rot_0","rot_1","rot_2","rot_3","opacity"})require(index.count(name),std::string("Missing Gaussian property: ")+name);
    const bool dc=index.count("f_dc_0")&&index.count("f_dc_1")&&index.count("f_dc_2");
    require(dc||(index.count("red")&&index.count("green")&&index.count("blue")),"Missing Gaussian color.");
    int rest=0;while(index.count("f_rest_"+std::to_string(rest)))rest++;
    require(rest==0||rest==9||rest==24||rest==45,"Unsupported spherical harmonics layout.");
    for(const auto& item:index)if(item.first.rfind("f_rest_",0)==0)require(std::stoi(item.first.substr(7))<rest,"Non-contiguous spherical harmonics.");
    std::vector<double> values(props.size());s.splats.reserve(count);
    auto v=[&](const std::string&name){return float(values[index.at(name)]);};
    for(size_t i=0;i<count;i++){
        for(size_t j=0;j<props.size();j++)values[j]=scalar(f,props[j].second,ascii);
        Raw r;r.p={v("x"),v("y"),v("z")};r.alpha=sigmoid(v("opacity"));
        r.scale={std::exp(v("scale_0")),std::exp(v("scale_1")),std::exp(v("scale_2"))};
        for(int j=0;j<4;j++)r.q[j]=v("rot_"+std::to_string(j));
        r.degree=rest==45?3:rest==24?2:rest==9?1:0;
        const char*rgb[]={"red","green","blue"};
        for(int j=0;j<3;j++)r.sh[0][j]=dc?v("f_dc_"+std::to_string(j)):(v(rgb[j])/255.f-0.5f)/0.2820947918f;
        for(int j=0;j<rest/3;j++)for(int c=0;c<3;c++)r.sh[j+1][c]=v("f_rest_"+std::to_string(c*(rest/3)+j));
        s.splats.push_back(encode(r,s.settings));
    }
}
}
Scene loadScene(const std::string&path,Settings settings){
    require(std::isfinite(settings.units)&&settings.units>0,"Invalid source units.");
    std::ifstream f(path,std::ios::binary|std::ios::ate);require(bool(f),"Could not open scan.");
    auto length=f.tellg();require(length>0&&uint64_t(length)<=maxFileBytes,"Scan must be between 1 byte and 128 MiB for this prototype.");f.seekg(0);
    Scene s;s.settings=settings;
    auto ext=path.substr(path.find_last_of('.')+1);std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
    if(ext=="ply")ply(f,s);
    else if(ext=="splat"){
        require(length%32==0,"Truncated .splat record.");size_t count=size_t(length)/32;
        require(count<=maxSplats,"This prototype accepts up to 1,000,000 splats.");s.splats.reserve(count);
        for(size_t i=0;i<count;i++){
            Raw r;r.p={float(readValue<float>(f)),float(readValue<float>(f)),float(readValue<float>(f))};
            r.scale={float(readValue<float>(f)),float(readValue<float>(f)),float(readValue<float>(f))};
            for(int c=0;c<3;c++)r.sh[0][c]=(float(readValue<uint8_t>(f))/255-0.5f)/0.2820947918f;
            r.alpha=float(readValue<uint8_t>(f))/255;
            for(float&q:r.q)q=(float(readValue<uint8_t>(f))-128)/128;
            s.splats.push_back(encode(r,settings));
        }
    }else if(ext=="spz"){
        auto packed=spz::loadSpzPacked(path);
        require(packed.numPoints>0&&size_t(packed.numPoints)<=maxSplats,"Invalid, oversized, or unsupported SPZ (versions 1–3, up to 1,000,000 splats).");
        s.antialiased=packed.antialiased;
        auto converter=spz::coordinateConverter(spz::CoordinateSystem::RUB,spz::CoordinateSystem::RDF);
        s.splats.reserve(packed.numPoints);
        for(int i=0;i<packed.numPoints;i++){
            auto g=packed.unpack(i,converter);Raw r;r.p={g.position[0],g.position[1],g.position[2]};
            r.scale={std::exp(g.scale[0]),std::exp(g.scale[1]),std::exp(g.scale[2])};
            r.q[0]=g.rotation[3];for(int j=0;j<3;j++)r.q[j+1]=g.rotation[j];
            r.alpha=sigmoid(g.alpha);r.degree=packed.shDegree;
            for(int j=0;j<3;j++)r.sh[0][j]=g.color[j];
            int n=(r.degree+1)*(r.degree+1)-1;
            for(int j=0;j<n;j++){r.sh[j+1][0]=g.shR[j];r.sh[j+1][1]=g.shG[j];r.sh[j+1][2]=g.shB[j];}
            s.splats.push_back(encode(r,settings));
        }
    }else throw std::runtime_error("Choose a .ply, .spz, or .splat file.");
    estimateSpawn(s);buildSpatialIndex(s);return s;
}
std::vector<uint32_t> matchSubset(const Scene&full,const Scene&preview){
    require(full.antialiased==preview.antialiased,"Comparison scans must use the same antialiasing convention.");
    require(!preview.splats.empty()&&preview.splats.size()<full.splats.size(),"Choose a reduced copy with fewer splats than the full scan.");
    std::vector<uint32_t> indices;indices.reserve(preview.splats.size());size_t next=0;
    for(size_t i=0;i<full.splats.size()&&next<preview.splats.size();i++){
        if(std::memcmp(&full.splats[i],&preview.splats[next],sizeof(Splat))==0){indices.push_back(uint32_t(i));next++;}
    }
    require(next==preview.splats.size(),"Comparison needs a reduced copy with unchanged splats in the original order. Choose the supplied 200k preview.");
    return indices;
}
Scene loadComparison(const std::string&fullPath,const std::string&previewPath,Settings settings){
    Scene full=loadScene(fullPath,settings),preview=loadScene(previewPath,settings);
    full.previewIndices=matchSubset(full,preview);return full;
}
void estimateSpawn(Scene&s){
    require(!s.splats.empty(),"The scan is empty.");
    using Cell=std::tuple<int,int,int>;std::map<Cell,int> counts;std::vector<Vec3> samples;
    size_t step=std::max(size_t(1),(s.splats.size()+65535)/65536);
    auto cell=[](Vec3 p){return Cell{int(std::floor(p.x/4)),int(std::floor(p.y/4)),int(std::floor(p.z/4))};};
    for(size_t i=0;i<s.splats.size();i+=step){auto&p=s.splats[i].position;if(p[3]>=0.05f){Vec3 v{p[0],p[1],p[2]};samples.push_back(v);counts[cell(v)]++;}}
    require(!samples.empty(),"Scan has no visible splats.");Cell best{};int score=-1;
    for(auto&[c,n]:counts){auto[x,y,z]=c;int total=0;for(int a=-1;a<=1;a++)for(int b=-1;b<=1;b++)for(int d=-1;d<=1;d++){auto it=counts.find({x+a,y+b,z+d});if(it!=counts.end())total+=it->second;}if(total>score){score=total;best=c;}}
    auto[bx,by,bz]=best;Vec3 center{};std::vector<float> heights;std::vector<Vec3> local;
    for(auto p:samples){auto[x,y,z]=cell(p);if(std::abs(x-bx)<=1&&std::abs(y-by)<=1&&std::abs(z-bz)<=1){center=center+p;heights.push_back(p.y);local.push_back(p);}}
    center=center*(1.f/local.size());std::sort(heights.begin(),heights.end());float floor=heights[heights.size()/20];
    s.spawn={center.x,floor+1.65f,center.z};
    auto clearance=[&](Vec3 v){float best=1e10;for(auto p:local)if(p.y>floor+0.2f&&p.y<s.spawn.y+0.25f){float x=p.x-v.x,z=p.z-v.z;best=std::min(best,x*x+z*z);}return best;};
    float clear=clearance(s.spawn);
    if(clear<0.25f)for(float radius:{0.5f,1.f,1.5f,2.f,3.f}){for(int i=0;i<16;i++){float a=i*pi/8;Vec3 p{center.x+std::cos(a)*radius,s.spawn.y,center.z+std::sin(a)*radius};float c=clearance(p);if(c>clear){clear=c;s.spawn=p;}if(clear>=0.25f)break;}if(clear>=0.25f)break;}
    Vec3 d=center-s.spawn;if(d.x*d.x+d.z*d.z>0.0625f)s.spawnYaw=std::atan2(-d.x,-d.z);
}
void buildSpatialIndex(Scene&s){
    s.spatialOrder.clear();s.spatialBlocks.clear();if(s.splats.empty())return;
    // Vision Pro's Morton-local 512-point blocks, retaining original source IDs.
    double mean[3]{},m2[3]{};size_t n=0;
    for(const auto&p:s.splats){++n;for(int c=0;c<3;c++){double delta=p.position[c]-mean[c];mean[c]+=delta/n;m2[c]+=delta*(p.position[c]-mean[c]);}}
    double low[3],span[3];for(int c=0;c<3;c++){double sigma=std::sqrt(std::max(0.0,m2[c]/n));low[c]=mean[c]-2.5*sigma;span[c]=5*sigma;}
    struct Key {uint32_t morton,index;};std::vector<Key> keys;keys.reserve(n);
    for(uint32_t i=0;i<n;i++){
        uint32_t q[3],code=0;for(int c=0;c<3;c++)q[c]=span[c]>0?uint32_t(std::clamp((s.splats[i].position[c]-low[c])/span[c],0.0,1.0)*1023):0;
        for(int b=0;b<10;b++)for(int c=0;c<3;c++)code|=((q[c]>>b)&1u)<<(3*b+c);
        keys.push_back({code,i});
    }
    std::sort(keys.begin(),keys.end(),[](auto a,auto b){return a.morton==b.morton?a.index<b.index:a.morton<b.morton;});
    s.spatialOrder.reserve(n);for(auto key:keys)s.spatialOrder.push_back(key.index);
    for(uint32_t begin=0;begin<n;begin+=512){
        SpatialBlock block;block.begin=begin;block.end=uint32_t(std::min(size_t(begin)+512,n));
        auto&p=s.splats[s.spatialOrder[begin]].position;block.low=block.high={p[0],p[1],p[2]};
        for(uint32_t k=begin+1;k<block.end;k++){auto&v=s.splats[s.spatialOrder[k]].position;
            block.low={std::min(block.low.x,v[0]),std::min(block.low.y,v[1]),std::min(block.low.z,v[2])};
            block.high={std::max(block.high.x,v[0]),std::max(block.high.y,v[1]),std::max(block.high.z,v[2])};}
        s.spatialBlocks.push_back(block);
    }
}
std::vector<uint32_t> sortBackToFront(const Scene&s,Vec3 eye,Vec3 forward,float radius){
    require(std::isfinite(radius)&&radius>=0,"Invalid live radius.");
    struct Entry {uint32_t index;float depth;};std::vector<Entry> visible;
    const float squaredRadius=radius*radius;
    auto append=[&](uint32_t i){auto&p=s.splats[i].position;Vec3 delta=Vec3{p[0],p[1],p[2]}-eye;
        if(radius==0||dot(delta,delta)<=squaredRadius)visible.push_back({i,dot(delta,forward)});};
    if(radius>0&&!s.spatialBlocks.empty()){
        for(const auto&block:s.spatialBlocks){
            Vec3 closest{std::max({block.low.x-eye.x,eye.x-block.high.x,0.f}),std::max({block.low.y-eye.y,eye.y-block.high.y,0.f}),std::max({block.low.z-eye.z,eye.z-block.high.z,0.f})};
            if(dot(closest,closest)>squaredRadius)continue;
            for(uint32_t k=block.begin;k<block.end;k++)append(s.spatialOrder[k]);
        }
    }else{visible.reserve(s.splats.size());for(uint32_t i=0;i<s.splats.size();i++)append(i);}
    std::sort(visible.begin(),visible.end(),[](auto a,auto b){return a.depth==b.depth?a.index<b.index:a.depth>b.depth;});
    std::vector<uint32_t> indices;indices.reserve(visible.size());for(auto e:visible)indices.push_back(e.index);return indices;
}
}
