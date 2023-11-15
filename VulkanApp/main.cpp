#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include <iostream>

#include "VulkanRenderer.h"
#include "VulkanWindow.h"

GLFWwindow* mainWindow = nullptr;

VulkanRenderer vulkanRenderer;

int main()
{
	VulkanWindow* window = new VulkanWindow("MainWindow",800,600);

	mainWindow = window->GetWindow();

	if (vulkanRenderer.init(mainWindow) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	while (!glfwWindowShouldClose(mainWindow))
	{
		glfwPollEvents();
		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanUp();
	window->DestroyWindow();
	window = nullptr;

	return 0;
}