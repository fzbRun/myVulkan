#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include<glm/glm.hpp>

#include <iostream>
#include<array>
#include <vector>

#include "structSet.h"

#ifndef MY_BUFFER
#define MY_BUFFER

class myBuffer {

public:

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	//一个大的全塞进去，然后用offset
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	void createCommandPool(VkDevice logicalDevice, QueueFamilyIndices queueFamilyIndices);
	void createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue queue, uint32_t verticeSize, std::vector<Vertex>* vertices);
	void createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue queue, uint32_t indiceSize, std::vector<uint32_t>* indices);
	void createUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t frameSize);
	void createCommandBuffers(VkDevice logicalDevice, uint32_t frameSize);
	void createFramebuffers(uint32_t swapChainImageViewsSize, std::vector<VkImageView> swapChainImageViews, VkExtent2D swapChainExtent, std::vector<VkImageView> imageViews, VkImageView depthImageView, VkRenderPass renderPass, VkDevice logicalDevice);

	void clean(VkDevice logicalDevice, int frameSize);

	static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	static void copyBuffer(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	static VkCommandBuffer beginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
	static void endSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);
	static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

};

#endif
