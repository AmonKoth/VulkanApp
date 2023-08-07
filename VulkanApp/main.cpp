#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include <iostream>

#include "VulkanRenderer.h"
#include "VulkanWindow.h"


int main()
{
	VulkanWindow* window = new VulkanWindow("MainWindow",800,600);

	GLFWwindow* mainWindow = window->GetWindow();

	while (!glfwWindowShouldClose(mainWindow))
	{
		glfwPollEvents();
	}

	window->DestoryWindow();
	window = nullptr;

	return 0;
}