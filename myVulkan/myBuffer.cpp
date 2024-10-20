#include "myBuffer.h"

void myBuffer::createCommandPool(VkDevice logicalDevice, QueueFamilyIndices queueFamilyIndices) {

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	//VK_COMMAND_POOL_CREATE_TRANSIENT_BIT����ʾ����������������¼�¼��������ܻ�ı��ڴ������Ϊ��
	//VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT�����������¼�¼������������û�д˱�־�������һ�����������������
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &this->commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

}

void myBuffer::createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue queue, uint32_t verticeSize, std::vector<Vertex>* vertices){

	//sizeof���ܲ�ָ����ָ������͵Ĵ�С
	VkDeviceSize bufferSize = verticeSize;// vertices->size();

	//GPU�������Ļ�������VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT�����ֻ�����GPU�޷�����
	//�����ڶ���������GPU�У���ô������Ҫ�Ƚ����ݴ����ݴ滺�����������ݴ滺�������Ƶ�GPU���ʵĻ������У�Ϊɶ��Ҳ���Ǻ����������������ڲ�ͬ�˵ĸ�ʽ��أ�Ҳ����ʹ���Ĵ����йأ�
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	//���Խ�һ���豸������������GPU��ռ��)��һ���ڴ�����ָ����ӳ��
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices->data(), (size_t)bufferSize);	//�ú���ֻ�����ڶ���CPU�˻�������ʱ��
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->vertexBuffer, this->vertexBufferMemory);

	copyBuffer(logicalDevice, queue, this->commandPool, stagingBuffer, this->vertexBuffer, bufferSize);
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

}

void myBuffer::createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue queue, uint32_t indiceSize, std::vector<uint32_t>* indices) {

	VkDeviceSize bufferSize = indiceSize * indices->size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->indexBuffer, this->indexBufferMemory);

	copyBuffer(logicalDevice, queue, this->commandPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

}

void myBuffer::createUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, uint32_t frameSize) {

	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	//��������ÿ֡���ã�����uniform����ÿ֡��ͬ
	this->uniformBuffers.resize(frameSize);
	this->uniformBuffersMemory.resize(frameSize);
	this->uniformBuffersMapped.resize(frameSize);

	for (size_t i = 0; i < frameSize; i++) {
		createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, this->uniformBuffers[i], this->uniformBuffersMemory[i]);
		//�־�ӳ�䣬��û�����ָ����ֱ�Ӳ���������Ҫͨ��api
		vkMapMemory(logicalDevice, this->uniformBuffersMemory[i], 0, bufferSize, 0, &this->uniformBuffersMapped[i]);
	}

}

void myBuffer::createCommandBuffers(VkDevice logicalDevice, uint32_t frameSize) {

	//��������������ˮ��һ�����ƣ�������Ҫ���ָ�����
	this->commandBuffers.resize(frameSize);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = this->commandPool;
	//VK_COMMAND_BUFFER_LEVEL_PRIMARY�������ύ������ִ�У������ܴ���������������á�
	//VK_COMMAND_BUFFER_LEVEL_SECONDARY������ֱ���ύ�������Դ��������������
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)this->commandBuffers.size();	//ָ����������������������������Ǹ�����������Ĵ�С

	//��������ĵ��������������ǵ����������ָ��Ҳ����������
	if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, this->commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

}

//һ��������ͼ����ͼ����һ��չʾ����
//һ��renderPass����һ֡�����е�������̣������������ͼ����һ��������ͼ��
//һ��frameBuffer��renderPass��һ��ʵ������renderPass�й涨�����ͼ��������
void myBuffer::createFramebuffers(uint32_t swapChainImageViewsSize, std::vector<VkImageView> swapChainImageViews, VkExtent2D swapChainExtent, std::vector<VkImageView> imageViews, VkImageView depthImageView, VkRenderPass renderPass, VkDevice logicalDevice) {

	this->swapChainFramebuffers.resize(swapChainImageViewsSize);
	for (size_t i = 0; i < swapChainImageViewsSize; i++) {

		std::vector<VkImageView> attachments = imageViews;
		attachments.push_back(swapChainImageViews[i]);
		attachments.push_back(depthImageView);

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &this->swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}

	}

}

//frameBuffer��reSwapChain��أ����Է��ڱ�clean
void myBuffer::clean(VkDevice logicalDevice, int frameSize) {

	for (size_t i = 0; i < frameSize; i++) {
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
	}

	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);

	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

}


void myBuffer::createBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create vertex buffer!");
	}

	VkMemoryRequirements memRequirements;
	//���Եõ������ڴ��С���ڴ����Ҫ���Լ��ڴ���������
	//���ǻ�õĴ洢�ռ�Ĵ�С��һ��������ָ��Ҫ�ĵȴ���Ϊ�������ڲ���ϵͳ�Ķ��뷽ʽ�Ȼ���ֲ�һ�����������Ĵ�С����memRequirements�и�����
	//������ڴ�����ָ����
	// 1.�洢�����Ǵ洢VBO�ȶ������ݻ���uniform��ͨ������
	// 2.���ڴ���CPU�ɼ�����GPU�ɼ����Ƕ��ɼ�
	//	2.1 CPU�ɼ����ڴ����host memory
	//	2.2 GPU�ɼ����ڴ����local device memory
	//	2.3 CPU��GPU���ɼ����ڴ����coherent memory
	//�����������usage�и����Ƕ������ݣ���������������ڴ���������Ҳ�Ƕ������ݵ�
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory!");
	}

	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);

}

void myBuffer::copyBuffer(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(logicalDevice, commandPool);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);	//�ú���������������˵Ļ�����������memcpy

	endSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);

}

VkCommandBuffer myBuffer::beginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool) {

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;

}

void myBuffer::endSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool) {

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);
	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);

}

uint32_t myBuffer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {

	VkPhysicalDeviceMemoryProperties memProperties;
	//�ҵ���ǰ�����豸���Կ�����֧�ֵ��Դ����ͣ��Ƿ�֧������洢���������ݴ洢��ͨ�����ݴ洢�ȣ�
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		//�����type�˳���ͨ��one-hot����ģ���0100��ʾ����,1000��ʾ�ĺ�
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");

}