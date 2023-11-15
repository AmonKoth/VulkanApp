#include "VulkanWindow.h"

VulkanWindow::VulkanWindow(std::string wName, const int width, const int height)
{
	glfwInit();

	//GLFW not set for openGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	//Window Not Resizable
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mainWindow = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

VulkanWindow::~VulkanWindow()
{

}

void VulkanWindow::DestroyWindow()
{
	glfwDestroyWindow(mainWindow);
	mainWindow = nullptr;
	glfwTerminate();
}
