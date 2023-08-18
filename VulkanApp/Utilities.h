#pragma once

#include <fstream>

const int MAX_FRAME_DRAWS = 2;

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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