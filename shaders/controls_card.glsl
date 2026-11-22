// Control labels are small rectangles in the same Android text atlas as View cards.
float controlPixel(ivec2 p,ivec2 origin,ivec2 size){
 p=clamp(p,origin,origin+size-1);return textPixel(ivec2(p.x,p.y%256),p.y/256);
}
float controlBilinear(vec2 p,ivec2 origin,ivec2 size){
 p-=0.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);
 return mix(mix(controlPixel(i,origin,size),controlPixel(i+ivec2(1,0),origin,size),f.x),mix(controlPixel(i+ivec2(0,1),origin,size),controlPixel(i+ivec2(1,1),origin,size),f.x),f.y);
}
float controlText(vec2 pixel,ivec2 origin,ivec2 size,vec2 dx,vec2 dy){
 vec2 at=pixel+vec2(origin);
 return 0.25*(controlBilinear(at-dx-dy,origin,size)+controlBilinear(at+dx-dy,origin,size)+controlBilinear(at-dx+dy,origin,size)+controlBilinear(at+dx+dy,origin,size));
}
bool inside(vec2 p,vec2 size){return all(greaterThanEqual(p,vec2(0)))&&all(lessThan(p,size));}
vec4 controlsCard(vec2 at){
 vec2 p=at*vec2(SV_MENU_PIXELS_X,SV_MENU_PIXELS_Y),dx=dFdx(p)*0.25,dy=dFdy(p)*0.25;float aa=max(fwidth(p.y),0.5);
 bool comparison=camera.options.z>0.5;bool views=int(camera.options.w+0.5)==8;
 float edge=roundedBox(p-vec2(SV_MENU_PIXELS_X/2,192),vec2(SV_MENU_PIXELS_X/2-32,168),28);
 float shadow=exp(-max(roundedBox(p-vec2(SV_MENU_PIXELS_X/2,198),vec2(SV_MENU_PIXELS_X/2-32,168),28),0.0)/8.0)*0.24;
 vec4 result=vec4(0,0,0,shadow);float panel=coverage(edge,aa);
 result=over(outputColor(mix(vec3(0.090,0.145,0.130),vec3(0.055,0.090,0.083),at.y)),panel*0.94,result);
 result=over(outputColor(vec3(0.38,0.53,0.46)),coverage(abs(edge)-0.65,aa)*0.28,result);
 // Small key caps separate the physical controls from their actions.
 float keyDistance=roundedBox(p-vec2(830,66),vec2(20,22),10);
 if(!views)keyDistance=min(keyDistance,roundedBox(p-vec2(395,66),vec2(20,22),10));
 for(int i=0;i<3&&!views;i++){
  keyDistance=min(keyDistance,roundedBox(p-vec2(130,126+i*80),vec2(62,22),12));
  keyDistance=min(keyDistance,roundedBox(p-vec2(596,126+i*80),vec2(62,22),12));
 }
 float key=coverage(keyDistance,aa);
 result=over(outputColor(vec3(0.46,0.90,0.71)),key*0.09,result);
 float separator=coverage(abs(p.y-96.0)-0.5,aa)*step(72.0,p.x)*step(p.x,952.0);
 result=over(outputColor(vec3(0.38,0.53,0.46)),separator*0.16,result);
 float text=inside(p,vec2(1024,384))?controlText(p,ivec2(0,views?3584:(comparison?1920:1536)),ivec2(1024,384),dx,dy):0;
 vec2 label=p-vec2(72,37);
 if(!views&&inside(label,vec2(340,64))){int title=comparison?(camera.options.y>0.5?2:1):0;text=max(text,controlText(label,ivec2(0,3328+title*64),ivec2(340,64),dx,dy));}
 label=p-vec2(216,254);
 if(!views&&inside(label,vec2(300,64))){
  int radius=int(camera.bubble.w+0.5);int row=radius==0?0:radius==30?1:clamp(radius/60+1,2,7);
  if(camera.renderInfo.z!=camera.bubble.w)row=8;
  text=max(text,controlText(label,ivec2(0,2304+row*64),ivec2(300,64),dx,dy));
 }
 label=p-vec2(682,254);
 if(!views&&inside(label,vec2(270,64))){int filterIndex=clamp(int(camera.options.x+0.5),0,2);text=max(text,controlText(label,ivec2(0,3072+filterIndex*64),ivec2(270,64),dx,dy));}
 if(views){
  for(int i=0;i<int(camera.feedback.x);i++){
   float row=roundedBox(p-vec2(528,130+i*80),vec2(456,28),10);
   bool hover=int(camera.menu.y+0.5)==i+4;
   result=over(outputColor(vec3(0.46,0.90,0.71)),coverage(row,aa)*(hover?0.22:0.05),result);
  }
 }
 vec3 ink=key>0.5?vec3(0.46,0.90,0.71):vec3(0.88,0.93,0.90);
 result=over(outputColor(ink),text*panel,result);
 float divider=coverage(abs(p.x-1010.0)-0.5,aa)*step(48.0,p.y)*step(p.y,336.0);
 result=over(outputColor(vec3(0.38,0.53,0.46)),divider*0.25,result);
 for(int i=0;i<3;i++){
  vec2 q=p-vec2(SV_MENU_BUTTON_X,SV_MENU_BUTTON_FIRST_Y+i*SV_MENU_BUTTON_SPACING);
  float shape=roundedBox(q,vec2(SV_MENU_BUTTON_HALF),12);
  bool disabled=views?((i==0&&camera.feedback.y<0.5)||(i==2&&camera.feedback.z<0.5)):((i==0&&camera.menu.x>=SV_MENU_MAX_STEP*SV_MENU_STEP-0.001)||(i==2&&camera.menu.x<=SV_MENU_MIN_STEP*SV_MENU_STEP+0.001));
  bool hover=int(camera.menu.y+0.5)==i+1&&!disabled;
  vec3 accent=i==1?vec3(0.88,0.93,0.90):vec3(0.46,0.90,0.71);
  result=over(outputColor(accent),coverage(shape,aa)*(hover?0.24:0.07),result);
  result=over(outputColor(accent),coverage(abs(shape)-0.7,aa)*(hover?0.75:0.25),result);
  float mark;
  if(i==1)mark=min(segment(q,vec2(-10,-10),vec2(10,10)),segment(q,vec2(-10,10),vec2(10,-10)))-2.2;
  else {if(i==2)q.y=-q.y;mark=min(segment(q,vec2(-13,4),vec2(0,-9)),segment(q,vec2(0,-9),vec2(13,4)))-2.4;}
  result=over(outputColor(accent),coverage(mark,aa)*(disabled?0.25:0.95),result);
 }
 if(camera.menu.z>=0.0&&camera.menu.w>=0.0){
  float d=length(p-camera.menu.zw*vec2(SV_MENU_PIXELS_X,SV_MENU_PIXELS_Y));
  result=over(outputColor(vec3(0.02,0.04,0.03)),coverage(d-8.0,aa)*0.9,result);
  result=over(outputColor(vec3(0.82,1.0,0.91)),coverage(d-4.5,aa),result);
 }
 return result;
}
