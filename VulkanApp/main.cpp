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


	while (!glfwWindowShouldClose(mainWindow))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;
		angle += 10.0f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }

		glm::mat4 firstModel(1.0f);
		glm::mat4 secondModel(1.0f);

		firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -5.0f));
		firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

		secondModel = glm::translate(secondModel, glm::vec3(2.0f, 0.0f, -5.0f));
		secondModel = glm::rotate(secondModel, glm::radians(-angle *100), glm::vec3(0.0f, 0.0f, 1.0f));

		vulkanRenderer.updateModel(0, firstModel);
		vulkanRenderer.updateModel(1, secondModel);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanUp();
	window->DestroyWindow();
	window = nullptr;

	return 0;
}