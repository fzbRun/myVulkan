#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <chrono>
#include<stdexcept>
#include<functional>
#include<cstdlib>
#include<cstdint>
#include<limits>
#include<fstream>

#include "structSet.h"
#include "myDevice.h"
#include "myBuffer.h"
#include "myImage.h"
#include "mySwapChain.h"
#include "myModel.h"
#include "myCamera.h"
#include "myDescriptor.h"


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

//层主要是对vulkan函数的重载，比如vulkan有一个函数A，那么层1可以对这个函数进行重载，层2也可以，基本是上层对下层的重载，如层1可以对层2的进行重载
//我们不关心层如何重载，我们只关心最后可以得到哪些功能
//我们使用的下面的这个层的功能是debug的，是层可以有debug的功能而不是只能debug，虽然其他功能现在我还不知道
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"	//包含所有校验层，检测参数值是否合法；主要包括：
									//追踪对象的创建和清除操作，发现资源泄露问题；
									//追踪调用线程，看是否线程安全；
									//将API调用和调用的参数写入日志；
									//追踪API调用，进行分析和回放
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//如果不调试，则关闭校验层
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebygMessenger) {
	//由于是扩展函数，所以需要通过vkGetInstanceProcAddr获得该函数指针
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebygMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

myCamera camera(glm::vec3(0.0f, 5.0f, 3.0f));
float lastTime = 0.0f;
float deltaTime = 0.0f;
bool firstMouse = true;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

class HelloTriangleApplication {

public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:

	GLFWwindow* window;		//窗口

	VkInstance instance;	//vulkan实例
	VkDebugUtilsMessengerEXT debugMessenger;	//消息传递者
	VkSurfaceKHR surface;

	//Device
	std::unique_ptr<myDevice> my_device;

	//SwapChain
	std::unique_ptr<mySwapChain> my_swapChain;

	std::unique_ptr<myDescriptor> my_descriptor;
	std::unordered_map<std::string, uint32_t> uniqueDescriptorSets;

	VkRenderPass renderPass;
	VkPipelineLayout gBufferPipelineLayout;
	VkPipeline gBufferGraphicsPipeline;
	VkPipelineLayout lightPipelineLayout;
	VkPipeline lightGraphicsPipeline;

	//Buffer
	std::unique_ptr<myBuffer> my_buffer;

	std::unique_ptr<myModel> my_model;
	int verticesSize = 0;	//妈的，必须显示传size才行，封装后vertices,size()返回的大小是错误的
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	//Image
	std::unordered_map<std::string, uint32_t> uniqueMeshToAlbedoTextures;
	std::vector<std::unique_ptr<myImage>> albedoTextureImages;
	std::unordered_map<std::string, uint32_t> uniqueMeshToNormalTextures;
	std::vector<std::unique_ptr<myImage>> normalTextureImages;
	std::unique_ptr<myImage> gBufferAlbedoImage;
	std::unique_ptr<myImage> gBufferNormalImage;
	std::unique_ptr<myImage> testImage;
	std::unique_ptr<myImage> depthImage;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;

	bool framebufferResized = false;

	void initWindow() {

		glfwInit();

		//阻止GLFW自动创建OpenGL上下文
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//是否禁止改变窗口大小
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		//glfwSetFramebufferSizeCallback函数在回调时，需要为我们设置framebufferResized，但他不知道我是谁
		//所以通过对window设置我是谁，从而让回调函数知道我是谁
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
		glfwSetScrollCallback(window, scroll_callback);

	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {

		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;

	}

	void initVulkan() {

		createInstance();
		setupDebugMessenger();
		createSurface();
		createMyDevice();
		createMySwapChain();
		createMyBuffer();
		createTargetTextureResources();
		loadModel();
		createTextureImage();
		createBuffers();
		createRenderPass();
		createFramebuffers();
		createMyDescriptor();
		createGraphicsPipeline();
		createSyncObjects();

	}

	//创造一个vulkan实例需要以下几步
	//1.检查一下层是否支持
	//2.填充appInfo，就是描述当前这个实例是用于哪个应用
	//3.填充实例创建info，主要是指明实例所需的层和扩展
	//4.创建实例创建时的消息信使（主要是其他时候所需的信使需要实例来创建，而现在实例都还没有，只能单独创建一个）
	void createInstance() {

		//检测layer
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		//扩展就是Vulkan本身没有实现，但被程序员封装后的功能函数，如跨平台的各种函数，把它当成普通函数即可，别被名字唬到了
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();	//将扩展的具体信息的指针存储在该结构体中

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();	//将校验层的具体信息的指针存储在该结构体中

			//因为信使是通过实例创建出来的，所以在实例的创建和销毁过程中无法使用回调函数（毕竟实例都还没有）
			//那么我们可以对实例的创建和销毁单独设立信使
			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}
		//createInfo.enabledExtensionCount = glfwExtensionCount;
		//createInfo.ppEnabledExtensionNames = glfwExtensions;


		//VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

	}

	//检查实例所需的校验层与实际可用的校验层是否相等
	bool checkValidationLayerSupport() {

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);	//返回可用的层数
		std::vector<VkLayerProperties> availableLayers(layerCount);	//VkLayerProperties是一个结构体，记录层的名字、描述等
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {

			bool layerFound = false;
			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}

		}

		return true;
	}

	//返回实例所需的扩展
	std::vector<const char*> getRequiredExtensions() {

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);	//得到glfw所需的扩展数
		//参数1是指针起始位置，参数2是指针终止位置
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);	//这个扩展是为了打印校验层反映的错误，所以需要知道是否需要校验层
		}

		return extensions;
	}

	//Vulkan中消息的传递（回调函数）必须通过信使
	void setupDebugMessenger() {

		if (!enableValidationLayers)
			return;
		VkDebugUtilsMessengerCreateInfoEXT  createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		//通过func的构造函数给debugMessenger赋值
		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}

	}

	//消息的严重性 消息的类型 消息的具体信息
	//只有加上两个宏才能被Vulkan识别
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		//createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		//	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		//	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		//	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		//	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr;
	}

	//窗口是我们点击运行后出现的那个框，而surface则表示我们要渲染的区域，即画面
	//我们的surface不可以小于或大于窗口，surface与window实际上就是一个东西，surface是我们操作window的句柄
	//如果我们想使我们的渲染结果只占据一部分的窗口，则可以使用viewport
	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface");
		}
	}

	void createMyDevice() {
		my_device = std::make_unique<myDevice>(instance, surface);
		my_device->pickPhysicalDevice();
		my_device->createLogicalDevice(enableValidationLayers, validationLayers);
	}

	//交换链应该就是多缓冲交替呈现渲染结果的句柄吧
	void createMySwapChain() {
		my_swapChain = std::make_unique<mySwapChain>(window, surface, my_device->logicalDevice, my_device->swapChainSupportDetails, my_device->queueFamilyIndices);
	}

	void createMyBuffer() {
		my_buffer = std::make_unique<myBuffer>();
		my_buffer->createCommandPool(my_device->logicalDevice, my_device->queueFamilyIndices);
		my_buffer->createCommandBuffers(my_device->logicalDevice, MAX_FRAMES_IN_FLIGHT);
	}

	static std::vector<char> readFile(const std::string& filename) {

		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;

	}

	VkShaderModule createShaderModule(const std::vector<char>& code) {

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(my_device->logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;

	}

	void createTargetTextureResources() {
		gBufferAlbedoImage = std::make_unique<myImage>(my_device->physicalDevice, my_device->logicalDevice, my_swapChain->swapChainExtent.width, my_swapChain->swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		gBufferNormalImage = std::make_unique<myImage>(my_device->physicalDevice, my_device->logicalDevice, my_swapChain->swapChainExtent.width, my_swapChain->swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		testImage = std::make_unique<myImage>(my_device->physicalDevice, my_device->logicalDevice, my_swapChain->swapChainExtent.width, my_swapChain->swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		depthImage = std::make_unique<myImage>(my_device->physicalDevice, my_device->logicalDevice, my_swapChain->swapChainExtent.width, my_swapChain->swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, myImage::findDepthFormat(my_device->physicalDevice), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void loadModel() {

		//使用tiny
		/*
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/viking_room.obj")) {
			throw std::runtime_error(warn + err);
		}

		//for (int i = 0; i < 4000; i++) {
		//	Vertex vertex{};
		//	//attrib中的顶点的属性是一个一维数组，而每个面的顶点的索引就是数据中数据的索引（这里的索引不是用来减少顶点数的），所以需要根据index来检索
		//	vertex.pos = {
		//		attrib.vertices[0],
		//		attrib.vertices[1],
		//		attrib.vertices[2],
		//	};
		//	vertex.texCoord = {
		//		attrib.texcoords[0],
		//		1.0f - attrib.texcoords[1],
		//	};
		//	vertex.color = { 1.0f, 1.0f, 1.0f };
		//	vertices.push_back(vertex);
		//}
		//int i = 0;
		//
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {

				Vertex vertex{};
				//attrib中的顶点的属性是一个一维数组，而每个面的顶点的索引就是数据中数据的索引（这里的索引不是用来减少顶点数的），所以需要根据index来检索
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
				};
				vertex.normal = { 1.0f, 1.0f, 1.0f };

				//我们需要用index来减少顶点数
				//妈的，这里有个bug，就是说编译器编译后发现后面函数的参数是直接传整个vector的话就不让push_back了，必须将后面传参改为指针才行
				//妈的，更大的bug是vertor.size返回的大小是错的，只能用verticesSize不断的++得到vertices的size了
				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
					verticesSize++;
					//std::cout << vertices[k].pos.x << std::endl;
				}
				indices.push_back(uniqueVertices[vertex]);
			}
		}

		//std::vector<Vertex> myVertices;
		//std::cout << myVertices.size() << std::endl;
		//for (int i = 0; i < myVertices.size(); i++) {
		//	vertices.push_back(myVertices[i]);
		//}
		*/

		//使用assimp
		//verticesSize = 0;
		uint32_t index = 0;
		my_model = std::make_unique<myModel>("models/nanosuit/nanosuit.obj");	//给绝对路径读不到，给相对路径能读到
		if (my_model->meshs.size() == 0) {
			throw std::runtime_error("failed to create model!");
		}
		//将所有mesh的顶点合并
		for (uint32_t i = 0; i < my_model->meshs.size(); i++) {

			this->vertices.insert(this->vertices.end(), my_model->meshs[i].vertices.begin(), my_model->meshs[i].vertices.end());

			//因为assimp是按一个mesh一个mesh的存，所以每个indices都是相对一个mesh的，当我们将每个mesh的顶点存到一起时，indices就会出错，我们需要增加索引
			for (uint32_t j = 0; j < my_model->meshs[i].indices.size(); j++) {
				my_model->meshs[i].indices[j] += index;
			}
			index += my_model->meshs[i].vertices.size();
			//verticesSize += my_model->meshs[i].vertices.size();
			this->indices.insert(this->indices.end(), my_model->meshs[i].indices.begin(), my_model->meshs[i].indices.end());
		}

	}

	void createTextureImage() {

		for (uint32_t i = 0; i < my_model->meshs.size(); i++) {
			for (uint32_t j = 0; j < my_model->meshs[i].textures.size(); j++) {
				if (my_model->meshs[i].textures[j].type == "texture_albedo") {
					if (uniqueMeshToAlbedoTextures.count(my_model->meshs[i].textures[j].path) == 0) {
						uniqueMeshToAlbedoTextures[my_model->meshs[i].textures[j].path] = albedoTextureImages.size();
						albedoTextureImages.push_back(std::make_unique<myImage>(my_model->meshs[i].textures[j].path.c_str(), my_device->physicalDevice, my_device->logicalDevice, my_device->graphicsQueue, my_buffer->commandPool, true));
					}
				}
				else if (my_model->meshs[i].textures[j].type == "texture_normal") {
					if (uniqueMeshToNormalTextures.count(my_model->meshs[i].textures[j].path) == 0) {
						uniqueMeshToNormalTextures[my_model->meshs[i].textures[j].path] = normalTextureImages.size();
						normalTextureImages.push_back(std::make_unique<myImage>(my_model->meshs[i].textures[j].path.c_str(), my_device->physicalDevice, my_device->logicalDevice, my_device->graphicsQueue, my_buffer->commandPool, false));
					}
					
				}
			}
		}
	}

	void createBuffers() {
		//之后我们可能有很多的顶点数据，我们应该直接去拿一个大的缓冲区，然后将之分配成小的，而不是小的一个一个申请
		//vulkan规定设备的最大可申请缓冲区数>4096
		my_buffer->createVertexBuffer(my_device->physicalDevice, my_device->logicalDevice, my_device->graphicsQueue, sizeof(vertices[0]) * vertices.size(), &vertices);
		my_buffer->createIndexBuffer(my_device->physicalDevice, my_device->logicalDevice, my_device->graphicsQueue, sizeof(indices[0]), &indices);
		my_buffer->createUniformBuffers(my_device->physicalDevice, my_device->logicalDevice, MAX_FRAMES_IN_FLIGHT);
	}

	//renderPass描述了整个渲染的流程，他包括附件attachment、子渲染subpass以及子渲染之间的依赖（串并行）subpassdependency
	//我们以前延迟渲染时，需要先渲染出G-buff，然后再进行光照处理，每次G-buffer渲染完成后会调出one-shot（高速缓冲区），然后在光照处理时再调入，消耗性能
	//如果我们将G-buffer渲染和光照处理渲染都设为subpass，并指明依赖关系（前后串行），则可以保留G-buffer纹理在one-shot等待光照处理时使用
	//（我从未来来的）从后面的学习可以直到G-Buffer可以当作独立的renderPass，因为后面的光照渲染只在片元着色器中使用G-Buffer，那么可以通过barrier使得G-Buffer的片元着色器阶段与光照的顶点着色器阶段相重合，来节约时间
	//这里需要注意一下，所有的附件都是renderpass的，所以subpass只能通过附件引用来绑定附件
	//initialLayout表示该描述符对应的纹理在开始渲染前的布局，VkAttachmentReference的layout表示纹理在渲染中的布局，finalLayout则表示渲染后的布局
	void createRenderPass() {

		//延迟渲染GBUffer反射率纹理
		VkAttachmentDescription albedoAttachment{};
		albedoAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
		albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;	// msaaSamples;	延迟渲染不支持多采样，颜色可以，法线什么的不行
		//VK_ATTACHMENT_LOAD_OP_LOAD：保留附件的现有内容
		//VK_ATTACHMENT_LOAD_OP_CLEAR：在开始时将值清除为常数
		//K_ATTACHMENT_LOAD_OP_DONT_CARE：现有内容未定义；我们不关心它们
		albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;		//VK_ATTACHMENT_STORE_OP_STORE：渲染的内容将存储在内存中，稍后可以读取//VK_ATTACHMENT_STORE_OP_DONT_CARE：渲染操作后，帧缓冲区的内容将未定义
		albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	//不关心//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL：用作颜色附件的图像//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR：交换链中要呈现的图像//VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL：用作内存复制操作目标的图像
		albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//交换链中的颜色附件无法多采样，所以需要额外的附件先多采样渲染然后再交到交换链中的附件

		//延迟渲染GBUffer法线
		VkAttachmentDescription normalAttachment{};
		normalAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
		normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		normalAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//延迟渲染GBUffer深度纹理
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthImage->findDepthFormat(my_device->physicalDevice);
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//光照计算，最终颜色纹理
		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = my_swapChain->swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		std::array<VkAttachmentDescription, 4> attachments = { albedoAttachment, normalAttachment, colorAttachmentResolve, depthAttachment };

		std::array<VkAttachmentReference, 2> gBufferOutAttachmentRef;
		gBufferOutAttachmentRef[0].attachment = 0;
		gBufferOutAttachmentRef[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		gBufferOutAttachmentRef[1].attachment = 1;
		gBufferOutAttachmentRef[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		std::array<VkAttachmentReference, 3> gBufferInAttachmentRef;
		gBufferInAttachmentRef[0].attachment = 0;
		gBufferInAttachmentRef[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gBufferInAttachmentRef[1].attachment = 1;
		gBufferInAttachmentRef[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		gBufferInAttachmentRef[2].attachment = 3;
		gBufferInAttachmentRef[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 3;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//该数组中附件的索引是使用layout(location = 0) out vec4 outColor指令从片段着色器直接引用的！
		//子通道可以引用以下其他类型的附件：
		//pInputAttachments：从着色器读取的附件
		//pColorAttachments：输出数据的附件
		//pResolveAttachments：如果既有颜色附件，又有resolve附件，则颜色纹理多采样后输入resolve附件（因为交换链纹理不支持多采样，所以必须先在颜色纹理中多采样后转为单采样的resolve附件再交到交换链）
		//pDepthStencilAttachment：深度和模板数据附件
		//pPreserveAttachments：此子通道未使用但必须保留数据的附件
		VkSubpassDescription gBuffer_subpass{};
		gBuffer_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	//保留通用计算通道
		gBuffer_subpass.colorAttachmentCount = gBufferOutAttachmentRef.size();
		gBuffer_subpass.pColorAttachments = gBufferOutAttachmentRef.data();	//可以是一个数组，表示该subpass使用多个输出附件MRT
		gBuffer_subpass.pDepthStencilAttachment = &depthAttachmentRef;
		//gBuffer_subpass.pResolveAttachments = &colorAttachmentResolveRef;

		VkSubpassDescription final_subpass{};
		final_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	//保留通用计算通道
		final_subpass.inputAttachmentCount = gBufferInAttachmentRef.size();
		final_subpass.pInputAttachments = gBufferInAttachmentRef.data();
		final_subpass.colorAttachmentCount = 1;
		final_subpass.pColorAttachments = &colorAttachmentResolveRef;	//可以是一个数组，表示该subpass使用多个输出附件MRT
		//final_subpass.pDepthStencilAttachment = &depthAttachmentRef;	//不需要深度测试
		//subpass.pResolveAttachments = &colorAttachmentResolveRef;	//不需要多采样
		
		std::array<VkSubpassDescription, 2> subpass = { gBuffer_subpass, final_subpass };

		VkSubpassDependency dependency{};
		dependency.srcSubpass = 0;
		dependency.dstSubpass = 1;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//VkSubpassDependency dependency{};
		//dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		//dependency.dstSubpass = 0;
		//dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//dependency.srcAccessMask = 0;
		//dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = subpass.size();
		renderPassInfo.pSubpasses = subpass.data();
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(my_device->logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}

	}

	//这里的帧缓冲是renderpass的输出纹理
	void createFramebuffers() {
		std::vector<VkImageView> imageViews = { gBufferAlbedoImage->imageView, gBufferNormalImage->imageView};
		my_buffer->createFramebuffers(my_swapChain->swapChainImageViews.size(), my_swapChain->swapChainImageViews, my_swapChain->extent, imageViews, depthImage->imageView, renderPass, my_device->logicalDevice);
	}

	void createMyDescriptor() {

		my_descriptor = std::make_unique<myDescriptor>(my_device->logicalDevice, MAX_FRAMES_IN_FLIGHT);

		uint32_t uniformBufferNumAllLayout = 1;
		std::vector<uint32_t> textureNumAllLayout;
		std::vector<VkDescriptorType> types = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT };
		textureNumAllLayout.push_back(uniqueMeshToAlbedoTextures.size() + uniqueMeshToNormalTextures.size());
		textureNumAllLayout.push_back(3);
		my_descriptor->createDescriptorPool(uniformBufferNumAllLayout, types, textureNumAllLayout);	//这里是一共有几个，要算上所有的布局

		//创造uniformDescriptorObject
		std::vector<VkShaderStageFlagBits> usages;
		usages.push_back(VK_SHADER_STAGE_ALL_GRAPHICS);
		std::vector<std::vector<VkBuffer>> uniformBuffersAllSet = { my_buffer->uniformBuffers };
		my_descriptor->descriptorObjects.push_back(my_descriptor->createDescriptorObject(1, 0, &usages, nullptr, 1, &uniformBuffersAllSet, nullptr, nullptr));

		//创造模型textureDescriptorObject
		std::vector<std::vector<VkImageView>> textureImageViewsAllSet;
		std::vector<std::vector<VkSampler>> textureSamplersAllSet;
		std::vector<VkDescriptorType> textureDescriptorType;
		for (uint32_t j = 0; j < my_model->meshs.size(); j++) {

			std::string twoPath = "";
			for (int k = 0; k < 2; k++) {
				twoPath += my_model->meshs[j].textures[k].path;
			}

			std::vector<VkImageView> textureImageViews;
			std::vector<VkSampler> textureSamplers;
			if (uniqueDescriptorSets.count(twoPath) == 0) {

				uniqueDescriptorSets[twoPath] = uniqueDescriptorSets.size();

				int albedoTextureIndex = uniqueMeshToAlbedoTextures[my_model->meshs[j].textures[0].path];
				textureImageViews.push_back(albedoTextureImages[albedoTextureIndex]->imageView);
				textureSamplers.push_back(albedoTextureImages[albedoTextureIndex]->textureSampler);

				int normalTextureIndex = uniqueMeshToNormalTextures[my_model->meshs[j].textures[1].path];
				textureImageViews.push_back(normalTextureImages[normalTextureIndex]->imageView);
				textureSamplers.push_back(normalTextureImages[normalTextureIndex]->textureSampler);

				textureImageViewsAllSet.push_back(textureImageViews);
				textureSamplersAllSet.push_back(textureSamplers);
				textureDescriptorType.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			}

		}
		my_descriptor->descriptorObjects.push_back(my_descriptor->createDescriptorObject(0, 2, nullptr, &textureDescriptorType, uniqueDescriptorSets.size(), nullptr, &textureImageViewsAllSet, &textureSamplersAllSet));

		//创建gBufferTextureDescriptorObject
		textureImageViewsAllSet.resize(1);
		textureImageViewsAllSet[0] = {gBufferAlbedoImage->imageView, gBufferNormalImage->imageView, depthImage->imageView};
		//textureSamplersAllSet.resize(1);
		//textureSamplersAllSet[0] = { gBufferAlbedoImage->textureSampler, gBufferNormalPositionImage->textureSampler };
		textureDescriptorType = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT };
		my_descriptor->descriptorObjects.push_back(my_descriptor->createDescriptorObject(0, 3, nullptr, &textureDescriptorType, 1, nullptr, &textureImageViewsAllSet, nullptr));

	}

	//相当于是shader，与renderPass中的一个subPass对应
	void createGraphicsPipeline() {

		//gBuffer图形管线
		auto gBufferVertShaderCode = readFile("C:/Users/fangzanbo/Desktop/渲染/Vulkan/myVulkan/myVulkan/shaders/deferredShading/gBufferVert.spv");
		auto gBufferFragShaderCode = readFile("C:/Users/fangzanbo/Desktop/渲染/Vulkan/myVulkan/myVulkan/shaders/deferredShading/gBufferFrag.spv");

		VkShaderModule gBufferVertShaderModule = createShaderModule(gBufferVertShaderCode);
		VkShaderModule gBufferFragShaderModule = createShaderModule(gBufferFragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = gBufferVertShaderModule;
		vertShaderStageInfo.pName = "main";
		//允许指定着色器常量的值，比起在渲染时指定变量配置更加有效，因为可以通过编译器优化（没搞懂）
		vertShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = gBufferFragShaderModule;
		fragShaderStageInfo.pName = "main";
		fragShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		//VAO
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		//设置渲染图元方式
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		//VK_PRIMITIVE_TOPOLOGY_POINT_LIST：来自顶点的点
		//VK_PRIMITIVE_TOPOLOGY_LINE_LIST：每 2 个顶点绘制一条线，不重复使用
		//VK_PRIMITIVE_TOPOLOGY_LINE_STRIP：每条线的结束顶点用作下一条线的起始顶点
		//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST：每 3 个顶点生成一个三角形，无需重复使用
		//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP：每个三角形的第二和第三个顶点用作下一个三角形的前两个顶点
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		//设置视口
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		//viewportState.pViewports = &viewport;	//可以设置固定值，修改新值需要创建新的pipeline
		viewportState.scissorCount = 1;	//好像和坐标相关，之后测试一下
		//viewportState.pScissors = &scissor;

		//设置光栅化器，主要是深度测试等的开关、面剔除等
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;	//超出近远平面的都会被剔除
		rasterizer.rasterizerDiscardEnable = VK_FALSE; //设置为VK_TRUE，则几何图形永远不会经过光栅化阶段。这基本上会禁用对帧缓冲区的任何输出。
		//确定polygonMode如何为几何体生成片段。有以下模式可用：
		//VK_POLYGON_MODE_FILL：用碎片填充多边形的区域
		//VK_POLYGON_MODE_LINE：多边形边缘绘制为线
		//VK_POLYGON_MODE_POINT：多边形顶点绘制为点
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	//顶点序列是逆时针的是正面
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		//多采样
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		//multisampling.rasterizationSamples = msaaSamples;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;// .2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		//颜色混合
		//妈的，必须为每一个输出结果写一个颜色混合状态，因为要在这里规定是否可以写入
		std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachmentStates;
		for (int i = 0; i < 2; i++) {
			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optiona
			blendAttachmentStates[i] = colorBlendAttachment;
		}
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 2;
		colorBlending.pAttachments = blendAttachmentStates.data();
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		//一般渲染管道状态都是固定的，不能渲染循环中修改，但是某些状态可以，如视口，长宽和混合常数
		//同样通过宏来确定可动态修改的状态
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		//pipeline布局
		VkPipelineLayoutCreateInfo uniformPipelineLayoutInfo{};
		uniformPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		std::array<VkDescriptorSetLayout, 2> discriptorSetLayouts = { my_descriptor->descriptorObjects[0].discriptorLayout, my_descriptor->descriptorObjects[1].discriptorLayout };
		uniformPipelineLayoutInfo.setLayoutCount = discriptorSetLayouts.size();
		uniformPipelineLayoutInfo.pSetLayouts = discriptorSetLayouts.data();
		uniformPipelineLayoutInfo.pushConstantRangeCount = 0;
		uniformPipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(my_device->logicalDevice, &uniformPipelineLayoutInfo, nullptr, &gBufferPipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;	//顶点和片元两个着色器
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = gBufferPipelineLayout;
		pipelineInfo.renderPass = renderPass;	//先建立连接，获得索引
		pipelineInfo.subpass = 0;	//对应renderpass的哪个子部分
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;	//可以直接使用现有pipeline
		pipelineInfo.basePipelineIndex = -1;

		//可以同时创造多个pipeline
		//VkPipelineCache可以将管道缓存存储在文件中，则可以使用管道缓存来存储和重用与管道创建相关的数据
		//这些数据可在多次调用 vkCreateGraphicsPipelines甚至跨程序执行中使用，这样可以显著加快以后的管道创建速度
		if (vkCreateGraphicsPipelines(my_device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gBufferGraphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(my_device->logicalDevice, gBufferVertShaderModule, nullptr);
		vkDestroyShaderModule(my_device->logicalDevice, gBufferFragShaderModule, nullptr);

		//light图形管线
		auto lightVertShaderCode = readFile("C:/Users/fangzanbo/Desktop/渲染/Vulkan/myVulkan/myVulkan/shaders/deferredShading/lightVert.spv");
		auto lightFragShaderCode = readFile("C:/Users/fangzanbo/Desktop/渲染/Vulkan/myVulkan/myVulkan/shaders/deferredShading/lightFrag.spv");
		
		VkShaderModule lightVertShaderModule = createShaderModule(lightVertShaderCode);
		VkShaderModule lightFragShaderModule = createShaderModule(lightFragShaderCode);
		
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = lightVertShaderModule;
		vertShaderStageInfo.pName = "main";
		//允许指定着色器常量的值，比起在渲染时指定变量配置更加有效，因为可以通过编译器优化（没搞懂）
		vertShaderStageInfo.pSpecializationInfo = nullptr;
		
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = lightFragShaderModule;
		fragShaderStageInfo.pName = "main";
		fragShaderStageInfo.pSpecializationInfo = nullptr;
		
		shaderStages[0] = vertShaderStageInfo;
		shaderStages[1] = fragShaderStageInfo;
		
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		
		inputAssembly = VkPipelineInputAssemblyStateCreateInfo();
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optiona
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	//顶点序列是逆时针的是正面
		
		VkPipelineLayoutCreateInfo lightPipelineLayoutInfo{};
		lightPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		discriptorSetLayouts = { my_descriptor->descriptorObjects[0].discriptorLayout, my_descriptor->descriptorObjects[2].discriptorLayout };
		lightPipelineLayoutInfo.setLayoutCount = discriptorSetLayouts.size();
		lightPipelineLayoutInfo.pSetLayouts = discriptorSetLayouts.data();
		lightPipelineLayoutInfo.pushConstantRangeCount = 0;
		lightPipelineLayoutInfo.pPushConstantRanges = nullptr;
		if (vkCreatePipelineLayout(my_device->logicalDevice, &lightPipelineLayoutInfo, nullptr, &lightPipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
		
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.layout = lightPipelineLayout;
		pipelineInfo.subpass = 1;
		if (vkCreateGraphicsPipelines(my_device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightGraphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}
		
		vkDestroyShaderModule(my_device->logicalDevice, lightVertShaderModule, nullptr);
		vkDestroyShaderModule(my_device->logicalDevice, lightFragShaderModule, nullptr);

	}

	void createSyncObjects() {

		//信号量主要用于Queue之间的同步
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		//用于CPU和GPU之间的同步
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		//第一帧可以直接获得信号，而不会阻塞
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		//每一帧都需要一定的信号量和栏栅
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(my_device->logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(my_device->logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(my_device->logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create semaphores!");
			}
		}


	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			processInput(window);
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(my_device->logicalDevice);

	}

	void processInput(GLFWwindow* window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			camera.ProcessKeyboard(FORWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			camera.ProcessKeyboard(BACKWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.ProcessKeyboard(LEFT, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.ProcessKeyboard(RIGHT, deltaTime);
	}

	void drawFrame() {

		//第2个参数为是否等待栏栅的数量，第四个参数为是否等待所有栏栅信号化
		vkWaitForFences(my_device->logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(my_device->logicalDevice, my_swapChain->swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		//VK_ERROR_OUT_OF_DATE_KHR：交换链与表面不兼容，无法再用于渲染。通常在调整窗口大小后发生。
		//VK_SUBOPTIMAL_KHR：交换链仍可用于成功呈现到表面，但表面属性不再完全匹配。
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		updateUniformBuffer(currentFrame);

		//调用vkResetFences后栏栅不会信号化，反而会变成未信号化
		//所以这里放在调整交换链大小之后，使得只要交换链没有调到最终状态就可以一直调
		//我们运行后拉动window边框时可以发现vulkan卡住了没有渲染，就是这里的原因
		vkResetFences(my_device->logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(my_buffer->commandBuffers[currentFrame], 0);
		recordCommandBuffer(my_buffer->commandBuffers[currentFrame], imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };	//需要等待的信号量
		//我们希望等待将颜色写入图像，直到图像可用，因此我们指定写入颜色附件的图形管道阶段。这意味着理论上实现可以在图像尚未可用时开始执行我们的顶点着色器等。
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &my_buffer->commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		//只有当所有命令执行完成后才释放fence（提交后立即返回，不需要等待命令执行完成），也就是说提交的过程是临界区，需要互斥
		// 这可以保证CPU和GPU的同步
		//recordCommandBuffer函数记录渲染指令，通过vkQueueSubmit交给graphicsQueue执行
		if (vkQueueSubmit(my_device->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { my_swapChain->swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		result = vkQueuePresentKHR(my_device->presentQueue, &presentInfo);	//将渲染完成后的交换链纹理呈现到显示器上
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	}

	void updateUniformBuffer(uint32_t currentImage) {

		//static auto startTime = std::chrono::high_resolution_clock::now();
		float currentTime = static_cast<float>(glfwGetTime());;
		deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
		lastTime = currentTime;

		UniformBufferObject ubo{};
		//ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 0.4f, 0.4f));// glm::mat4(1.0f); //glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = camera.GetViewMatrix();//glm::lookAt(glm::vec3(0.0f, 15.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), my_swapChain->swapChainExtent.width / (float)my_swapChain->swapChainExtent.height, 0.1f, 100.0f);
		ubo.proj[1][1] *= -1;	//vulkan的ndc空间y轴向下，所以需要将y分量乘以-1，同时这会导致顶点顺逆时针的改变，导致面的正反发生改变

		ubo.lightPos = glm::vec4(0.0f, 130.0f, 0.0f, 0.0f); //glm::vec3(0.0f, 10.0f, 0.0f);
		ubo.cameraPos = glm::vec4(camera.Position, 0.0f);
		//std::cout << ubo.cameraPos.y << std::endl;

		memcpy(my_buffer->uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

		//标量必须按 N 对齐（= 32 位浮点数为 4 个字节）。
		//Avec2必须按 2N（ = 8 个字节）对齐
		//Avec3或vec4必须按 4N（ = 16 字节）对齐
		//嵌套结构必须按其成员的基本对齐方式对齐，并向上四舍五入为 16 的倍数。
		//矩阵mat4必须具有与之相同的对齐vec4。

	}

	void recreateSwapChain() {

		int width = 0, height = 0;
		//获得当前window的大小
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(my_device->logicalDevice);
		cleanupSwapChain();
		createMySwapChain();
		createTargetTextureResources();
		createFramebuffers();
	}

	//这个函数记录渲染的命令，并指定渲染结果所在的纹理索引
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：命令缓冲区执行一次后将立即重新记录。
		//VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT：这是一个辅助命令缓冲区，将完全位于单个渲染过程中。
		//VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT：命令缓冲区可以在等待执行的同时重新提交。
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		//只有调用vkBeginCommandBuffer函数，命令缓冲才会记录渲染命令，也只有被记录的命令才能被提交
		//注意，调用vkBeginCommandBuffer也会重置参数中的命令缓冲
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(my_swapChain->swapChainExtent.width);
		viewport.height = static_cast<float>(my_swapChain->swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = my_swapChain->swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = my_buffer->swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = my_swapChain->swapChainExtent;

		std::array<VkClearValue, 4> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		//clearValues[3].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[3].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		//VK_SUBPASS_CONTENTS_INLINE：渲染过程命令将嵌入到主命令缓冲区本身中，并且不会执行任何辅助命令缓冲区。
		//VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS：渲染过程命令将从辅助命令缓冲区执行。
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//注意，若使用vkCmdDraw，则需要对vertexBuffer设置偏移量
		//若使用vkCmdDrawIndexed，则vertexBuffer不需要偏移，只需要偏移indexBuffer，否则，会导致indices连线出错
		VkBuffer vertexBuffers[] = { my_buffer->vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, my_buffer->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferGraphicsPipeline);
		VkDescriptorSet uniformDescriptorSet = my_descriptor->descriptorObjects[0].descriptorSets[currentFrame];
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipelineLayout, 0, 1, &uniformDescriptorSet, 0, nullptr);
		uint32_t index = 0;
		for (uint32_t i = 0; i < my_model->meshs.size(); i++) {

			std::string twoPath = "";
			for (int k = 0; k < 2; k++) {
				twoPath += my_model->meshs[i].textures[k].path;
			}
			int descriptorOffset = currentFrame * uniqueDescriptorSets.size() + uniqueDescriptorSets[twoPath];
			//int descriptorOffset = uniqueDescriptorSets[twoPath];
			VkDescriptorSet textureDescriptorSet = my_descriptor->descriptorObjects[1].descriptorSets[descriptorOffset];

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gBufferPipelineLayout, 1, 1, &textureDescriptorSet, 0, nullptr);

			//vkCmdDraw(commandBuffer, static_cast<uint32_t>(my_model->meshs[i].vertices.size()), 1, 0, 0);
			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(my_model->meshs[i].indices.size()), 1, index, 0, 0);	//并不是立刻执行，就像Unity SRP里一样最后提交才执行
			index += my_model->meshs[i].indices.size();

		}

		vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightGraphicsPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipelineLayout, 0, 1, &uniformDescriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipelineLayout, 1, 1, &(my_descriptor->descriptorObjects[2].descriptorSets[currentFrame]), 0, nullptr);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		
		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}

	}

	void cleanup() {

		cleanupSwapChain();

		vkDestroyPipeline(my_device->logicalDevice, gBufferGraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(my_device->logicalDevice, gBufferPipelineLayout, nullptr);
		vkDestroyRenderPass(my_device->logicalDevice, renderPass, nullptr);

		vkDestroyDescriptorPool(my_device->logicalDevice, my_descriptor->discriptorPool, nullptr);

		for (int i = 0; i < albedoTextureImages.size(); i++) {
			albedoTextureImages[i]->clean();
		}
		for (int i = 0; i < normalTextureImages.size(); i++) {
			normalTextureImages[i]->clean();
		}

		my_descriptor->clean();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(my_device->logicalDevice, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(my_device->logicalDevice, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(my_device->logicalDevice, inFlightFences[i], nullptr);
		}
		my_buffer->clean(my_device->logicalDevice, MAX_FRAMES_IN_FLIGHT);

		my_device->clean();

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();

	}

	void cleanupSwapChain() {

		gBufferAlbedoImage->clean();
		gBufferNormalImage->clean();
		testImage->clean();
		depthImage->clean();
		//vkDestroyImageView(my_device->logicalDevice, colorImageView, nullptr);
		//vkDestroyImage(my_device->logicalDevice, colorImage, nullptr);
		//vkFreeMemory(my_device->logicalDevice, colorImageMemory, nullptr);
		//vkDestroyImageView(my_device->logicalDevice, depthImageView, nullptr);
		//vkDestroyImage(my_device->logicalDevice, depthImage, nullptr);
		//vkFreeMemory(my_device->logicalDevice, depthImageMemory, nullptr);
		for (size_t i = 0; i < my_buffer->swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(my_device->logicalDevice, my_buffer->swapChainFramebuffers[i], nullptr);
		}
		for (size_t i = 0; i < my_swapChain->swapChainImageViews.size(); i++) {
			vkDestroyImageView(my_device->logicalDevice, my_swapChain->swapChainImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(my_device->logicalDevice, my_swapChain->swapChain, nullptr);
	}

};

int main() {

	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		//system("pause");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

}

