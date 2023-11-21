#pragma once

#include <fstream>

#define GLFW_INLCUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 9001;

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex
{
	glm::vec3 pos; //Vertex Pos(x,y,z)
	glm::vec3 col; //Vertex Col(r,g,b);
};

//Locations (Indices) of Queue Families (if they exists at all)
struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >=0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities; //Surface Properties
	std::vector<VkSurfaceFormatKHR> formats; //Image formats that surface can support
	std::vector<VkPresentModeKHR> presentationModes; //How images should be presented to screen
};

struct SwapchainImage 
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& fileName)
{
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open a file");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	file.seekg(0);

	file.read(fileBuffer.data(),fileSize);
	file.close();
	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	//Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i))//index of memory type must mach corrosponding bit in allowedTypes
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) //Desired propperty bit flags are part of memory's type flags
		{
			//The memory is valid;
			return i;
		}
	}

	return 0;
}


static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, 
						 VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer,
						 VkDeviceMemory* bufferMemory)
{
	//Create Vertex Buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vertex buffer");
	}

	//Get buffer memory Req
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	//Allocate memory to buffer
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits,
		bufferProperties);
	//Allocate memory to VkDevice memory
	result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory");
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool , 
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{

	//If many object are needed to be loaded at the same time have more than one transfer 
	//command buffer and try to do synchronization between them
	
	//Command buffer to hold and transfer commands
	VkCommandBuffer transferCommandBuffer;
	
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = transferCommandPool;
	allocateInfo.commandBufferCount = 1;
	
	//Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocateInfo, &transferCommandBuffer);

	
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//Begin recording transfer commands
	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	//Region of data from copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	vkEndCommandBuffer(transferCommandBuffer);

	//Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);

}