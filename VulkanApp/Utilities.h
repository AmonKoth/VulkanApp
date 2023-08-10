#pragma once

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