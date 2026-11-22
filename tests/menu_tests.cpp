#include "menu.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace sv;
MenuHit atPixel(float x,float y,int step=0,bool views=false){
 Vec3 target{float((x/SV_MENU_PIXELS_X-.5)*SV_MENU_WIDTH),float(SV_MENU_TOP+step*SV_MENU_STEP-y/SV_MENU_PIXELS_Y*SV_MENU_HEIGHT),-SV_MENU_DEPTH};
 Vec3 hand{.25f,-.3f,-.4f};return menuHit(hand,target-hand,step,views);
}
int main(){
 for(int step=SV_MENU_MIN_STEP;step<=SV_MENU_MAX_STEP;step++){
  for(int i=0;i<3;i++)assert(atPixel(SV_MENU_BUTTON_X,80+i*112,step).button==i+1);
  assert(atPixel(SV_MENU_BUTTON_X,136,step).button==0); // gap
  assert(atPixel(SV_MENU_BUTTON_X+35,80+35,step).button==0); // rounded corner
 }
 for(int step=SV_MENU_MIN_STEP;step<=SV_MENU_MAX_STEP;step++){
  for(int row=0;row<3;row++)assert(atPixel(300,130+row*80,step,true).button==4+row);
  assert(atPixel(300,170,step,true).button==0); // row gaps
  assert(atPixel(300,341,step,true).button==0); // footer
  assert(atPixel(1076,80,step,true).button==1); // page arrow remains separate
 }
 assert(atPixel(300,190).button==0);
 assert(menuHit({0,0,0},{0,1,0},0).u<0); // parallel
 assert(menuHit({0,0,0},{0,0,1},0).u<0); // away
 assert(menuHit({0,0,-3},{0,0,-1},0).u<0); // behind plane
 assert(menuHit({0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,-1},0).u<0);
 assert(atPixel(-1,100).u<0);
 assert(atPixel(10,100).u<0); // transparent panel margin must not show a beam
 assert(atPixel(33,25).u<0); // rounded panel corner
 assert(atPixel(600,200).u>=0);
 MenuControls m;MenuHit up=atPixel(SV_MENU_BUTTON_X,80),hide=atPixel(SV_MENU_BUTTON_X,192),down=atPixel(SV_MENU_BUTTON_X,304);
 auto release=[&]{m.update({},true,true,false,false);};
 m.update(up,true,true,true,false);assert(m.step==0); // held on first focus
 release();m.update(up,true,true,true,false);assert(m.step==1);
 for(int i=0;i<10;i++)m.update(up,true,true,true,false);assert(m.step==1); // no repeat
 m.update({},false,true,false,false);m.update(up,true,true,true,false);assert(m.step==1); // focus restore must release
 for(int i=0;i<20;i++){release();m.update(up,true,true,true,false);}assert(m.step==SV_MENU_MAX_STEP);
 for(int i=0;i<20;i++){release();m.update(down,true,true,true,false);}assert(m.step==SV_MENU_MIN_STEP);
 release();m.update(hide,true,true,true,false);assert(m.hidden);
 release();m.update(up,true,true,true,false);assert(m.step==SV_MENU_MIN_STEP); // hidden not pickable
 release();m.update({},true,true,false,true);assert(!m.hidden);
 release();m.update(up,true,false,true,false);assert(m.step==SV_MENU_MIN_STEP); // feedback card blocks picking
 release();m.update({},true,true,true,false);m.update(up,true,true,true,false);assert(m.step==SV_MENU_MIN_STEP); // dragging held trigger into button is not click
 std::cout<<"PASS: menu ray picking, limits, single-click steps, hide/restore, focus and feedback gating\n";
}
