#version 450
#extension GL_GOOGLE_include_directive : require
#include "../native/menu_layout.h"
layout(std140,binding=2) uniform Camera {
 mat4 view; mat4 projection; vec4 eye; vec4 viewport; vec4 options; mat4 headToEye; vec4 bubble; vec4 renderInfo; vec4 feedback; vec4 menu; vec4 beamOrigin;
} camera;
layout(location=0) out vec2 uv;
const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
layout(location=1) flat out int isBeam;
void main(){
 uv=corners[gl_VertexIndex];isBeam=gl_InstanceIndex;
 if(isBeam==1){
  vec3 endpoint=vec3((camera.menu.z-0.5)*SV_MENU_WIDTH,SV_MENU_TOP+camera.menu.x-camera.menu.w*SV_MENU_HEIGHT,-SV_MENU_DEPTH);
  vec3 a=(camera.headToEye*vec4(camera.beamOrigin.xyz,1)).xyz;
  vec3 b=(camera.headToEye*vec4(endpoint,1)).xyz;
  // Clip the ribbon's origin in front of each eye before projecting it.
  if(a.z> -0.04)a=mix(a,b,clamp((-0.04-a.z)/(b.z-a.z),0.0,1.0));
  vec3 side=cross(b-a,a+b);float sideLength=length(side);
  side=sideLength>0.000001?side/sideLength:vec3(1,0,0);
  gl_Position=camera.projection*vec4(mix(a,b,uv.y)+side*(uv.x-0.5)*0.006,1);
 }else{
  bool feedback=camera.options.w>=2.0&&camera.options.w<8.0;
  vec3 p=feedback?vec3((uv.x-0.5)*1.04,-0.05-uv.y*0.26,-2.0):vec3((uv.x-0.5)*SV_MENU_WIDTH,SV_MENU_TOP+camera.menu.x-uv.y*SV_MENU_HEIGHT,-SV_MENU_DEPTH);
  gl_Position=camera.projection*camera.headToEye*vec4(p,1);
 }
}
