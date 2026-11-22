#version 450
#extension GL_GOOGLE_include_directive : require
#include "filter.glsl"
struct Splat {vec4 position;vec4 covA;vec4 covB;};
layout(std430,binding=0) readonly buffer Splats {Splat splats[];};
layout(std430,binding=3) readonly buffer HarmonicsLow {float coefficientsLow[];};
layout(std430,binding=4) readonly buffer HarmonicsHigh {float coefficientsHigh[];};
layout(std430,binding=1) readonly buffer Order {uint indices[];};
layout(std140,binding=2) uniform Camera {
    mat4 view;
    mat4 projection;
    vec4 eye;
    vec4 viewport; // width, height, source up-axis, sRGB framebuffer
    vec4 options; // filter: 0 classic / 1 pixel / 2 SPZ AA; reduced; comparison; HUD
    mat4 headToEye;
    vec4 bubble; // committed shared selection center xyz and radius (0 = full)
    vec4 renderInfo; // fade start, live draw count, requested radius, reserved
    vec4 feedback; // HUD opacity and elapsed time
    vec4 menu; // menu height, hovered button and pointer UV
    vec4 beamOrigin; // controller aim origin in head space and beam visibility
} camera;
layout(location=0) out vec2 gaussian;
layout(location=1) out vec4 color;
const vec2 corners[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));
vec3 sh(uint base,uint coefficient){
    uint offset=base+(coefficient%8)*3;
    return coefficient<8?vec3(coefficientsLow[offset],coefficientsLow[offset+1],coefficientsLow[offset+2]):vec3(coefficientsHigh[offset],coefficientsHigh[offset+1],coefficientsHigh[offset+2]);
}
vec3 evalSH(uint base,Splat s,vec3 d){
    float x=d.x,y=d.y,z=d.z;vec3 c=0.2820947918*sh(base,0);
    if(s.covA.w>=1){
        c+=0.4886025119*(-y*sh(base,1)+z*sh(base,2)-x*sh(base,3));
    }
    if(s.covA.w>=2){
        c+=1.0925484306*x*y*sh(base,4)-1.0925484306*y*z*sh(base,5);
        c+=0.3153915653*(2*z*z-x*x-y*y)*sh(base,6);
        c-=1.0925484306*x*z*sh(base,7);
        c+=0.5462742153*(x*x-y*y)*sh(base,8);
    }
    if(s.covA.w>=3){
        c-=0.5900435899*y*(3*x*x-y*y)*sh(base,9);
        c+=2.8906114426*x*y*z*sh(base,10);
        c-=0.4570457995*y*(4*z*z-x*x-y*y)*sh(base,11);
        c+=0.3731763326*z*(2*z*z-3*x*x-3*y*y)*sh(base,12);
        c-=0.4570457995*x*(4*z*z-x*x-y*y)*sh(base,13);
        c+=1.4453057213*z*(x*x-y*y)*sh(base,14);
        c-=0.5900435899*x*(x*x-3*y*y)*sh(base,15);
    }
    return clamp(c+0.5,0,1);
}
void main(){
    gaussian=vec2(0);color=vec4(0);gl_Position=vec4(2,2,2,1);
    uint logicalIndex=uint(gl_InstanceIndex)*1024+uint(gl_VertexIndex)/4;
    uint corner=uint(gl_VertexIndex)%4;
    if(logicalIndex>=uint(camera.renderInfo.y))return;
    if(camera.eye.w==1){
        if(logicalIndex>0)return;
        gaussian=corners[corner]*0.5;
        gl_Position=vec4(corners[corner]*0.25,0.5,1);color=vec4(0,1,0,1);return;
    }
    uint index=indices[logicalIndex];
    Splat s=splats[index];
    if(camera.options.y>0.5&&s.covB.w<0.5)return;
    vec3 p=(camera.view*vec4(s.position.xyz,1)).xyz;
    if(p.z>=-0.03||s.position.w<0.00392157)return;
    vec4 clip=camera.projection*vec4(p,1);
    if(clip.z<0||clip.z>clip.w)return;
    // Match the source renderer's guard band: the perspective Jacobian is
    // unreliable for near-camera splats far outside the eye's field of view.
    if(any(greaterThan(abs(clip.xy),vec2(1.2*clip.w))))return;
    if(camera.eye.w==2){
        gaussian=corners[corner];
        gl_Position=clip+vec4(corners[corner]*6/camera.viewport.xy*clip.w,0,0);color=vec4(1,1,1,1);return;
    }
    mat3 covariance=mat3(s.covA.x,s.covA.y,s.covA.z,
                         s.covA.y,s.covB.x,s.covB.y,
                         s.covA.z,s.covB.y,s.covB.z);
    mat3 viewCov=mat3(camera.view)*covariance*transpose(mat3(camera.view));
    // Differentiate the actual asymmetric OpenXR projection into pixel coordinates.
    float fx=camera.projection[0][0]*camera.viewport.x*0.5;
    float fy=camera.projection[1][1]*camera.viewport.y*0.5;
    float invZ=1.0/p.z;
    // Limit the derivative to a padded field of view, including its asymmetric
    // center. Otherwise off-axis splats can expand across the entire image.
    vec2 projectionScale=vec2(camera.projection[0][0],camera.projection[1][1]);
    vec2 projectionOffset=vec2(camera.projection[2][0],camera.projection[2][1]);
    vec2 ratioCenter=-projectionOffset/projectionScale;
    vec2 ratioLimit=1.3/abs(projectionScale);
    vec2 derivativePosition=clamp(p.xy*invZ,ratioCenter-ratioLimit,ratioCenter+ratioLimit)*p.z;
    vec3 jx=vec3(-fx*invZ,0,fx*derivativePosition.x*invZ*invZ);
    vec3 jy=vec3(0,-fy*invZ,fy*derivativePosition.y*invZ*invZ);
    float a=dot(jx,viewCov*jx);
    float b=dot(jx,viewCov*jy);
    float d=dot(jy,viewCov*jy);
    // Experimental one-pixel box variance, with integrated-opacity preservation.
    // Mode 0: classic 3DGS. Mode 1: experimental pixel filter.
    // Mode 2: SPZ antialiasing (0.3 variance with determinant compensation).
    float padding=camera.options.x>0.5&&camera.options.x<1.5?1.0/12.0:0.3;
    float opacityScale=filterOpacity(a,b,d,padding,camera.options.x>0.5);
    if(opacityScale*s.position.w<1.0/255.0)return;
    float mid=0.5*(a+d),disc=sqrt(max(0,0.25*(a-d)*(a-d)+b*b));
    float l1=max(0,mid+disc),l2=max(0,mid-disc);
    vec2 e1=abs(b)>0.000001?normalize(vec2(b,l1-a)):(a>=d?vec2(1,0):vec2(0,1));
    vec2 axis1=e1*sqrt(l1),axis2=vec2(-e1.y,e1.x)*sqrt(l2);
    if(any(isnan(axis1))||any(isinf(axis1))||any(isnan(axis2))||any(isinf(axis2)))return;
    // Reject only when the whole 3-sigma ellipse is outside the viewport.
    vec2 extent=3*(abs(axis1)+abs(axis2))*2/camera.viewport.xy;
    if(any(greaterThan(abs(clip.xy/clip.w),vec2(1)+extent)))return;
    vec2 uv=corners[corner];gaussian=uv*3;
    vec2 delta=3*(uv.x*axis1+uv.y*axis2)*2/camera.viewport.xy;
    gl_Position=clip+vec4(delta*clip.w,0,0);
    if(camera.eye.w==3){color=vec4(1,0,0,1);return;}
    vec3 dir=s.position.xyz-camera.eye.xyz;
    dir=length(dir)>1e-6?normalize(dir):vec3(0,0,1);
    if(camera.viewport.z<0.5)dir=vec3(dir.x,-dir.y,-dir.z);
    else if(camera.viewport.z>1.5)dir=vec3(dir.x,-dir.z,dir.y);
    vec3 rgb=evalSH(index*24,s,dir);
    if(camera.viewport.w>0.5)rgb=pow(rgb,vec3(2.2));
    float fade=camera.bubble.w>0?clamp((camera.bubble.w-distance(s.position.xyz,camera.bubble.xyz))/max(camera.bubble.w-camera.renderInfo.x,1e-6),0,1):1;
    color=vec4(rgb,s.position.w*opacityScale*fade);
}
