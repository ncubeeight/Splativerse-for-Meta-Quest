#include "gpu.h"
#include <cstring>
#include <fstream>
#include <android/log.h>

void Gpu::initialize(XrInstance xr,XrSystemId system){
    auto requirements=xrFunction<PFN_xrGetVulkanGraphicsRequirements2KHR>(xr,"xrGetVulkanGraphicsRequirements2KHR");
    XrGraphicsRequirementsVulkan2KHR limits{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};XR(requirements(xr,system,&limits));
    const XrVersion api=XR_MAKE_VERSION(1,1,0);
    if(api<limits.minApiVersionSupported||api>limits.maxApiVersionSupported)throw std::runtime_error("Vulkan 1.1 is unavailable on this runtime.");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Splativerse";app.applicationVersion=1;app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
    XrVulkanInstanceCreateInfoKHR xci{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};xci.systemId=system;xci.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;xci.vulkanCreateInfo=&ci;
    VkResult result;XR(xrFunction<PFN_xrCreateVulkanInstanceKHR>(xr,"xrCreateVulkanInstanceKHR")(xr,&xci,&instance,&result));VK(result);
    XrVulkanGraphicsDeviceGetInfoKHR gi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};gi.systemId=system;gi.vulkanInstance=instance;
    XR(xrFunction<PFN_xrGetVulkanGraphicsDevice2KHR>(xr,"xrGetVulkanGraphicsDevice2KHR")(xr,&gi,&physical));
    uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties>families(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families.data());
    bool found=false;for(uint32_t i=0;i<n;i++)if(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){queueFamily=i;found=true;break;}
    if(!found)throw std::runtime_error("No Vulkan graphics queue.");
    float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=queueFamily;qi.queueCount=1;qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
    XrVulkanDeviceCreateInfoKHR xd{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};xd.systemId=system;xd.pfnGetInstanceProcAddr=vkGetInstanceProcAddr;xd.vulkanPhysicalDevice=physical;xd.vulkanCreateInfo=&di;
    XR(xrFunction<PFN_xrCreateVulkanDeviceKHR>(xr,"xrCreateVulkanDeviceKHR")(xr,&xd,&device,&result));VK(result);vkGetDeviceQueue(device,queueFamily,0,&queue);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool.queueFamilyIndex=queueFamily;VK(vkCreateCommandPool(device,&pool,nullptr,&commandPool));
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocate.commandPool=commandPool;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;VK(vkAllocateCommandBuffers(device,&allocate,&command));
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK(vkCreateFence(device,&fi,nullptr,&fence));
    VkPhysicalDeviceProperties p;vkGetPhysicalDeviceProperties(physical,&p);
    __android_log_print(ANDROID_LOG_INFO,"Splativerse","GPU: %s; storage range %u",p.deviceName,p.limits.maxStorageBufferRange);
    if(sv::maxSplats*sv::shFloatsPerHalf*sizeof(float)>p.limits.maxStorageBufferRange)throw std::runtime_error("GPU storage buffer limit is too small.");
}
XrGraphicsBindingVulkan2KHR Gpu::binding()const{
    XrGraphicsBindingVulkan2KHR b{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};b.instance=instance;b.physicalDevice=physical;b.device=device;b.queueFamilyIndex=queueFamily;b.queueIndex=0;return b;
}
Buffer Gpu::makeBuffer(VkDeviceSize bytes,VkBufferUsageFlags usage){
    Buffer b;b.size=std::max(VkDeviceSize(16),bytes);
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=b.size;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;VK(vkCreateBuffer(device,&ci,nullptr,&b.buffer));
    try{
        VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,b.buffer,&mr);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(physical,&mp);
        uint32_t type=UINT32_MAX;for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){type=i;break;}
        if(type==UINT32_MAX)throw std::runtime_error("No CPU-visible coherent GPU memory.");
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=type;VK(vkAllocateMemory(device,&ai,nullptr,&b.memory));VK(vkBindBufferMemory(device,b.buffer,b.memory,0));VK(vkMapMemory(device,b.memory,0,b.size,0,&b.mapped));
    }catch(...){destroyBuffer(b);throw;}return b;
}
void Gpu::destroyBuffer(Buffer&b){if(b.mapped)vkUnmapMemory(device,b.memory);if(b.buffer)vkDestroyBuffer(device,b.buffer,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);b={};}
void Gpu::createTargets(XrSession session,const std::vector<XrViewConfigurationView>&config,AAssetManager*assets){
    uint32_t n;XR(xrEnumerateSwapchainFormats(session,0,&n,nullptr));std::vector<int64_t>formats(n);XR(xrEnumerateSwapchainFormats(session,n,&n,formats.data()));
    for(auto preferred:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM})if(std::find(formats.begin(),formats.end(),preferred)!=formats.end()){format=preferred;break;}
    if(format==VK_FORMAT_UNDEFINED)throw std::runtime_error("No compatible XR color format.");srgb=format==VK_FORMAT_R8G8B8A8_SRGB||format==VK_FORMAT_B8G8R8A8_SRGB;
    VkAttachmentDescription attachment{};attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=1;sub.pColorAttachments=&ref;
    VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=1;rp.pAttachments=&attachment;rp.subpassCount=1;rp.pSubpasses=&sub;rp.dependencyCount=1;rp.pDependencies=&dep;VK(vkCreateRenderPass(device,&rp,nullptr,&renderPass));
    for(int e=0;e<2;e++){
        auto&t=eyes[e];t.width=std::min(config[e].recommendedImageRectWidth,config[e].maxImageRectWidth);t.height=std::min(config[e].recommendedImageRectHeight,config[e].maxImageRectHeight);
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;ci.format=format;ci.sampleCount=1;ci.width=t.width;ci.height=t.height;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;XR(xrCreateSwapchain(session,&ci,&t.swapchain));
        XR(xrEnumerateSwapchainImages(t.swapchain,0,&n,nullptr));t.images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});XR(xrEnumerateSwapchainImages(t.swapchain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(t.images.data())));
        t.views.resize(n);t.framebuffers.resize(n);
        for(uint32_t i=0;i<n;i++){
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=t.images[i].image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VK(vkCreateImageView(device,&vi,nullptr,&t.views[i]));
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=renderPass;fb.attachmentCount=1;fb.pAttachments=&t.views[i];fb.width=t.width;fb.height=t.height;fb.layers=1;VK(vkCreateFramebuffer(device,&fb,nullptr,&t.framebuffers[i]));
        }
        t.camera=makeBuffer(sizeof(CameraUniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        __android_log_print(ANDROID_LOG_INFO,"Splativerse","Eye %d: %ux%u, %u swapchain images",e,t.width,t.height,n);
    }
    // The Vision Pro renderer batches 1024 indexed quads per instance.
    // Four unique vertices per splat replace the former six repeated vertices.
    quadIndices=makeBuffer(1024*6*sizeof(uint16_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const uint16_t corners[]={0,1,2,2,1,3};auto*quads=static_cast<uint16_t*>(quadIndices.mapped);
    for(uint16_t i=0;i<1024;i++)for(int j=0;j<6;j++)quads[i*6+j]=i*4+corners[j];
    makePipeline(assets);
}
void Gpu::uploadViewText(const std::vector<uint32_t>&alpha){
    if(alpha.size()!=1024*256*16/4)throw std::runtime_error("Invalid View text dimensions.");
    // Rendering waits for each submitted eye; a frame-boundary atlas update is safe.
    if(!viewText.buffer)viewText=makeBuffer(alpha.size()*sizeof(uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(viewText.mapped,alpha.data(),viewText.size);
}
void Gpu::makePipeline(AAssetManager*assets){
    VkDescriptorSetLayoutBinding bindings[6]{};for(int i=0;i<6;i++){bindings[i].binding=i;bindings[i].descriptorType=i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[i].descriptorCount=1;bindings[i].stageFlags=i==5?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT|(i==2?VK_SHADER_STAGE_FRAGMENT_BIT:0);}
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};dl.bindingCount=6;dl.pBindings=bindings;VK(vkCreateDescriptorSetLayout(device,&dl,nullptr,&descriptorLayout));
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,10},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}};
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=2;dp.poolSizeCount=2;dp.pPoolSizes=sizes;VK(vkCreateDescriptorPool(device,&dp,nullptr,&descriptorPool));
    VkDescriptorSetLayout layouts[]={descriptorLayout,descriptorLayout};VkDescriptorSet ds[2];VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptorPool;da.descriptorSetCount=2;da.pSetLayouts=layouts;VK(vkAllocateDescriptorSets(device,&da,ds));for(int i=0;i<2;i++)eyes[i].descriptor=ds[i];
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&descriptorLayout;VK(vkCreatePipelineLayout(device,&pl,nullptr,&pipelineLayout));
    auto shader=[&](const char*path){AAsset*a=AAssetManager_open(assets,path,AASSET_MODE_BUFFER);if(!a)throw std::runtime_error(std::string("Missing shader ")+path);size_t n=AAsset_getLength(a);std::vector<uint32_t>code((n+3)/4);int read=AAsset_read(a,code.data(),n);AAsset_close(a);if(n%4||read!=int(n))throw std::runtime_error("Invalid shader asset.");VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=n;ci.pCode=code.data();VkShaderModule m;VK(vkCreateShaderModule(device,&ci,nullptr,&m));return m;};
    VkShaderModule vert=shader("splat.vert.spv"),frag=VK_NULL_HANDLE;
    try{
        frag=shader("splat.frag.spv");VkPipelineShaderStageCreateInfo stages[2]{};
        for(int i=0;i<2;i++){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=i?frag:vert;stages[i].pName="main";}
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=1;vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};blend.blendEnable=VK_TRUE;blend.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;blend.colorBlendOp=VK_BLEND_OP_ADD;blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;blend.alphaBlendOp=VK_BLEND_OP_ADD;blend.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&blend;
        VkDynamicState dynamic[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dy.dynamicStateCount=2;dy.pDynamicStates=dynamic;
        VkGraphicsPipelineCreateInfo p{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};p.stageCount=2;p.pStages=stages;p.pVertexInputState=&vi;p.pInputAssemblyState=&ia;p.pViewportState=&vp;p.pRasterizationState=&rs;p.pMultisampleState=&ms;p.pColorBlendState=&cb;p.pDynamicState=&dy;p.layout=pipelineLayout;p.renderPass=renderPass;
        VK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&p,nullptr,&pipeline));
        vkDestroyShaderModule(device,frag,nullptr);frag=VK_NULL_HANDLE;vkDestroyShaderModule(device,vert,nullptr);vert=VK_NULL_HANDLE;
        vert=shader("hud.vert.spv");frag=shader("hud.frag.spv");stages[0].module=vert;stages[1].module=frag;
        VK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&p,nullptr,&hudPipeline));
    }catch(...){if(frag)vkDestroyShaderModule(device,frag,nullptr);vkDestroyShaderModule(device,vert,nullptr);throw;}
    vkDestroyShaderModule(device,frag,nullptr);vkDestroyShaderModule(device,vert,nullptr);
}
void Gpu::upload(const sv::Scene&s,const std::vector<uint32_t>&indices){
    count=s.splats.size();
    // Tight float32 RGB halves: each is <=96,000,000 bytes at 1M splats.
    struct Geometry {float position[4],covA[4],covB[4];};static_assert(sizeof(Geometry)==48);
    splats=makeBuffer(count*sizeof(Geometry),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    harmonicsLow=makeBuffer(count*sv::shFloatsPerHalf*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    harmonicsHigh=makeBuffer(count*sv::shFloatsPerHalf*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    order=makeBuffer(count*sizeof(uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto*geometry=static_cast<Geometry*>(splats.mapped);auto*low=static_cast<float*>(harmonicsLow.mapped);auto*high=static_cast<float*>(harmonicsHigh.mapped);
    for(size_t i=0;i<count;i++){std::memcpy(&geometry[i],&s.splats[i],sizeof(Geometry));sv::packHarmonics(s.splats[i],low+i*sv::shFloatsPerHalf,high+i*sv::shFloatsPerHalf);}
    for(auto i:s.previewIndices)geometry[i].covB[3]=1;
    updateOrder(indices);
    for(auto&e:eyes){
        VkDescriptorBufferInfo info[]={{splats.buffer,0,splats.size},{order.buffer,0,order.size},{e.camera.buffer,0,e.camera.size},{harmonicsLow.buffer,0,harmonicsLow.size},{harmonicsHigh.buffer,0,harmonicsHigh.size},{viewText.buffer,0,viewText.size}};
        VkWriteDescriptorSet writes[6]{};for(int i=0;i<6;i++){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=e.descriptor;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&info[i];}vkUpdateDescriptorSets(device,6,writes,0,nullptr);
    }
}
void Gpu::updateOrder(const std::vector<uint32_t>&indices){if(indices.size()>count)throw std::runtime_error("Invalid sort size.");drawCount=uint32_t(indices.size());if(drawCount)std::memcpy(order.mapped,indices.data(),drawCount*sizeof(uint32_t));}
void Gpu::render(int index,uint32_t image,const CameraUniform&camera){
    auto&e=eyes[index];recordRender(e,e.framebuffers.at(image),camera);VK(vkEndCommandBuffer(command));submitAndWait();
}
void Gpu::recordRender(EyeTarget&e,VkFramebuffer framebuffer,const CameraUniform&camera){
    std::memcpy(e.camera.mapped,&camera,sizeof(camera));VK(vkResetCommandBuffer(command,0));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;VK(vkBeginCommandBuffer(command,&begin));
    VkClearValue clear{};float grey=srgb?0.029556834f:48.f/255;clear.color={{grey,grey,grey,1}};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=renderPass;rp.framebuffer=framebuffer;rp.renderArea={{0,0},{e.width,e.height}};rp.clearValueCount=1;rp.pClearValues=&clear;vkCmdBeginRenderPass(command,&rp,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{0,0,float(e.width),float(e.height),0,1};VkRect2D scissor{{0,0},{e.width,e.height}};vkCmdSetViewport(command,0,1,&vp);vkCmdSetScissor(command,0,1,&scissor);
    if(count){vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&e.descriptor,0,nullptr);}
    if(drawCount){vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);vkCmdBindIndexBuffer(command,quadIndices.buffer,0,VK_INDEX_TYPE_UINT16);vkCmdDrawIndexed(command,1024*6,(drawCount+1023)/1024,0,0,0);}
    if(count&&camera.options[3]>0){vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,hudPipeline);if((camera.options[3]==1||camera.options[3]==8)&&camera.beamOrigin[3]>0)vkCmdDraw(command,6,1,0,1);vkCmdDraw(command,6,1,0,0);}
    vkCmdEndRenderPass(command);
}
void Gpu::submitAndWait(){
    VK(vkResetFences(device,1,&fence));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;VK(vkQueueSubmit(queue,1,&submit,fence));
    // Correctness-first prototype: no host buffer is reused until both its GPU reads finish.
    VK(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));
}
void Gpu::capture(const CameraUniform&camera,const std::string&path,uint32_t width,uint32_t height){
    EyeTarget e=eyes[0];if(width&&height){if(width>4096||height>4096)throw std::runtime_error("Capture dimensions exceed limit.");e.width=width;e.height=height;}VkImage target=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;
    VkImageView view=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;Buffer pixels;
    auto release=[&]{destroyBuffer(pixels);if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);if(view)vkDestroyImageView(device,view,nullptr);if(target)vkDestroyImage(device,target,nullptr);if(memory)vkFreeMemory(device,memory,nullptr);};
    try{
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={e.width,e.height,1};ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VK(vkCreateImage(device,&ci,nullptr,&target));VkMemoryRequirements mr;vkGetImageMemoryRequirements(device,target,&mr);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(physical,&mp);
        uint32_t type=UINT32_MAX;for(uint32_t i=0;i<mp.memoryTypeCount;i++)if(mr.memoryTypeBits&(1u<<i)){type=i;if(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)break;}
        if(type==UINT32_MAX)throw std::runtime_error("No memory type for comparison capture.");
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=type;VK(vkAllocateMemory(device,&ai,nullptr,&memory));VK(vkBindImageMemory(device,target,memory,0));
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=target;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VK(vkCreateImageView(device,&vi,nullptr,&view));
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fb.renderPass=renderPass;fb.attachmentCount=1;fb.pAttachments=&view;fb.width=e.width;fb.height=e.height;fb.layers=1;VK(vkCreateFramebuffer(device,&fb,nullptr,&framebuffer));
        pixels=makeBuffer(VkDeviceSize(e.width)*e.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        CameraUniform reference=camera;reference.options[3]=0;reference.viewport[0]=e.width;reference.viewport[1]=e.height;recordRender(e,framebuffer,reference);
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=target;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={e.width,e.height,1};vkCmdCopyImageToBuffer(command,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,pixels.buffer,1,&region);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=pixels.buffer;host.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
        VK(vkEndCommandBuffer(command));submitAndWait();
        std::ofstream out(path,std::ios::binary);out<<"P6\n"<<e.width<<' '<<e.height<<"\n255\n";
        auto*data=static_cast<uint8_t*>(pixels.mapped);bool bgra=format==VK_FORMAT_B8G8R8A8_SRGB||format==VK_FORMAT_B8G8R8A8_UNORM;
        std::vector<uint8_t> rgb(size_t(e.width)*e.height*3);
        for(size_t i=0;i<rgb.size()/3;i++){rgb[3*i]=data[4*i+(bgra?2:0)];rgb[3*i+1]=data[4*i+1];rgb[3*i+2]=data[4*i+(bgra?0:2)];}
        out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());out.close();if(!out)throw std::runtime_error("Could not save comparison image.");
    }catch(...){vkDeviceWaitIdle(device);release();throw;}release();
}
void Gpu::releaseSwapchains(){
    if(!device)return;
    vkDeviceWaitIdle(device);
    for(auto&e:eyes){for(auto f:e.framebuffers)if(f)vkDestroyFramebuffer(device,f,nullptr);for(auto v:e.views)if(v)vkDestroyImageView(device,v,nullptr);e.framebuffers.clear();e.views.clear();e.images.clear();if(e.swapchain)xrDestroySwapchain(e.swapchain);e.swapchain=XR_NULL_HANDLE;}
}
void Gpu::cleanup(){
    releaseSwapchains();
    if(device){for(auto&e:eyes){destroyBuffer(e.camera);e={};}
        destroyBuffer(splats);destroyBuffer(harmonicsLow);destroyBuffer(harmonicsHigh);destroyBuffer(order);destroyBuffer(quadIndices);destroyBuffer(viewText);if(hudPipeline)vkDestroyPipeline(device,hudPipeline,nullptr);if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);if(descriptorLayout)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);if(renderPass)vkDestroyRenderPass(device,renderPass,nullptr);if(fence)vkDestroyFence(device,fence,nullptr);if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);vkDestroyDevice(device,nullptr);device=VK_NULL_HANDLE;}
    if(instance){vkDestroyInstance(instance,nullptr);instance=VK_NULL_HANDLE;}
}
