#version 450
#extension GL_GOOGLE_include_directive : require
layout(std140,binding=2) uniform Camera {
 mat4 view; mat4 projection; vec4 eye; vec4 viewport; vec4 options; mat4 headToEye; vec4 bubble; vec4 renderInfo; vec4 feedback; vec4 menu; vec4 beamOrigin;
} camera;
#include "view_card.glsl"
layout(location=0) in vec2 uv;
layout(location=1) flat in int isBeam;
layout(location=0) out vec4 outColor;
#include "../native/menu_layout.h"
#include "controls_card.glsl"
void main(){
 if(isBeam==1){
  float alpha=(1.0-smoothstep(0.12,0.5,abs(uv.x-0.5)))*mix(0.18,0.8,uv.y);
  outColor=vec4(outputColor(vec3(0.46,0.90,0.71))*alpha,alpha);return;
 }
 int status=int(camera.options.w+0.5);
 outColor=status>=2&&status<8?viewCard(uv,status):controlsCard(uv);
}
