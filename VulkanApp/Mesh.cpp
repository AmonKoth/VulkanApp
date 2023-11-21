#include "Mesh.h"

Mesh::Mesh()
{
} 

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{
	vertexCount = vertices->size();
	indexCount = indices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(transferQueue, transferCommandPool,vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);

	uboModel.model = glm::mat4(1.0f);
}

void Mesh::setModel(glm::mat4 model)
{
	uboModel.model = model;
}

UBOModel Mesh::getModel()
{
	return uboModel;
}

int Mesh::getVertexCount()
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

int Mesh::getIndexCount()
{
	return indexCount;
}

VkBuffer Mesh::getIndexBuffer()
{
	return indexBuffer;
}

void Mesh::destroyBuffers()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

Mesh::~Mesh()
{
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

	//Temp stage buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;


	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		&stagingBuffer, &stagingBufferMemory);

	//Map memory to vertex buffer
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices->data(),(size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	//The actual buffer that gpu is gonna use
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);
	
	//Copy staging buffer to vertex buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();
	
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, &indexBufferMemory);
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory,nullptr);
}
