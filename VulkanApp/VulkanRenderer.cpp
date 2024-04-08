#include "VulkanRenderer.h"
#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

VulkanRenderer::VulkanRenderer()
{

}
int VulkanRenderer::init(GLFWwindow* newWindow)
{
    window = newWindow;
    createInstance();
    try
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        getPysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createDepthBufferImage();
        createRenderPass();
        createDescriptorSetLayout();
        createPushConstantRange();
        createGraphicsPipeline();
        createFrameBuffers();
        createCommandPool();

        createCommandBuffers();
        createTextureSampler();
        //allocateDynamicBufferTransferSpace();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createSynchronisation();

        uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
        uboViewProjection.view = glm::lookAt(glm::vec3(30.0f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, -4.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        uboViewProjection.projection[1][1] *= -1;

        int firstTexture = createTexture("plain.png");


    }
    catch (const std::runtime_error& e)
    {
        printf("ERROR %s", e.what());
        return EXIT_FAILURE;
    }
    return 0;
}
void VulkanRenderer::updateModel(int modelID, glm::mat4 newModel)
{
    if (modelID >= modelList.size()) { return; }
    modelList[modelID].setModel(newModel);
}
void VulkanRenderer::draw()
{
    // Get Next Image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    
    vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

    recordCommand(imageIndex);
    updateUniformBuffers(imageIndex);

    //SUBMIT Command Buffer to Render
    VkSubmitInfo submitInfo = {  };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable[currentFrame]; //Signal to be waited
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished[currentFrame]; //Signals when finished
    
    VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit Command Buffer to queue");
    }

    //Present rendered image to screen;
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished[currentFrame]; //Semaphores to wait on
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    
    result = vkQueuePresentKHR(presentationQueue,&presentInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present Image");
    }
    currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}
void VulkanRenderer::cleanUp()
{
    vkDeviceWaitIdle(mainDevice.logicalDevice);

    for (size_t i = 0; i < modelList.size(); i++)
    {
        modelList[i].destroyMesh();
    }

    //_aligned_free(modelTransferSpace);
    vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

    vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
    for (size_t i =0; i<textureImages.size(); i++)
    {
        vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
        vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, textureImagesMemory[i], nullptr);
    }

    vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
    vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
    vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory, nullptr);

    vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout,nullptr);
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
        //vkDestroyBuffer(mainDevice.logicalDevice, modelDynamicUniformBuffer[i], nullptr);
        //vkFreeMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[i], nullptr);
    }
    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
        vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
        vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
    }
    vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
    for (auto framebuffer : swapchainFramebuffers)
    {
        vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
    }
    vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
    for (auto image : swapchainImages)
    {
        vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
    }
    vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    vkDestroyDevice(mainDevice.logicalDevice,nullptr);
    if (enableValidationLayers)
    {
       DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);

}
VulkanRenderer::~VulkanRenderer()
{

}

void VulkanRenderer::createInstance()
{
    //App info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Vulkan Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1; 

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    //Extensions list
    std::vector<const char*> instanceExtensions = std::vector<const char*>();
    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions; 
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (size_t i = 0; i < glfwExtensionCount; i++)
    {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    //For INSTANCE extensions
    if (!checkInstanceExtensionSupport(&instanceExtensions))
    {
        throw std::runtime_error("VkInstance does not support required extensions!");
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    //For ValidationLayers
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested but not available");
    }
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    std::vector<const char*> extensions = getRequriredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    //Create Instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Fail to create a Vulkan instance");
    }
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {}; 
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanRenderer::createLogicalDevice()
{

    QueueFamilyIndices indices = getQueueFamiles(mainDevice.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = { indices.graphicsFamily,indices.presentationFamily };

    for (int queueFamilyIndex : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        float priority = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority; //1 is highest;

        queueCreateInfos.push_back(queueCreateInfo);
    }


    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    //Physical Device features that the Logical device will be using
    VkPhysicalDeviceFeatures deviceFeatures = {};
    if (checkDeviceSuitable(mainDevice.physicalDevice))
    {
        deviceFeatures.samplerAnisotropy = VK_TRUE; //Enable anisotropy
    }
    else
    {
        deviceFeatures.samplerAnisotropy = VK_FALSE; //Disable anisotropy if device does not support it
    }
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
  
    VkResult result = vkCreateDevice(mainDevice.physicalDevice,&deviceCreateInfo,nullptr,&mainDevice.logicalDevice);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Logical Device!");
    }
    //From given logical device of givin queue family of given queue index place refrence in given queue
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::setupDebugMessenger()
{
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up debug messenger");
    }
}

void VulkanRenderer::createSurface()
{
    //THANK YOU GLFW
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create surface!");
    }
}

void VulkanRenderer::createSwapChain()
{
    SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
    VkPresentModeKHR presentationMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount+1;
    if (swapChainDetails.surfaceCapabilities.maxImageCount>0 &&
        swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
    {
        imageCount = swapChainDetails.surfaceCapabilities.minImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.presentMode = presentationMode;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.clipped = VK_TRUE;
    
    QueueFamilyIndices indices = getQueueFamiles(mainDevice.physicalDevice);
    
    if (indices.graphicsFamily != indices.presentationFamily)
    {
        uint32_t queueFamilyIndices[] = {
        (uint32_t)indices.graphicsFamily,
        (uint32_t)indices.presentationFamily,
        };

        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    //If old swap chain being destroyed and this one replaces it then link old one to quickly hand over responsiblities. Like resizing the app
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Swapchain!");
    }

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage>images(swapchainImageCount);
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());
    
    for (VkImage image : images)
    {
        SwapchainImage swapchainImage = {};
        swapchainImage.image = image;

        swapchainImage.imageView = createImageView(image,swapChainImageFormat,VK_IMAGE_ASPECT_COLOR_BIT);

        swapchainImages.push_back(swapchainImage);
    }

}

void VulkanRenderer::createRenderPass()
{
    //Attachments
    //Color Attachment of Render Pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //Depth Attachment of render Pass
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    //References
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // RayTracing option is here
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;


    std::array<VkSubpassDependency, 2> subpassDependencies;

    //Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    //Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription, 2 > renderPassAttachments = { colorAttachment,depthAttachment };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
    renderPassCreateInfo.pAttachments = renderPassAttachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create renderpass");
    }
}

void VulkanRenderer::createDescriptorSetLayout()
{
    //UniformValues DescriptorSetLayout
    VkDescriptorSetLayoutBinding vpLayoutBinding = {};
    vpLayoutBinding.binding = 0;
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpLayoutBinding.descriptorCount = 1;
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vpLayoutBinding.pImmutableSamplers = nullptr; //For texture

    //VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    //modelLayoutBinding.binding = 1;
    //modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    //modelLayoutBinding.descriptorCount = 1;
    //modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    //modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutCreateInfo.pBindings = layoutBindings.data();

    VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a descriptor set layout");
    }

    //Create Texture Sampler DescriptorSet Layout
    //Texture binding INfo
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    //Create a descriptorSet layout with given bindings for texture
    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    //Create descriptor set layout
    result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a descriptor set layout");
    }
}

void VulkanRenderer::createPushConstantRange()
{
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::createGraphicsPipeline()
{
    auto vertexShaderCode = readFile("Shaders/vert.spv");
    auto fragmentShaderCode = readFile("Shaders/frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexCreateInfo = {};
    vertexCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexCreateInfo.module = vertexShaderModule;
    vertexCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentCreateInfo = {};
    fragmentCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentCreateInfo.module = fragmentShaderModule;
    fragmentCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexCreateInfo , fragmentCreateInfo };

    // Vertex data as a whole

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0; //Can bind mult streams of data
    bindingDescription.stride = sizeof(Vertex); //Size of single vertex object
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //Index: Move on to next vertex, Instance: move to a vertex for the next instance(applies if you draw a lots of same object)

    std::array<VkVertexInputAttributeDescription, 3> attributeDiscriptions;

    attributeDiscriptions[0].binding = 0; // Which binding the date is at(should be same as above)
    attributeDiscriptions[0].location = 0; //value of the location;
    attributeDiscriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDiscriptions[0].offset = offsetof(Vertex, pos);

    //Color Attrib
    attributeDiscriptions[1].binding = 0; // Which binding the date is at(should be same as above)
    attributeDiscriptions[1].location = 1; //value of the location;
    attributeDiscriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDiscriptions[1].offset = offsetof(Vertex, col);
     
    //Texture Attrib
    attributeDiscriptions[2].binding = 0; // Which binding the date is at(should be same as above)
    attributeDiscriptions[2].location = 2; //value of the location;
    attributeDiscriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDiscriptions[2].offset = offsetof(Vertex, tex);

    //Create Pipeline
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDiscriptions.size());
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDiscriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { };
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    //ViewPort
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0,0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;


    // In case of making the window resizable

    ////Dynaimc States
    //std::vector<VkDynamicState> dynamicStateEnable;
    //dynamicStateEnable.push_back(VK_DYNAMIC_STATE_VIEWPORT); // Can Resize command buffer with vkCmdSetViewport(commandbuffer,0,1,&viewport);
    //dynamicStateEnable.push_back(VK_DYNAMIC_STATE_SCISSOR); // Can Resize command buffer with vkCmdSetScissor(commandbuffer,0,1,&viewport);
    ////Everytime the window is resized you have to destroy swapchain and and re zie it as well

    //VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    //dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    //dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnable.size());

    //Rasteriser
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE; // To enalbe this you need to enable this in device features as well
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth = 1.0f;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
    
    // Multisampling -- Disabled for now
    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    //Blending

    VkPipelineColorBlendAttachmentState colorStateAttachment = {};
    colorStateAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorStateAttachment.blendEnable = VK_TRUE;

    //equation for blending : (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
    colorStateAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorStateAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorStateAttachment.colorBlendOp = VK_BLEND_OP_ADD;

    // (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)

    colorStateAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorStateAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorStateAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    // (1 * new alph) +(0 * old aplha)

    VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
    colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendingCreateInfo.attachmentCount = 1;
    colorBlendingCreateInfo.pAttachments = &colorStateAttachment;

    //Pipeline Layout
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = { descriptorSetLayout,samplerSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout");
    }

    // depth stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; //Comparision operation that allows and overwrite
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE; // to enable the stencilTest


    //Graphics Pipeline Creation 
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr; //For now its null
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo; //Not yet
    pipelineCreateInfo.layout = pipelineLayout; 
    pipelineCreateInfo.renderPass = renderPass; //Render pass description the pipeline is compatible with
    pipelineCreateInfo.subpass = 0;             //Subpass of the renderpass to use with pipeline

    //Can create mmultiple pipelines that derive from one another for optimisation
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Base one
    pipelineCreateInfo.basePipelineIndex = -1; //Or index of the pipeline being created to derive from

    result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to Create a graphics pipeline");
    }

    //Destryo Shader Modules

    vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

}

void VulkanRenderer::createDepthBufferImage()
{
    //With stencil buffer
    depthFormat = chooseSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT , VK_FORMAT_D32_SFLOAT , 
                                                   VK_FORMAT_D24_UNORM_S8_UINT },VK_IMAGE_TILING_OPTIMAL, 
                                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthBufferImage = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory);

    depthBufferImageView = createImageView(depthBufferImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::createFrameBuffers()
{
    swapchainFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {
            swapchainImages[i].imageView,
            depthBufferImageView
        };

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = swapChainExtent.width;
        framebufferCreateInfo.height = swapChainExtent.height;
        framebufferCreateInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a Framebuffer");
        }
    }
}

void VulkanRenderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = getQueueFamiles(mainDevice.physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

    VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics command pool");
    }
}

void VulkanRenderer::createCommandBuffers()
{
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo cbAllocInfo = {};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = graphicsCommandPool;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult result =  vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allacote Command Buffers!");
    }
}

void VulkanRenderer::createSynchronisation()
{
    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) ||
            vkCreateFence(mainDevice.logicalDevice,&fenceCreateInfo,nullptr, &drawFences[i]))
        {
            throw std::runtime_error("Failed to create at least one semaphore and/or Fence!");
        }
    }
}

void VulkanRenderer::createTextureSampler()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;     
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;                         //How to render when image is magnified on screen
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;                         //How to render when image is minified on screen
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;        //How to handle texture wrap in U (x) direction
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;        //How to handle texture wrap in V (y) direction
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;        //How to handle texture wrap in W (z) direction
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;       //Border beyond texture, Only works for border clamp(SHADOWMAPS!!!)
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;                   //Yes to normalized coords(UV is between 0-1)
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;           //Mip map interpolation mode;
    samplerCreateInfo.mipLodBias = 0.0f;                                    //Add a bias to mipmap level of detail
    samplerCreateInfo.minLod = 0.0f;                                        //Minimum level of detal to pick mip level
    samplerCreateInfo.maxLod = 0.0f;                                        //Maximum level of detal to pick mip level
    if (checkDeviceSuitable(mainDevice.physicalDevice))
    {
        samplerCreateInfo.anisotropyEnable = VK_TRUE;                       //Enable Anisotropy
    }
    else
    {
        samplerCreateInfo.anisotropyEnable = VK_FALSE;                      //Disable if device does not support it
    }
    samplerCreateInfo.maxAnisotropy = 16;                                   //Anisotropy sample level

    VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler");
    }

}

void VulkanRenderer::createUniformBuffers()
{
    //View projection buffer size
    VkDeviceSize vpBufferSize = sizeof(UBOViewProjection);

    //ModelBufferSize
    //VkDeviceSize modelBufferSize = modelUniformAllignment * MAX_OBJECTS;

    vpUniformBuffer.resize(swapchainImages.size());
    vpUniformBufferMemory.resize(swapchainImages.size());
    //modelDynamicUniformBuffer.resize(swapchainImages.size());
    //modelDynamicUniformBufferMemory.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &vpUniformBuffer[i], &vpUniformBufferMemory[i]);
        //createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        //    &modelDynamicUniformBuffer[i], &modelDynamicUniformBufferMemory[i]);
    }
}

void VulkanRenderer::createDescriptorPool()
{
    //Create Unifor Descriptor Pool
    //ViewProjeciton Pool
    VkDescriptorPoolSize vpPoolSize = {};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    ////Model Projection Pool
    //VkDescriptorPoolSize modelPoolSize = {};
    //modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    //modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynamicUniformBuffer.size());

    std::vector<VkDescriptorPoolSize> poolSizeList = { vpPoolSize };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t> (swapchainImages.size());
    poolCreateInfo.poolSizeCount = static_cast<uint32_t> (poolSizeList.size());
    poolCreateInfo.pPoolSizes = poolSizeList.data();

    VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Pool");
    }

    //Create Sampler Descriptor Pool
    //Texture sampler Pool
    VkDescriptorPoolSize samplerPoolSize = {};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; //Separate this later
    //Since I'm creating the images and the descriptor sets at the same time, I'm assuming there will be one texture for each objects which is not optimal way to do this
    samplerPoolSize.descriptorCount = MAX_OBJECTS; 

    VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a sampler Descriptor Pool");
    }

}

void VulkanRenderer::createDescriptorSets()
{
    descriptorSets.resize(swapchainImages.size());

    std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(),descriptorSetLayout);

    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
    setAllocateInfo.pSetLayouts = setLayouts.data();

    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocateInfo, descriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Descriptor Sets");
    }

    for (size_t i = 0; i < descriptorSets.size(); i++)
    {
        //View Projeciton Buffer info and data offset info
        VkDescriptorBufferInfo vpBufferInfo = {};
        vpBufferInfo.buffer = vpUniformBuffer[i];
        vpBufferInfo.offset = 0;
        vpBufferInfo.range = sizeof(UBOViewProjection);

        //Data about connection between binding and data
        VkWriteDescriptorSet vpSetWrite = {};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i];
        vpSetWrite.dstBinding = 0;
        vpSetWrite.dstArrayElement = 0;
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpSetWrite.descriptorCount = 1;
        vpSetWrite.pBufferInfo = &vpBufferInfo;


        //Model Projeciton Buffer info and data offset info
        //VkDescriptorBufferInfo modelBufferInfo = {};
        //modelBufferInfo.buffer = modelDynamicUniformBuffer[i];
        //modelBufferInfo.offset = 0;
        //modelBufferInfo.range = modelUniformAllignment; // this is dynamic it's gonna start from 0 and jump to the model block size

        //VkWriteDescriptorSet modelSetWrite = {};
        //modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //modelSetWrite.dstSet = descriptorSets[i];
        //modelSetWrite.dstBinding = 1;
        //modelSetWrite.dstArrayElement = 0;
        //modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        //modelSetWrite.descriptorCount = 1;
        //modelSetWrite.pBufferInfo = &modelBufferInfo;

        std::vector<VkWriteDescriptorSet> descriptorSets = { vpSetWrite  };

        vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
                               0, nullptr);
    }

}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
    //Copy VP Data
    void* data;
    vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UBOViewProjection), 0, &data);
    memcpy(data, &uboViewProjection, sizeof(UBOViewProjection));
    vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

    //For dynamic uniform Buffers
    //Copy Model Data 
    //for (size_t i = 0; i < meshList.size(); i++)
    //{
    //    UBOModel* model = (UBOModel*)((uint32_t)modelTransferSpace + (i * modelUniformAllignment));
    //    *model = meshList[i].getModel();
    //}

    //vkMapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex], 0, modelUniformAllignment*meshList.size(), 0, &data);
    //memcpy(data, modelTransferSpace, modelUniformAllignment * meshList.size());
    //vkUnmapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::recordCommand(uint32_t currentImage)
{
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    //only needed for Graphical Applications
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset = { 0,0 };
    renderPassBeginInfo.renderArea.extent = swapChainExtent;
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {{0.6f,0.65f,0.4f,1.0f}};
    clearValues[1].depthStencil.depth = 1.0f;
    renderPassBeginInfo.pClearValues = clearValues.data(); //List of clear values
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

       renderPassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];
       VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
       if (result != VK_SUCCESS)
       {
           throw std::runtime_error("Failed to start recording a Command Buffer");
       }
       //Start recording
       
        vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);
        //Begin Render Pass

            //Bind pipeline to be used in render pass
            vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
                
            for (size_t j = 0; j < modelList.size(); j++)
            {
                MeshModel thisModel = modelList[j];
                glm::mat4 matModel = thisModel.getModel();

                vkCmdPushConstants(commandBuffers[currentImage],
                    pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(Model),
                    &matModel);

                for (size_t k = 0; k < thisModel.getMeshCount(); k++)
                {
                    VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer()};
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

                    vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                    //Dynamic offset amount
                    //uint32_t dynamicOffset = static_cast<uint32_t> (modelUniformAllignment) * j; //Mult with current mesh graph



                    std::array<VkDescriptorSet, 2> descriptorSetGroup = {descriptorSets[currentImage],
                    samplerDescriptorSets[thisModel.getMesh(k)->getTextureID()] };

                    vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                        0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

                    //Execute pipeline
                    vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
                }
            }
        //End renderPass
        vkCmdEndRenderPass(commandBuffers[currentImage]);
       //End recording
       result = vkEndCommandBuffer(commandBuffers[currentImage]);
       if (result != VK_SUCCESS)
       {
           throw std::runtime_error("Failed to stop recording a Command Buffer");
       }

}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanRenderer::getPysicalDevice()
{
    // All pysical deviced the vkInstance can access
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("Can't find GPUs that support Vulkan instance!");
    }

    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

    // Just get the first one
    for (const auto& device : deviceList)
    {
        if (checkDeviceSuitable(device))
        {
            mainDevice.physicalDevice = device;
            break;
        }
    }
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(mainDevice.physicalDevice,&deviceProperties);

    //minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

//void VulkanRenderer::allocateDynamicBufferTransferSpace()
//{
//    //Calculate model allignment
//    modelUniformAllignment = sizeof(UBOModel) + (minUniformBufferOffset-1) 
//                             & ~(minUniformBufferOffset - 1);
//
//    //Create space in memory to hold dynamic buffer that is aligned to our required alignment and holds Max Objects
//    modelTransferSpace = (UBOModel*) _aligned_malloc(modelUniformAllignment * MAX_OBJECTS,modelUniformAllignment);
//}

std::vector<const char*> VulkanRenderer::getRequriredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
  
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    for(const auto &checkExtensions: *checkExtensions)
    { 
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(checkExtensions, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) return false;
        
    }

    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    if (extensionCount == 0)
    {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
    
    for (const auto& deviceExtension : deviceExtensions)
    {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(deviceExtension, extension.extensionName) == 0)
            {
                hasExtension = true;
                break;
           }
        }
        if (!hasExtension)
        {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
    /*
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    */

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = getQueueFamiles(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainValid = false;
    if (extensionsSupported)
    {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }

    return indices.isValid() && extensionsSupported && swapChainValid &&deviceFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkValidationLayerSupport()
{

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }

    return true;
}

QueueFamilyIndices VulkanRenderer::getQueueFamiles(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilyList)
    {
        //Check if queue fam has atleast one member and check if its the required type
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i; //If valid the get index;
        }

        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
        if (queueFamily.queueCount > 0 && presentationSupport)
        {
            indices.presentationFamily = i;
        }

        if (indices.isValid())
        {
            break;
        }
        i++;
    }

    return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
    SwapChainDetails swapChainDetail;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetail.surfaceCapabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        swapChainDetail.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetail.formats.data());
    }

    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);
    if (presentationCount != 0)
    {
        swapChainDetail.presentationModes.resize(presentationCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetail.presentationModes.data());
    } 

    return swapChainDetail;
}

//For this program best format is : VK_FORMAT_R8G8B8A8_UNORM , VK_FORMAT_B8G8R8A8_UNOR as backup
//Color space : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_R8G8B8A8_UNORM ,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }
    for (const auto& format : formats)
    {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes)
{
    for (const auto& presentationMode : presentationModes)
    {
        if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentationMode;
        }
    }
    //This is always available in Vulkan
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surfaceCapabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D newExtent = {};
        newExtent.width = static_cast<uint32_t>(width);
        newExtent.height = static_cast<uint32_t>(height);

        newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width,newExtent.width));
        newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height,newExtent.height));
        
        return newExtent;
    }

}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
    //Loop throught the options and find compatible one
    for (VkFormat format : formats)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }

    }

    throw std::runtime_error("Failed to find a matching format");
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat imageformat, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = imageformat;

    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an image view");
    }
    return imageView;

}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

VkImage VulkanRenderer::createImage(uint32_t witdh, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
    //Create Image

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = witdh;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = useFlags;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; // For Multisampling
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    VkImage image;
    VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image");
    }

    //Create memory for Image

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice,memoryRequirements.memoryTypeBits,propFlags);

    result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for Image");
    }

    //Connect memory to image
    vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

    return image;
}

int VulkanRenderer::createTextureImage(std::string fileName)
{
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc* imageData = loadTextureFile(fileName, &width, &height, &imageSize);

    //create staging buffer to hold loaded date ready to copy to device
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;
    createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &imageStagingBuffer, &imageStagingBufferMemory);

    void* data;
    vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, imageData, static_cast<size_t>(imageSize));
    vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

    stbi_image_free(imageData);

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;

    textureImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImageMemory);

    //Copy data to image

    //Transition image to be dst for copy operation
    transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, textureImage,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //Copy Image data
    copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, textureImage, width, height);

    //Transition image to be shader readable
    transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, textureImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //Add texture data to vector for reference (texture manager)
    textureImages.push_back(textureImage);
    textureImagesMemory.push_back(textureImageMemory);

    //Destroy staging buffers
    vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
    vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

    //Return the index of new image
    return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(std::string fileName)
{
    int textureImageLocation = createTextureImage(fileName);

    VkImageView imageView = createImageView(textureImages[textureImageLocation], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    textureImageViews.push_back(imageView);

    int descriptorLoc = createTextureDescriptor(imageView);
    
    
    return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
    VkDescriptorSet descriptorSet;

    //Descriptor set allocation info
    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = samplerDescriptorPool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &samplerSetLayout;

    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocateInfo, &descriptorSet);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate texture descriptor sets");
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //Image layout when in use
    imageInfo.imageView = textureImage;                               //Image to bind to set
    imageInfo.sampler = textureSampler;                               //Sampler to use for set

    VkWriteDescriptorSet descriptorWrite = {  };
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    //Update the new descriptor set
    vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

    //Add descriptor set to list
    samplerDescriptorSets.push_back(descriptorSet);

    return samplerDescriptorSets.size() - 1;
    
}

int VulkanRenderer::createMeshModel(std::string modelFile)
{
    //Import Model "scene
    Assimp::Importer importer;
    //Add GenSmoothnormals if using lighting 
    const aiScene* scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if (!scene)
    {
        throw std::runtime_error("Failed to load model! (" + modelFile + " )");
    }
    //Get vector of all material with 1:1 ID placement
    std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

    //Conversion from the materials list IDs to Descriptor Array IDs

    //TODO PROBLEM WITH LOADING THE TEXTURES FIX
    std::vector<int> matToTex(textureNames.size());
    for (size_t i = 0; i < textureNames.size(); i++)
    {
        if (textureNames[i].empty())
        {
            matToTex[i] = 1; //FIX THIS
        }
        else
        {
            matToTex[i] = createTexture(textureNames[i]);
        }
    }

    //Load all meshes
    std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool
        , scene->mRootNode, scene, matToTex);

    MeshModel meshModel = MeshModel(modelMeshes);
    modelList.push_back(meshModel);

    return modelList.size() - 1;
}

stbi_uc* VulkanRenderer::loadTextureFile(std::string fileName, int* width, int* height, VkDeviceSize* imageSize)
{
    // Number of channels image uses
    int channels;

    std::string fileLocation = "Textures/" + fileName;

    stbi_uc* image = stbi_load(fileLocation.c_str(), width, height, &channels, STBI_rgb_alpha);

    if (!image)
    {
        printf("Failed to load a texture file");
    }

    //Getting the values and multiplying with the number of channels which is 4 in this case
    *imageSize = *width * *height * 4;

    return image;

}
