#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>

#include "Utilities.h"
#include "Mesh.h"

class VulkanRenderer
{

public:
	VulkanRenderer();
	
	int init(GLFWwindow* newWindow);

	void updateModel(int modelID, glm::mat4 newModel);

	void draw();
	void cleanUp();

	~VulkanRenderer();

private:
	GLFWwindow* window;

	int currentFrame = 0;

	//Scene Objects
	std::vector<Mesh> meshList;

	//Scen Settings
	//Model View Projection
	struct UBOViewProjection {
		glm::mat4 projection;
		glm::mat4 view;
	}uboViewProjection;
	//Vulkan Components:
	//Main
	VkInstance instance;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;

	std::vector<SwapchainImage> swapchainImages;
	std::vector<VkFramebuffer> swapchainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	//Descriptors
	VkDescriptorSetLayout descriptorSetLayout;

	VkDescriptorPool descriptorPool;

	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkBuffer> vpUniformBuffer;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;

	std::vector<VkBuffer> modelDynamicUniformBuffer;
	std::vector<VkDeviceMemory> modelDynamicUniformBufferMemory;

	VkDeviceSize minUniformBufferOffset;
	size_t modelUniformAllignment;
	UBOModel* modelTransferSpace;

	//Pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	//Pools
	VkCommandPool graphicsCommandPool;

	//Syncronisation
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	//Utility
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;


	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	#ifdef NDEBUG
		const bool enableValidationLayers = false;
	#else
		const bool enableValidationLayers = true;
	#endif // NDEBUG
	
	VkDebugUtilsMessengerEXT debugMessenger;


	//Vulkan Functions:
	//Create Functions
	void createInstance();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void createLogicalDevice();
	void setupDebugMessenger();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFrameBuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();
	
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);

	//Record functions
	void recordCommand();

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
	//Get Funcitons
	void getPysicalDevice();

	void allocateDynamicBufferTransferSpace();
	std::vector<const char*> getRequriredExtensions();

	//Support Functions
	bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {
		printf("Validation layer: %s \n", pCallbackData->pMessage);
		return VK_FALSE;
	}

	QueueFamilyIndices getQueueFamiles(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

	//Choose Funcionts
	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &surfaceFormats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	//Support Create Functions
	VkImageView createImageView(VkImage image, VkFormat imageformat, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char> &code);

};

