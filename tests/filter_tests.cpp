#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
// Compile the actual scalar GLSL helper with only syntax/type substitutions.
using std::abs;using std::max;using std::clamp;using std::sqrt;using std::isnan;using std::isinf;
#include "filter-port.h"
int main(){
    std::mt19937 random(42);std::uniform_real_distribution<float> exponent(-12,30),correlation(-0.9f,0.9f);
    for(int sample=0;sample<20000;sample++){
        float a=std::pow(10.f,exponent(random)),d=std::pow(10.f,exponent(random));
        float b=float(correlation(random)*std::sqrt(double(a)*d)),v=sample%2?0.3f:1.f/12;
        const long double determinant=(long double)a*d-(long double)b*b;
        const long double filtered=((long double)a+v)*((long double)d+v)-(long double)b*b;
        const float expected=float(std::sqrt(std::max(0.L,determinant)/filtered));
        float aa=a,bb=b,dd=d;float actual=filterOpacity(aa,bb,dd,v,true);
        assert(std::isfinite(actual)&&actual>=0&&actual<=1&&std::abs(actual-expected)<0.0003f);
        assert(aa==a+v&&dd==d+v&&bb==b);
        aa=a;bb=b;dd=d;assert(filterOpacity(aa,bb,dd,v,false)==1);
    }
    for(float invalid:{-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){float a=invalid,b=0,d=1;assert(filterOpacity(a,b,d,.3f,true)==0);}
    float a=0,b=0,d=0;assert(filterOpacity(a,b,d,.3f,true)==0);
    std::cout<<"PASS: 20,000 covariance cases using the actual GLSL filter helper, classic preservation, invalid/degenerate inputs and overflow-scale ellipses\n";
}
