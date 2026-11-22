#pragma once
#include "core.h"
#include "menu_layout.h"
namespace sv {
struct MenuHit {float u=-1,v=-1;int button=0;};
inline MenuHit menuHit(Vec3 origin,Vec3 direction,int step,bool views=false){
    for(float f:{origin.x,origin.y,origin.z,direction.x,direction.y,direction.z})if(!std::isfinite(f))return {};
    if(direction.z>=-1e-5f)return {};
    float t=(-SV_MENU_DEPTH-origin.z)/direction.z;if(t<=0)return {};
    Vec3 p=origin+direction*t;
    float u=p.x/SV_MENU_WIDTH+0.5f,v=(SV_MENU_TOP+step*SV_MENU_STEP-p.y)/SV_MENU_HEIGHT;
    if(u<0||u>1||v<0||v>1)return {};
    MenuHit hit{u,v,0};float x=u*SV_MENU_PIXELS_X,y=v*SV_MENU_PIXELS_Y;
    float px=std::abs(x-SV_MENU_PIXELS_X/2)-(SV_MENU_PIXELS_X/2-32-28),py=std::abs(y-192)-(168-28);
    if(std::hypot(std::max(px,0.f),std::max(py,0.f))+std::min(std::max(px,py),0.f)>28)return {};
    for(int i=0;i<3;i++){
        // Same rounded-square footprint as the visual buttons, not their gaps.
        float qx=std::abs(x-SV_MENU_BUTTON_X)-(SV_MENU_BUTTON_HALF-12);
        float qy=std::abs(y-(SV_MENU_BUTTON_FIRST_Y+i*SV_MENU_BUTTON_SPACING))-(SV_MENU_BUTTON_HALF-12);
        float distance=std::hypot(std::max(qx,0.f),std::max(qy,0.f))+std::min(std::max(qx,qy),0.f)-12;
        if(distance<=0)hit.button=i+1;
    }
    if(views&&x>=72&&x<=984){
        for(int i=0;i<3;i++)if(y>=102+i*80&&y<=158+i*80)hit.button=4+i;
    }
    return hit;
}
struct MenuControls {
    int step=0;bool hidden=false,selectReady=false,restoreReady=false;
    void update(MenuHit hit,bool focused,bool selectable,bool selectDown,bool restoreDown){
        if(!focused){selectReady=restoreReady=false;return;}
        bool click=selectDown&&selectReady,restore=restoreDown&&restoreReady;
        selectReady=!selectDown;restoreReady=!restoreDown;
        if(restore)hidden=false;
        if(!hidden&&selectable&&click){
            if(hit.button==1)step=std::min(step+1,SV_MENU_MAX_STEP);
            if(hit.button==2)hidden=true;
            if(hit.button==3)step=std::max(step-1,SV_MENU_MIN_STEP);
        }
    }
};
}
