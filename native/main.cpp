#include "gpu.h"
#include "menu.h"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <chrono>
#include <future>
#include <thread>
#include <cstring>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sys/system_properties.h>

namespace {
struct AndroidState {bool resumed=false;};
void appCommand(android_app*app,int32_t command){auto*s=static_cast<AndroidState*>(app->userData);if(command==APP_CMD_RESUME)s->resumed=true;if(command==APP_CMD_PAUSE)s->resumed=false;}
struct Jni {
    android_app*app;JNIEnv*env=nullptr;jclass activityClass;jfloatArray hudStateArray;
    explicit Jni(android_app*a):app(a){a->activity->vm->AttachCurrentThread(&env,nullptr);activityClass=env->GetObjectClass(a->activity->clazz);hudStateArray=env->NewFloatArray(3);}
    ~Jni(){env->DeleteLocalRef(hudStateArray);env->DeleteLocalRef(activityClass);app->activity->vm->DetachCurrentThread();}
    std::string string(const char*name){auto method=env->GetMethodID(activityClass,name,"()Ljava/lang/String;");auto value=(jstring)env->CallObjectMethod(app->activity->clazz,method);if(!value)return {};const char*p=env->GetStringUTFChars(value,nullptr);std::string result=p;env->ReleaseStringUTFChars(value,p);env->DeleteLocalRef(value);return result;}
    void message(const std::string&message){auto text=env->NewStringUTF(message.c_str());env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"onComparisonStatus","(Ljava/lang/String;)V"),text);env->DeleteLocalRef(text);}
    int radius(){return env->CallIntMethod(app->activity->clazz,env->GetMethodID(activityClass,"getLiveRadius","()I"));}
    void saveRadius(int value){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"saveLiveRadius","(I)V"),jint(value));}
    int menuStep(){return env->CallIntMethod(app->activity->clazz,env->GetMethodID(activityClass,"getMenuHeightStep","()I"));}
    void saveMenuStep(int value){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"saveMenuHeightStep","(I)V"),jint(value));}
    bool pixelFilter(){return env->CallBooleanMethod(app->activity->clazz,env->GetMethodID(activityClass,"getPixelFilter","()Z"));}
    void savePixelFilter(bool value){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"savePixelFilter","(Z)V"),jboolean(value));}
    bool pose(const char*name,sv::ViewpointPose&out){
        auto array=(jfloatArray)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(activityClass,name,"()[F"));
        if(!array)return false;
        if(env->GetArrayLength(array)!=6){env->DeleteLocalRef(array);throw std::runtime_error("Invalid saved View.");}
        float v[6];env->GetFloatArrayRegion(array,0,6,v);env->DeleteLocalRef(array);out={{v[0],v[1],v[2]},{v[3],v[4],v[5]}};
        if(!sv::validViewpoint(out))throw std::runtime_error("Invalid saved View coordinates.");return true;
    }
    void saveView(const sv::ViewpointPose&p){float v[]={p.position.x,p.position.y,p.position.z,p.forward.x,p.forward.y,p.forward.z};auto array=env->NewFloatArray(6);env->SetFloatArrayRegion(array,0,6,v);env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"saveView","([F)V"),array);env->DeleteLocalRef(array);}
    std::array<float,3> viewHudState(){
        env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"getViewHudState","([F)V"),hudStateArray);
        std::array<float,3> state{};env->GetFloatArrayRegion(hudStateArray,0,3,state.data());return state;
    }
    std::vector<uint32_t> viewTextAtlas(){
        auto array=(jintArray)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(activityClass,"getViewTextAtlas","()[I"));
        if(env->ExceptionCheck()||!array){env->ExceptionClear();throw std::runtime_error("Could not prepare View confirmation text.");}
        const int expected=1024*256*16/4;if(env->GetArrayLength(array)!=expected){env->DeleteLocalRef(array);throw std::runtime_error("Invalid View text atlas.");}
        std::vector<uint32_t> data(expected);env->GetIntArrayRegion(array,0,expected,reinterpret_cast<jint*>(data.data()));env->DeleteLocalRef(array);return data;
    }
    int listRevision(){return env->CallIntMethod(app->activity->clazz,env->GetMethodID(activityClass,"pollViewList","()I"));}
    int listState(){return env->CallIntMethod(app->activity->clazz,env->GetMethodID(activityClass,"getViewListState","()I"));}
    void listOpen(bool open){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,open?"openViewList":"closeViewList","()V"));}
    bool listAction(int action,sv::ViewpointPose&out){
        auto array=(jfloatArray)env->CallObjectMethod(app->activity->clazz,env->GetMethodID(activityClass,"viewListAction","(I)[F"),jint(action));
        if(!array)return false;
        if(env->GetArrayLength(array)!=6){env->DeleteLocalRef(array);return false;}
        float v[6];env->GetFloatArrayRegion(array,0,6,v);env->DeleteLocalRef(array);out={{v[0],v[1],v[2]},{v[3],v[4],v[5]}};return sv::validViewpoint(out);
    }
    void ready(){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"onSceneReady","()V"));}
    void returnToLibrary(){env->CallVoidMethod(app->activity->clazz,env->GetMethodID(activityClass,"returnToLibraryAfterXrShutdown","()V"));}
    sv::Settings settings(){sv::Settings s;s.units=env->CallFloatMethod(app->activity->clazz,env->GetMethodID(activityClass,"getUnits","()F"));s.up=sv::UpAxis(env->CallIntMethod(app->activity->clazz,env->GetMethodID(activityClass,"getUpAxis","()I")));return s;}
    void report(const std::string&message){auto m=env->GetMethodID(activityClass,"onNativeError","(Ljava/lang/String;)V");auto text=env->NewStringUTF(message.c_str());env->CallVoidMethod(app->activity->clazz,m,text);env->DeleteLocalRef(text);}
};
sv::Vec3 vec(XrVector3f p){return {p.x,p.y,p.z};}
XrPosef identityPose(){XrPosef p{};p.orientation.w=1;return p;}
struct SortSelection {std::vector<uint32_t> indices;sv::Vec3 center{};int radius=0;double milliseconds=0;};
SortSelection prepareSelection(const sv::Scene&s,sv::Vec3 center,sv::Vec3 forward,int radius){
    auto start=std::chrono::steady_clock::now();auto indices=sv::sortBackToFront(s,center,forward,float(radius));
    return {std::move(indices),center,radius,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()};
}
int validRadius(int value){for(int r:{0,30,60,120,180,240,300,360})if(value==r)return r;return 60;}
struct Application {
    android_app*android;AndroidState&state;Jni&jni;
    XrInstance instance=XR_NULL_HANDLE;XrSystemId system=XR_NULL_SYSTEM_ID;XrSession session=XR_NULL_HANDLE;
    XrSpace room=XR_NULL_HANDLE,head=XR_NULL_HANDLE;XrActionSet actions=XR_NULL_HANDLE;
    XrSpace menuAimSpace=XR_NULL_HANDLE;XrAction menuAim=XR_NULL_HANDLE,menuSelect=XR_NULL_HANDLE,menuRestore=XR_NULL_HANDLE;
    sv::MenuControls menu;sv::Vec3 menuRayOrigin{};bool viewsOpen=false;int atlasRevision=0,viewListState=0;
    XrAction move=XR_NULL_HANDLE,turn=XR_NULL_HANDLE,reset=XR_NULL_HANDLE,exit=XR_NULL_HANDLE,detail=XR_NULL_HANDLE,filter=XR_NULL_HANDLE,capture=XR_NULL_HANDLE,radiusAction=XR_NULL_HANDLE;
    XrSessionState sessionState=XR_SESSION_STATE_UNKNOWN;bool running=false,quit=false,exitRequested=false,recenter=true;
    bool reduced=false,pixelFilter=false,captureRequested=false;int captureStep=0;
    CameraUniform captureCamera{};std::string captureDirectory;int lastCaptureRequest=0,lastSaveRequest=0,lastHudStatus=1,lastStoreCapture=0;
    sv::ViewpointPose entryPose{},defaultPose{};bool hasEntry=false,hasDefault=false,firstPlacement=true;
    Gpu gpu;sv::Navigation navigation;std::shared_ptr<sv::Scene>scene;
    std::future<sv::Scene>load;std::future<SortSelection>sort;
    sv::Vec3 activeCenter{};int requestedRadius=60,activeRadius=60;double lastSortMs=0;
    sv::Vec3 lastSortEye{},lastSortForward{0,0,-1};XrTime previousTime=0;
    std::chrono::steady_clock::time_point logStart=std::chrono::steady_clock::now();uint32_t frames=0;double workMs=0;
    Application(android_app*a,AndroidState&s,Jni&j):android(a),state(s),jni(j){}
    ~Application(){
        __android_log_print(ANDROID_LOG_INFO,"Splativerse","XR shutdown: begin");
        if(sort.valid())sort.wait();if(load.valid())load.wait();
        // Swapchains must be released before destroying the XR session and Vulkan device.
        gpu.releaseSwapchains();if(menuAimSpace)xrDestroySpace(menuAimSpace);if(head)xrDestroySpace(head);if(room)xrDestroySpace(room);if(session)xrDestroySession(session);gpu.cleanup();if(actions)xrDestroyActionSet(actions);if(instance)xrDestroyInstance(instance);
        __android_log_print(ANDROID_LOG_INFO,"Splativerse","XR shutdown: resources released");
    }
    XrPath path(const char*p){XrPath out;XR(xrStringToPath(instance,p,&out));return out;}
    XrAction action(const char*name,const char*label,XrActionType type){XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};std::strncpy(info.actionName,name,sizeof(info.actionName)-1);std::strncpy(info.localizedActionName,label,sizeof(info.localizedActionName)-1);info.actionType=type;XrAction result;XR(xrCreateAction(actions,&info,&result));return result;}
    void initialize(){
        requestedRadius=activeRadius=validRadius(jni.radius());pixelFilter=jni.pixelFilter();
        menu.step=std::clamp(jni.menuStep(),SV_MENU_MIN_STEP,SV_MENU_MAX_STEP);
        hasEntry=jni.pose("getEntryPose",entryPose);hasDefault=jni.pose("getDefaultPose",defaultPose);
        auto file=jni.string("getScanPath"),preview=jni.string("getComparePath");auto settings=jni.settings();
        load=std::async(std::launch::async,[file,preview,settings]{return preview.empty()?sv::loadScene(file,settings):sv::loadComparison(file,preview,settings);});
        PFN_xrInitializeLoaderKHR initializeLoader=nullptr;
        XR(xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)));
        XrLoaderInitInfoAndroidKHR loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};loader.applicationVM=android->activity->vm;loader.applicationContext=android->activity->clazz;XR(initializeLoader(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&loader)));
        uint32_t n;XR(xrEnumerateInstanceExtensionProperties(nullptr,0,&n,nullptr));std::vector<XrExtensionProperties>extensions(n,{XR_TYPE_EXTENSION_PROPERTIES});XR(xrEnumerateInstanceExtensionProperties(nullptr,n,&n,extensions.data()));
        std::vector<const char*>enabled={XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
        for(auto name:enabled){bool found=false;for(auto&e:extensions)if(std::strcmp(name,e.extensionName)==0)found=true;if(!found)throw std::runtime_error(std::string("Missing OpenXR extension: ")+name);}
        XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};androidInfo.applicationVM=android->activity->vm;androidInfo.applicationActivity=android->activity->clazz;
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};create.next=&androidInfo;std::strcpy(create.applicationInfo.applicationName,"Splativerse");create.applicationInfo.applicationVersion=1;std::strcpy(create.applicationInfo.engineName,"Splativerse Native");create.applicationInfo.engineVersion=1;create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,34);create.enabledExtensionCount=enabled.size();create.enabledExtensionNames=enabled.data();XR(xrCreateInstance(&create,&instance));
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};XR(xrGetInstanceProperties(instance,&props));__android_log_print(ANDROID_LOG_INFO,"Splativerse","OpenXR runtime: %s",props.runtimeName);
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;XR(xrGetSystem(instance,&si,&system));
        gpu.initialize(instance,system);auto binding=gpu.binding();XrSessionCreateInfo sc{XR_TYPE_SESSION_CREATE_INFO};sc.next=&binding;sc.systemId=system;XR(xrCreateSession(instance,&sc,&session));
        XR(xrEnumerateReferenceSpaces(session,0,&n,nullptr));std::vector<XrReferenceSpaceType>spaces(n);XR(xrEnumerateReferenceSpaces(session,n,&n,spaces.data()));
        XrReferenceSpaceCreateInfo rc{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};rc.poseInReferenceSpace=identityPose();rc.referenceSpaceType=std::find(spaces.begin(),spaces.end(),XR_REFERENCE_SPACE_TYPE_STAGE)!=spaces.end()?XR_REFERENCE_SPACE_TYPE_STAGE:XR_REFERENCE_SPACE_TYPE_LOCAL;XR(xrCreateReferenceSpace(session,&rc,&room));rc.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;XR(xrCreateReferenceSpace(session,&rc,&head));
        XrActionSetCreateInfo ac{XR_TYPE_ACTION_SET_CREATE_INFO};std::strcpy(ac.actionSetName,"navigation");std::strcpy(ac.localizedActionSetName,"Navigation");XR(xrCreateActionSet(instance,&ac,&actions));
        move=action("move","Fly and strafe",XR_ACTION_TYPE_VECTOR2F_INPUT);turn=action("turn","Turn and height",XR_ACTION_TYPE_VECTOR2F_INPUT);reset=action("reset","Return to start",XR_ACTION_TYPE_BOOLEAN_INPUT);exit=action("exit","Return to library",XR_ACTION_TYPE_BOOLEAN_INPUT);
        detail=action("detail","Show saved Views or compare detail",XR_ACTION_TYPE_BOOLEAN_INPUT);filter=action("filter","Compare original and pixel filter",XR_ACTION_TYPE_BOOLEAN_INPUT);capture=action("capture","Save View or comparison images",XR_ACTION_TYPE_BOOLEAN_INPUT);
        radiusAction=action("radius","Cycle live radius",XR_ACTION_TYPE_BOOLEAN_INPUT);
        menuAim=action("menu_aim","Point at menu",XR_ACTION_TYPE_POSE_INPUT);
        menuSelect=action("menu_select","Select menu control",XR_ACTION_TYPE_BOOLEAN_INPUT);
        menuRestore=action("menu_restore","Show controls menu",XR_ACTION_TYPE_BOOLEAN_INPUT);
        XrActionSuggestedBinding bindings[]={{move,path("/user/hand/left/input/thumbstick")},{turn,path("/user/hand/right/input/thumbstick")},{reset,path("/user/hand/right/input/a/click")},{exit,path("/user/hand/right/input/b/click")},{detail,path("/user/hand/left/input/x/click")},{filter,path("/user/hand/left/input/y/click")},{capture,path("/user/hand/left/input/thumbstick/click")},{radiusAction,path("/user/hand/right/input/thumbstick/click")}};
        std::vector<XrActionSuggestedBinding> allBindings(std::begin(bindings),std::end(bindings));
        allBindings.push_back({menuAim,path("/user/hand/right/input/aim/pose")});
        allBindings.push_back({menuSelect,path("/user/hand/right/input/trigger/value")});
        allBindings.push_back({menuRestore,path("/user/hand/left/input/trigger/value")});
        XrInteractionProfileSuggestedBinding suggestion{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};suggestion.interactionProfile=path("/interaction_profiles/oculus/touch_controller");suggestion.countSuggestedBindings=allBindings.size();suggestion.suggestedBindings=allBindings.data();XR(xrSuggestInteractionProfileBindings(instance,&suggestion));
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actions;XR(xrAttachSessionActionSets(session,&attach));
        XrActionSpaceCreateInfo aimSpace{XR_TYPE_ACTION_SPACE_CREATE_INFO};aimSpace.action=menuAim;aimSpace.poseInActionSpace=identityPose();XR(xrCreateActionSpace(session,&aimSpace,&menuAimSpace));
        XR(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr));if(n!=2)throw std::runtime_error("Stereo view configuration required.");std::vector<XrViewConfigurationView>views(n,{XR_TYPE_VIEW_CONFIGURATION_VIEW});XR(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,n,&n,views.data()));
        gpu.uploadViewText(jni.viewTextAtlas());
        gpu.createTargets(session,views,android->activity->assetManager);
    }
    XrVector2f stick(XrAction a){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;XrActionStateVector2f out{XR_TYPE_ACTION_STATE_VECTOR2F};XR(xrGetActionStateVector2f(session,&get,&out));return out.isActive?out.currentState:XrVector2f{};}
    bool pressed(XrAction a){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;XrActionStateBoolean out{XR_TYPE_ACTION_STATE_BOOLEAN};XR(xrGetActionStateBoolean(session,&get,&out));return out.isActive&&out.changedSinceLastSync&&out.currentState;}
    bool held(XrAction a){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=a;XrActionStateBoolean out{XR_TYPE_ACTION_STATE_BOOLEAN};XR(xrGetActionStateBoolean(session,&get,&out));return out.isActive&&out.currentState;}
    sv::MenuHit pointAtMenu(XrTime time){
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=menuAim;XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};XR(xrGetActionStatePose(session,&get,&pose));if(!pose.isActive)return {};
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};XR(xrLocateSpace(menuAimSpace,head,time,&location));
        auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;if((location.locationFlags&valid)!=valid)return {};
        XrMatrix4x4f matrix;XrMatrix4x4f_CreateFromRigidTransform(&matrix,&location.pose);
        menuRayOrigin=vec(location.pose.position);
        return sv::menuHit(menuRayOrigin,{-matrix.m[8],-matrix.m[9],-matrix.m[10]},menu.step,viewsOpen);
    }
    void events(){
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(instance,&event)==XR_SUCCESS){
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){auto&s=*reinterpret_cast<XrEventDataSessionStateChanged*>(&event);sessionState=s.state;__android_log_print(ANDROID_LOG_INFO,"Splativerse","Session state %d",int(s.state));
                if(s.state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;XR(xrBeginSession(session,&begin));running=true;previousTime=0;}
                if(s.state==XR_SESSION_STATE_STOPPING){running=false;XR(xrEndSession(session));if(exitRequested)quit=true;}
                if(s.state==XR_SESSION_STATE_EXITING||s.state==XR_SESSION_STATE_LOSS_PENDING){running=false;quit=true;}
            }
            if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)quit=true;
            if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)recenter=true;
            event={XR_TYPE_EVENT_DATA_BUFFER};
        }
    }
    void draw(){
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};XR(xrWaitFrame(session,&wait,&frame));XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};XR(xrBeginFrame(session,&begin));
        auto start=std::chrono::steady_clock::now();
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=room;std::array<XrCompositionLayerProjectionView,2>projection{};bool rendered=false;
        if(frame.shouldRender){
            uint32_t n;XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=frame.predictedDisplayTime;locate.space=room;XrViewState vs{XR_TYPE_VIEW_STATE};std::array<XrView,2>views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};XR(xrLocateViews(session,&locate,&vs,2,&n,views.data()));
            XrSpaceLocation headLocation{XR_TYPE_SPACE_LOCATION};XR(xrLocateSpace(head,room,frame.predictedDisplayTime,&headLocation));
            auto needed=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if(n==2&&(headLocation.locationFlags&needed)==needed&&(vs.viewStateFlags&(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT))==(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT)){
                if(!scene&&load.valid()&&load.wait_for(std::chrono::seconds(0))==std::future_status::ready){scene=std::make_shared<sv::Scene>(load.get());auto initialEye=hasEntry?sv::viewpointPosition(entryPose,scene->settings):scene->spawn;auto initialYaw=hasEntry?sv::viewpointYaw(entryPose,scene->settings):scene->spawnYaw;auto initial=prepareSelection(*scene,initialEye,sv::yawRotate({0,0,-1},initialYaw),requestedRadius);gpu.upload(*scene,initial.indices);activeCenter=initial.center;activeRadius=initial.radius;lastSortMs=initial.milliseconds;recenter=true;__android_log_print(ANDROID_LOG_INFO,"Splativerse","Loaded %zu splats; antialiased %d; comparison subset %zu; GPU scene %.1f MiB",scene->splats.size(),scene->antialiased,scene->previewIndices.size(),scene->splats.size()*244/1048576.0);}
                XrMatrix4x4f headMatrix;XrMatrix4x4f_CreateFromRigidTransform(&headMatrix,&headLocation.pose);sv::Vec3 physical=vec(headLocation.pose.position),forward{-headMatrix.m[8],-headMatrix.m[9],-headMatrix.m[10]};
                float headYaw=sv::headingYaw(forward,{headMatrix.m[0],headMatrix.m[1],headMatrix.m[2]});
                bool focused=sessionState==XR_SESSION_STATE_FOCUSED&&state.resumed;
                XrVector2f left{},right{};bool resetNow=false,saveViewNow=false;
                if(focused){XrActiveActionSet active{actions,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;XR(xrSyncActions(session,&sync));left=stick(move);right=stick(turn);resetNow=pressed(reset);if(pressed(exit)&&!exitRequested){XR(xrRequestExitSession(session));exitRequested=true;}}
                const auto viewHudState=jni.viewHudState();
                if(focused&&scene&&pressed(detail)){
                    if(scene->previewIndices.empty()){
                        viewsOpen=!viewsOpen;menu.hidden=false;jni.listOpen(viewsOpen);
                    }else{reduced=!reduced;__android_log_print(ANDROID_LOG_INFO,"Splativerse","Detail: %s",reduced?"reduced copy":"full detail");}
                }
                int revision=jni.listRevision();if(revision!=atlasRevision){gpu.uploadViewText(jni.viewTextAtlas());atlasRevision=revision;}
                viewListState=jni.listState();
                bool menuSelectable=scene&&(viewsOpen||int(viewHudState[0])<2)&&!exitRequested;
                sv::MenuHit menuHit=focused&&menuSelectable&&!menu.hidden?pointAtMenu(frame.predictedDisplayTime):sv::MenuHit{};
                int oldStep=menu.step;bool wasHidden=menu.hidden;
                bool selectDown=focused&&held(menuSelect),restoreDown=focused&&held(menuRestore);
                bool listClick=viewsOpen&&focused&&!exitRequested&&menu.selectReady&&selectDown;
                bool listRestore=viewsOpen&&focused&&menu.restoreReady&&restoreDown;
                sv::ViewpointPose chosenPose{};bool choseView=false;
                menu.update(menuHit,focused&&!exitRequested,menuSelectable&&!viewsOpen,selectDown,restoreDown);
                if(listRestore||(listClick&&menuHit.button==2)){viewsOpen=false;jni.listOpen(false);}
                else if(listClick){choseView=jni.listAction(menuHit.button,chosenPose);if(choseView)viewsOpen=false;}
                if(menu.step!=oldStep){jni.saveMenuStep(menu.step);menuHit=pointAtMenu(frame.predictedDisplayTime);}
                if(menu.step!=oldStep||menu.hidden!=wasHidden)__android_log_print(ANDROID_LOG_INFO,"Splativerse","Controls menu: step %d; hidden %d",menu.step,menu.hidden);
                if(focused&&scene){
                    if(pressed(radiusAction)){const int radii[]={30,60,120,180,240,300,360,0};for(int i=0;i<8;i++)if(requestedRadius==radii[i]){requestedRadius=radii[(i+1)%8];break;}jni.saveRadius(requestedRadius);}
                    if(pressed(filter)){pixelFilter=!pixelFilter;jni.savePixelFilter(pixelFilter);__android_log_print(ANDROID_LOG_INFO,"Splativerse","Filter: %s",pixelFilter?"pixel compensated":(scene->antialiased?"SPZ AA":"original"));}
                    if(pressed(capture)&&!captureStep&&!viewsOpen){if(scene->previewIndices.empty())saveViewNow=true;else captureRequested=true;}
                }
                // Optional shell overrides are read once per frame so both eyes agree.
                auto property=[](const char*key,int fallback){char value[PROP_VALUE_MAX]{};return __system_property_get(key,value)>0?std::atoi(value):fallback;};
                if(focused&&!exitRequested&&property("debug.splativerse.exit",0)>0){XR(xrRequestExitSession(session));exitRequested=true;}
                int filterOverride=property("debug.splativerse.filter",-1),detailOverride=property("debug.splativerse.detail",-1);
                if(filterOverride>=0)pixelFilter=filterOverride!=0;
                if(detailOverride>=0&&scene&&!scene->previewIndices.empty())reduced=detailOverride!=0;
                int captureRequest=property("debug.splativerse.capture",0);
                if(captureRequest!=lastCaptureRequest){lastCaptureRequest=captureRequest;if(captureRequest>0&&scene&&!scene->previewIndices.empty()&&!captureStep)captureRequested=true;}
                int saveRequest=property("debug.splativerse.save_view",0);
                if(saveRequest!=lastSaveRequest){lastSaveRequest=saveRequest;if(saveRequest>0&&focused&&scene&&scene->previewIndices.empty())saveViewNow=true;}
                int radiusOverride=property("debug.splativerse.radius",-1);if(radiusOverride>=0)requestedRadius=validRadius(radiusOverride);
                int debugMode=property("debug.splativerse.render",0);
                if(scene&&(recenter||resetNow)){
                    bool useEntry=firstPlacement&&!resetNow;bool hasPose=useEntry?hasEntry:hasDefault;const auto&p=useEntry?entryPose:defaultPose;
                    navigation.reset(physical,headYaw,hasPose?sv::viewpointPosition(p,scene->settings):scene->spawn,hasPose?sv::viewpointYaw(p,scene->settings):scene->spawnYaw);
                    if(firstPlacement){jni.ready();__android_log_print(ANDROID_LOG_INFO,"Splativerse","View placement: saved %d; world %.6f %.6f %.6f; yaw %.6f",hasPose,navigation.point(physical).x,navigation.point(physical).y,navigation.point(physical).z,headYaw+navigation.yaw);}
                    firstPlacement=false;recenter=false;
                }
                if(choseView){
                    navigation.reset(physical,headYaw,sv::viewpointPosition(chosenPose,scene->settings),sv::viewpointYaw(chosenPose,scene->settings));
                    __android_log_print(ANDROID_LOG_INFO,"Splativerse","Immersive View placement: world %.6f %.6f %.6f; yaw %.6f",navigation.point(physical).x,navigation.point(physical).y,navigation.point(physical).z,headYaw+navigation.yaw);
                }
                float dt=previousTime?float(frame.predictedDisplayTime-previousTime)*1e-9f:0;previousTime=frame.predictedDisplayTime;
                navigation.update(dt,physical,forward,left.x,left.y,right.x,right.y,focused&&bool(scene)&&!viewsOpen&&!choseView);
                sv::Vec3 virtualHead=navigation.point(physical),virtualForward=sv::yawRotate(forward,navigation.yaw);
                if(saveViewNow&&scene)jni.saveView(sv::captureViewpoint(virtualHead,headYaw+navigation.yaw,scene->settings));
                if(scene&&!captureStep&&!captureRequested){
                    if(sort.valid()&&sort.wait_for(std::chrono::seconds(0))==std::future_status::ready){
                        auto prepared=sort.get();
                        if(prepared.radius==requestedRadius){gpu.updateOrder(prepared.indices);activeCenter=prepared.center;activeRadius=prepared.radius;lastSortMs=prepared.milliseconds;}
                    }
                    auto diff=virtualHead-lastSortEye;
                    if(!sort.valid()&&(activeRadius!=requestedRadius||sv::dot(diff,diff)>0.0025f||sv::dot(virtualForward,lastSortForward)<0.999f)){
                        lastSortEye=virtualHead;lastSortForward=virtualForward;auto immutable=scene;int radius=requestedRadius;
                        sort=std::async(std::launch::async,[immutable,virtualHead,virtualForward,radius]{return prepareSelection(*immutable,virtualHead,virtualForward,radius);});
                    }
                }
                XrQuaternionf yaw{0,std::sin(navigation.yaw/2),0,std::cos(navigation.yaw/2)};XrVector3f offset{navigation.offset.x,navigation.offset.y,navigation.offset.z},scale{1,1,1};XrMatrix4x4f worldFromTracking;XrMatrix4x4f_CreateTranslationRotationScale(&worldFromTracking,&offset,&yaw,&scale);
                const int viewHudStatus=viewsOpen?8:(menu.hidden&&int(viewHudState[0])<2?0:int(viewHudState[0]));
                if(viewHudStatus!=lastHudStatus){lastHudStatus=viewHudStatus;__android_log_print(ANDROID_LOG_INFO,"Splativerse","View HUD status: %d",viewHudStatus);}
                for(int e=0;e<2;e++){
                    XrMatrix4x4f trackedEye,worldEye;XrMatrix4x4f_CreateFromRigidTransform(&trackedEye,&views[e].pose);XrMatrix4x4f_Multiply(&worldEye,&worldFromTracking,&trackedEye);
                    CameraUniform camera{};XrMatrix4x4f_InvertRigidBody(&camera.view,&worldEye);XrMatrix4x4f_CreateProjectionFov(&camera.projection,GRAPHICS_VULKAN,views[e].fov,0.03f,10000.f);
                    for(int i=0;i<3;i++)camera.eye[i]=worldEye.m[12+i];auto&t=gpu.eyes[e];camera.viewport[0]=t.width;camera.viewport[1]=t.height;camera.viewport[2]=scene?int(scene->settings.up):0;camera.viewport[3]=gpu.srgb?1:0;
                    camera.eye[3]=debugMode;camera.options[0]=pixelFilter?1:(scene&&scene->antialiased?2:0);camera.options[1]=reduced?1:0;camera.options[2]=scene&&!scene->previewIndices.empty()?1:0;camera.options[3]=float(viewHudStatus);camera.feedback[0]=viewHudState[1];camera.feedback[1]=viewHudState[2];if(viewsOpen){camera.feedback[0]=viewListState&7;camera.feedback[1]=(viewListState&8)?1:0;camera.feedback[2]=(viewListState&16)?1:0;}
                    camera.menu[0]=menu.step*SV_MENU_STEP;camera.menu[1]=menuHit.button;camera.menu[2]=menuHit.u;camera.menu[3]=menuHit.v;
                    camera.beamOrigin[0]=menuRayOrigin.x;camera.beamOrigin[1]=menuRayOrigin.y;camera.beamOrigin[2]=menuRayOrigin.z;camera.beamOrigin[3]=(viewHudStatus==1||viewHudStatus==8)&&menuHit.u>=0?1:0;
                    camera.bubble[0]=activeCenter.x;camera.bubble[1]=activeCenter.y;camera.bubble[2]=activeCenter.z;camera.bubble[3]=float(activeRadius);
                    camera.renderInfo[0]=float(std::max(0,activeRadius-5));camera.renderInfo[1]=float(gpu.liveCount());camera.renderInfo[2]=float(requestedRadius);camera.renderInfo[3]=scene?float(scene->splats.size()):0;
                    XrMatrix4x4f eyeFromTracking;XrMatrix4x4f_InvertRigidBody(&eyeFromTracking,&trackedEye);XrMatrix4x4f_Multiply(&camera.headToEye,&eyeFromTracking,&headMatrix);
                    int storeRequest=property("debug.splativerse.store_capture",0);
                    if(e==0&&focused&&scene&&storeRequest>0&&storeRequest!=lastStoreCapture){
                        lastStoreCapture=storeRequest;
                        try{
                            CameraUniform still=camera;still.options[3]=0;still.eye[3]=0;
                            // A clean monoscopic in-app capture from the current eye pose.
                            XrFovf photoFov{-sv::pi/4,sv::pi/4,std::atan(9.f/16),-std::atan(9.f/16)};
                            XrMatrix4x4f_CreateProjectionFov(&still.projection,GRAPHICS_VULKAN,photoFov,0.03f,10000.f);
                            auto folder=jni.string("getCaptureDirectory")+"/Store";std::filesystem::create_directories(folder);
                            auto base=folder+"/capture-"+std::to_string(storeRequest);
                            gpu.capture(still,base+".ppm",2560,1440);
                            std::ofstream metadata(base+".txt");metadata<<"Source: "<<jni.string("getScanPath")<<"\nDimensions: 2560 x 1440\nEye: "<<still.eye[0]<<' '<<still.eye[1]<<' '<<still.eye[2]<<"\nRadius: "<<activeRadius<<"\nFilter: "<<still.options[0]<<"\nView matrix:";for(float value:still.view.m)metadata<<' '<<value;metadata<<"\n";
                            __android_log_print(ANDROID_LOG_INFO,"Splativerse","Store capture saved: %s",base.c_str());
                        }catch(const std::exception&ex){__android_log_print(ANDROID_LOG_ERROR,"Splativerse","Store capture failed: %s",ex.what());}
                    }
                    if(e==0&&captureRequested&&scene){
                        captureRequested=false;
                        try{
                            // Fix one freshly computed order for the entire comparison.
                            if(sort.valid()){sort.wait();sort.get();}
                            auto prepared=prepareSelection(*scene,virtualHead,virtualForward,activeRadius);gpu.updateOrder(prepared.indices);activeCenter=prepared.center;lastSortMs=prepared.milliseconds;lastSortEye=virtualHead;lastSortForward=virtualForward;
                            camera.bubble[0]=activeCenter.x;camera.bubble[1]=activeCenter.y;camera.bubble[2]=activeCenter.z;camera.renderInfo[1]=float(gpu.liveCount());
                            captureCamera=camera;captureCamera.eye[3]=0;captureCamera.options[3]=0;
                            auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                            captureDirectory=jni.string("getCaptureDirectory")+"/"+std::to_string(stamp);std::filesystem::create_directories(captureDirectory);
                            std::ofstream metadata(captureDirectory+"/view.txt");metadata.precision(9);
                            metadata<<"Live radius "<<activeRadius<<"; center "<<activeCenter.x<<' '<<activeCenter.y<<' '<<activeCenter.z<<"; selected "<<gpu.liveCount()<<"; outer fade 5 metres.\n";
                            metadata<<"Source antialiased "<<scene->antialiased<<"; original captures use "<<(scene->antialiased?"SPZ AA (0.3, compensated)":"classic (0.3, uncompensated)")<<"; pixel captures use 1/12, compensated.\n";
                            metadata<<"Full "<<scene->splats.size()<<"; reduced "<<scene->previewIndices.size()<<"; same eye pose, projection, resolution, and sort order in all four images.\nView:";for(float f:camera.view.m)metadata<<' '<<f;metadata<<"\nProjection:";for(float f:camera.projection.m)metadata<<' '<<f;metadata<<"\nResolution "<<t.width<<' '<<t.height<<"\n";
                            if(!metadata)throw std::runtime_error("Could not write capture metadata.");captureStep=1;jni.message("Saving four comparisons. Rendering may pause briefly.");
                        }catch(const std::exception&ex){__android_log_print(ANDROID_LOG_ERROR,"Splativerse","Capture: %s",ex.what());jni.message("Could not create comparison folder.");}
                    }
                    if(e==0&&scene&&frames==0&&debugMode!=0){
                        size_t visible=0;float minZ=1e10f,maxZ=-1e10f;
                        for(auto&s:scene->splats){XrVector3f p{s.position[0],s.position[1],s.position[2]},v{},c{};XrMatrix4x4f_TransformVector3f(&v,&camera.view,&p);XrMatrix4x4f_TransformVector3f(&c,&camera.projection,&v);minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);if(v.z<-.03f&&std::abs(c.x)<=1&&std::abs(c.y)<=1&&c.z>=0&&c.z<=1)visible++;}
                        __android_log_print(ANDROID_LOG_INFO,"Splativerse","Camera %.3f %.3f %.3f; source spawn %.3f %.3f %.3f; visible centers %zu; view Z %.2f..%.2f; mode %.0f",camera.eye[0],camera.eye[1],camera.eye[2],scene->spawn.x,scene->spawn.y,scene->spawn.z,visible,minZ,maxZ,camera.eye[3]);
                    }
                    uint32_t image;XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(t.swapchain,&acquire,&image));XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(t.swapchain,&wi));gpu.render(e,image,camera);XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(t.swapchain,&release));
                    projection[e]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};projection[e].pose=views[e].pose;projection[e].fov=views[e].fov;projection[e].subImage.swapchain=t.swapchain;projection[e].subImage.imageRect={{0,0},{int32_t(t.width),int32_t(t.height)}};
                }
                layer.viewCount=2;layer.views=projection.data();rendered=true;
                if(captureStep){
                    const char*names[]={"full-original.ppm","reduced-original.ppm","full-pixel.ppm","reduced-pixel.ppm"};
                    captureCamera.options[0]=captureStep>=3?1:(scene&&scene->antialiased?2:0);captureCamera.options[1]=(captureStep==2||captureStep==4)?1:0;
                    try{gpu.capture(captureCamera,captureDirectory+"/"+names[captureStep-1]);if(++captureStep>4){captureStep=0;jni.message("Four comparison images saved.");__android_log_print(ANDROID_LOG_INFO,"Splativerse","Comparison saved: %s",captureDirectory.c_str());}}
                    catch(const std::exception&ex){captureStep=0;jni.message("Comparison capture failed. See logs.");__android_log_print(ANDROID_LOG_ERROR,"Splativerse","Capture: %s",ex.what());}
                }
            }
        }else{previousTime=0;navigation.turnLatched=false;menu.update({},false,false,false,false);}
        const XrCompositionLayerBaseHeader*layers[]={reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=rendered?1:0;end.layers=rendered?layers:nullptr;XR(xrEndFrame(session,&end));
        workMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();frames++;
        double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-logStart).count();if(seconds>=5){__android_log_print(ANDROID_LOG_INFO,"Splativerse","Frames %.1f/s; mean CPU + GPU-wait work %.2f ms; splats %zu; live %u; radius %d; sort %.2f ms; detail %s; filter %s",frames/seconds,workMs/frames,scene?scene->splats.size():0,gpu.liveCount(),activeRadius,lastSortMs,reduced?"reduced":"full",pixelFilter?"pixel":(scene&&scene->antialiased?"SPZ AA":"original"));frames=0;workMs=0;logStart=std::chrono::steady_clock::now();}
    }
    void run(){
        initialize();
        while(!android->destroyRequested&&!quit){int events;android_poll_source*source;
            int timeout=running?0:50;
            while(ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source))>=0){if(source)source->process(android,source);if(android->destroyRequested)break;timeout=0;}
            if(android->destroyRequested)break;eventsPoll();if(running&&!quit)draw();
        }
    }
    void eventsPoll(){events();}
};
}
void android_main(android_app*app){
    AndroidState state;app->userData=&state;app->onAppCmd=appCommand;Jni jni(app);
    bool returnToLibrary=false;
    try{Application application(app,state,jni);application.run();returnToLibrary=application.exitRequested;}
    catch(const std::exception&e){__android_log_print(ANDROID_LOG_ERROR,"Splativerse","%s",e.what());jni.report(e.what());returnToLibrary=true;}
    // Application's destructor has completed before Home is asked to restore
    // panel interaction. A system-driven exit must not relaunch our library.
    if(returnToLibrary&&!app->destroyRequested)jni.returnToLibrary();
    else ANativeActivity_finish(app->activity);
    while(!app->destroyRequested){int events;android_poll_source*source;if(ALooper_pollOnce(50,nullptr,&events,reinterpret_cast<void**>(&source))>=0&&source)source->process(app,source);}
}
