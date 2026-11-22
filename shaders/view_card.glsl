// All shapes are analytic and antialiased at the rendered pixel footprint.
// Text is Android's antialiased sans-serif atlas, packed four alpha bytes/uint.
layout(std430,binding=5) readonly buffer ViewText {uint textAlpha[];};
float textPixel(ivec2 p,int row){
 p=clamp(p,ivec2(0),ivec2(1023,255));int index=(row*256+p.y)*1024+p.x;
 return float((textAlpha[index/4]>>uint((index&3)*8))&255u)/255.0;
}
float textBilinear(vec2 at,int row){
 vec2 p=at*vec2(1024,256)-0.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);
 return mix(mix(textPixel(i,row),textPixel(i+ivec2(1,0),row),f.x),mix(textPixel(i+ivec2(0,1),row),textPixel(i+ivec2(1,1),row),f.x),f.y);
}
float roundedBox(vec2 p,vec2 halfSize,float radius){vec2 q=abs(p)-halfSize+radius;return length(max(q,0.0))+min(max(q.x,q.y),0.0)-radius;}
float segment(vec2 p,vec2 a,vec2 b){vec2 d=b-a;return length(p-a-d*clamp(dot(p-a,d)/dot(d,d),0.0,1.0));}
float coverage(float distance,float aa){return 1.0-smoothstep(-aa,aa,distance);}
vec3 outputColor(vec3 srgb){return camera.viewport.w>0.5?pow(srgb,vec3(2.2)):srgb;}
vec4 over(vec3 color,float alpha,vec4 under){return vec4(color*alpha,alpha)+under*(1.0-alpha);}
vec4 viewCard(vec2 at,int status){
 vec2 p=(at-0.5)*vec2(4,1);float aa=max(fwidth(p.y),0.001);
 float edge=roundedBox(p,vec2(1.84,0.40),0.13);
 float shadow=exp(-max(roundedBox(p-vec2(0,0.025),vec2(1.84,0.40),0.13),0.0)*32.0)*0.24;
 vec4 result=vec4(0,0,0,shadow);
 vec3 accent=status==5?vec3(1.0,0.65,0.43):vec3(0.46,0.90,0.71);
 vec3 background=mix(vec3(0.090,0.145,0.130),vec3(0.055,0.090,0.083),at.y);
 float panel=coverage(edge,aa);
 result=over(outputColor(background),panel*0.97,result);
 float border=coverage(abs(edge)-0.0025,aa)*0.28;
 result=over(outputColor(vec3(0.38,0.53,0.46)),border,result);
 vec2 icon=(at-vec2(0.135,0.5))*vec2(4,1);
 result=over(outputColor(accent),coverage(length(icon)-0.155,aa)*0.12,result);
 float mark=1.0;
 if(status==3||status==4){mark=min(segment(icon,vec2(-0.068,0.0),vec2(-0.018,0.048)),segment(icon,vec2(-0.018,0.048),vec2(0.078,-0.053)))-0.014;}
 else if(status==5){mark=min(segment(icon,vec2(-0.047,-0.047),vec2(0.047,0.047)),segment(icon,vec2(-0.047,0.047),vec2(0.047,-0.047)))-0.013;}
 else if(status==2){mark=abs(length(icon)-0.076)-0.011;}
 else if(status==6){mark=min(segment(icon,vec2(-0.070,0.0),vec2(0.070,0.0)),min(segment(icon,vec2(-0.070,0.0),vec2(-0.015,-0.054)),segment(icon,vec2(-0.070,0.0),vec2(-0.015,0.054))))-0.012;}
 else{mark=min(segment(icon,vec2(-0.050,0.050),vec2(0.048,-0.048)),segment(icon,vec2(-0.060,0.060),vec2(-0.026,0.056)))-0.014;}
 float markAlpha=coverage(mark,aa);
 if(status==2)markAlpha*=0.25+0.75*pow(0.5+0.5*cos((dot(icon,icon)>1e-8?atan(icon.y,icon.x):0.0)-camera.feedback.y*4.0),3.0);
 result=over(outputColor(accent),markAlpha,result);
 // Four bilinear taps keep the lettering smooth as the panel is minified in VR.
 vec2 dx=dFdx(at)*0.25,dy=dFdy(at)*0.25;int row=clamp(status-2,0,5);
 float text=0.25*(textBilinear(at-dx-dy,row)+textBilinear(at+dx-dy,row)+textBilinear(at-dx+dy,row)+textBilinear(at+dx+dy,row));
 vec3 ink=at.y<0.53?vec3(0.94,0.97,0.95):vec3(0.66,0.75,0.71);
 result=over(outputColor(ink),text*panel,result);
 return result*clamp(camera.feedback.x,0.0,1.0);
}
