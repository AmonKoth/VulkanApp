#include "VulkanRenderer.h"

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
        getPysicalDevice();
        createLogicalDevice();
    }
    catch (const std::runtime_error& e)
    {
        printf("ERROR %s", e.what());
        return EXIT_FAILURE;
    }
    return 0;
}
void VulkanRenderer::cleanUp()
{
    vkDestroyInstance(instance,nullptr);
    vkDestroyDevice(mainDevice.logicalDevice,nullptr);
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
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    //Create Instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Fail to create a Vulkan instance");
    }
}

void VulkanRenderer::createLogicalDevice()
{

    QueueFamilyIndices indices = getQueueFamiles(mainDevice.physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
    queueCreateInfo.queueCount = 1;
    float priority = 1.0f;
    queueCreateInfo.pQueuePriorities = &priority; //1 is highest;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
  
    VkResult result = vkCreateDevice(mainDevice.physicalDevice,&deviceCreateInfo,nullptr,&mainDevice.logicalDevice);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Logical Device!");
    }
    //From given logical device of givin queue family of given queue index place refrence in given queue
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
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

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    */

    QueueFamilyIndices indices = getQueueFamiles(device);

    return indices.isValid();
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
        if (indices.isValid())
        {
            break;
        }
        i++;
    }

    return indices;
}


