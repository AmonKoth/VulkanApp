#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include "Utilities.h"



class VulkanRenderer
{

public:
	VulkanRenderer();
	
	int init(GLFWwindow* newWindow);
	void cleanUp();

	~VulkanRenderer();

private:
	GLFWwindow* window;

	//Vulkan Components:
	VkInstance instance;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;

	//Vulkan Functions:
	void createInstance();
	void createLogicalDevice();



	//Get Funcitons
	void getPysicalDevice();

	//Support Functions
	bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);

	QueueFamilyIndices getQueueFamiles(VkPhysicalDevice device);


};

