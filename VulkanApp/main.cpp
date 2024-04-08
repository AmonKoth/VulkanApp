#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;
	float zposition = -4.0f;
	bool bIncreasing = false;

	int house = vulkanRenderer.createMeshModel("Models/cottage_obj.obj");

	while (!glfwWindowShouldClose(mainWindow))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;
		angle += 10.0f * deltaTime;
		if (bIncreasing) zposition += 0.60f * deltaTime;
		else zposition -= 0.60f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }

		if (zposition >= -3.0f) bIncreasing = false;
		if (zposition <= -6.0f) bIncreasing = true;

		glm::mat4 testMat = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
		testMat = glm::translate(testMat, glm::vec3(0.0f, -2.0f, -10.0f));
		vulkanRenderer.updateModel(house,testMat);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanUp();
	window->DestroyWindow();
	window = nullptr;

	return 0;
}