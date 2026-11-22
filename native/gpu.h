#pragma once
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <jni.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <android/asset_manager.h>
#include "xr_linear.h"
#include "core.h"
#include <stdexcept>

inline void xrCheck(XrResult r,const char*what){if(XR_FAILED(r))throw std::runtime_error(std::string(what)+" (OpenXR "+std::to_string(r)+")");}
inline void vkCheck(VkResult r,const char*what){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(what)+" (Vulkan "+std::to_string(r)+")");}
#define XR(call) xrCheck((call),#call)
#define VK(call) vkCheck((call),#call)
template<class T>T xrFunction(XrInstance instance,const char*name){PFN_xrVoidFunction p=nullptr;XR(xrGetInstanceProcAddr(instance,name,&p));return reinterpret_cast<T>(p);}
struct CameraUniform {XrMatrix4x4f view,projection;float eye[4],viewport[4],options[4];XrMatrix4x4f headToEye;float bubble[4],renderInfo[4],feedback[4],menu[4],beamOrigin[4];};
static_assert(sizeof(CameraUniform)==320);
struct Buffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void*mapped=nullptr;VkDeviceSize size=0;};
struct EyeTarget {
    XrSwapchain swapchain=XR_NULL_HANDLE;uint32_t width=0,height=0;
    std::vector<XrSwapchainImageVulkanKHR> images;
    std::vector<VkImageView> views;std::vector<VkFramebuffer> framebuffers;
    Buffer camera;VkDescriptorSet descriptor=VK_NULL_HANDLE;
};
class Gpu {
public:
    VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
    uint32_t queueFamily=0;VkQueue queue=VK_NULL_HANDLE;VkFormat format=VK_FORMAT_UNDEFINED;
    std::array<EyeTarget,2> eyes{};bool srgb=true;
    void initialize(XrInstance xr,XrSystemId system);
    XrGraphicsBindingVulkan2KHR binding()const;
    void createTargets(XrSession session,const std::vector<XrViewConfigurationView>&config,AAssetManager*assets);
    void uploadViewText(const std::vector<uint32_t>&alpha);
    void upload(const sv::Scene&scene,const std::vector<uint32_t>&indices);
    void updateOrder(const std::vector<uint32_t>&indices);
    uint32_t liveCount()const{return drawCount;}
    void render(int eye,uint32_t image,const CameraUniform&camera);
    void capture(const CameraUniform&camera,const std::string&path,uint32_t width=0,uint32_t height=0);
    void releaseSwapchains();
    void cleanup();
    ~Gpu(){cleanup();}
private:
    VkCommandPool commandPool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
    VkRenderPass renderPass=VK_NULL_HANDLE;VkDescriptorSetLayout descriptorLayout=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool=VK_NULL_HANDLE;VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE,hudPipeline=VK_NULL_HANDLE;
    Buffer splats,harmonicsLow,harmonicsHigh,order,quadIndices,viewText;uint32_t count=0,drawCount=0;
    void recordRender(EyeTarget&e,VkFramebuffer framebuffer,const CameraUniform&camera);
    void submitAndWait();
    Buffer makeBuffer(VkDeviceSize bytes,VkBufferUsageFlags usage);
    void destroyBuffer(Buffer&b);
    void makePipeline(AAssetManager*assets);
};
