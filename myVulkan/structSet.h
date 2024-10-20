#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include<glm/glm.hpp>

#include <array>
#include <optional>

#ifndef STRUCT_SET
#define STRUCT_SET
//所有命令都要被提交到队列中执行，而每个队列属于不同的队列族，不同的队列族支持不同的指令，比如有些队列族只负责数值计算，有些队列族只负责内存操作等
//而队列族是物理设备提供的，即一个物理设备提供某些队列族，如显卡能提供计算、渲染的功能，而不能提供造宇宙飞船的功能
struct QueueFamilyIndices {

	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}

};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
	glm::vec3 pos;
	//glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;
	glm::vec3 tangent;


	static VkVertexInputBindingDescription getBindingDescription() {

		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;

	}

	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {

		//VAO
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);	//找pos在Vertex中的偏移

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, normal);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, tangent);

		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
	}

};

struct Texture {
	//uint32_t id;
	std::string type;
	std::string path;
};

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Texture> textures;

	Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Texture> textures) {
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;
	}

};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	//强制对齐，必须是2的倍数
	glm::vec4 lightPos;
	glm::vec4 cameraPos;
};

struct DescriptorObject {

	VkDescriptorSetLayout discriptorLayout;
	uint32_t uniformBufferNum;
	uint32_t textureNum;
	std::vector<VkDescriptorSet> descriptorSets;
};

#endif