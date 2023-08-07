#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

class VulkanWindow
{
private :
	GLFWwindow* mainWindow;

public:
	VulkanWindow(std::string wName, const int width, const int height);
	
	~VulkanWindow();

	void DestoryWindow();
	GLFWwindow* GetWindow() {return mainWindow; }
};

