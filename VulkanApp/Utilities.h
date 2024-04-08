#pragma once

#include <fstream>

#define GLFW_INLCUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 20;

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex
{
	glm::vec3 pos; //Vertex Pos(x,y,z)
	glm::vec3 col; //Vertex Col(r,g,b);
	glm::vec2 tex; //Texture Coord(u,v);
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


static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	//If many object are needed to be loaded at the same time have more than one transfer 
//command buffer and try to do synchronization between them

//Command buffer to hold and transfer commands
	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = commandPool;
	allocateInfo.commandBufferCount = 1;

	//Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);


	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //Command buffer is only used once

	//Begin recording transfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;

}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	//Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool , 
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	//Region of data from copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferImageCopy bufferImageRegion = {};
	bufferImageRegion.bufferOffset = 0;												//start
	bufferImageRegion.bufferRowLength = 0;											//RowLength to calculate data spacing
	bufferImageRegion.bufferImageHeight = 0;										//image height to calculate data spacing
	bufferImageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;		//Which aspect of image to copy
	bufferImageRegion.imageSubresource.mipLevel = 0;								//Mipmap level to copy
	bufferImageRegion.imageSubresource.baseArrayLayer = 0;							//Starting array layer (if array)
	bufferImageRegion.imageSubresource.layerCount = 1;								//Number of layer to copy at starting base layer(ie qube maps)
	bufferImageRegion.imageOffset = { 0,0,0 };										//offset into image
	bufferImageRegion.imageExtent = { width, height,1 };							//Size of region to copy as x,y,z

	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1, &bufferImageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}


static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout;									//Layout to transition from
	imageMemoryBarrier.newLayout = newLayout;									//Layout to transition to
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			//Queue family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			//Queue family to transition to
	imageMemoryBarrier.image = image;											//Image being modified
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	//Aspect of image being altered
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;						//First mip level to start alteration on
	imageMemoryBarrier.subresourceRange.levelCount = 1;							//Number of mip levels to alter starting from base
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;						//First layer to start alterations on
	imageMemoryBarrier.subresourceRange.layerCount = 1;							//Number of layer to alter starting from the base

	VkPipelineStageFlags srcStage = 0;
	VkPipelineStageFlags dstStage = 0;

	//If transitioning from new image to image ready to recive data
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;										//Memory access stage hapens whenever
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;			//Memory access stage hapens before transfer write happens
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	//If transitioning from transfer destination to shader readable
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	}

	vkCmdPipelineBarrier(commandBuffer,
		srcStage, dstStage,						//Pipeline Stages( match to src and dst access masks)
		0,										//Dependency Flag
		0, nullptr,								//Memory barrier count + data
		0, nullptr,								//Buffer memory count+ data
		1, &imageMemoryBarrier);				//Image memory barrier count + data

	
	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}