#include <cstring>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <set>

// SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

// assimp(模型加载库)
#ifdef _MSC_VER
#include <assimp/Importer.hpp>
#else
#include <assimp/cimport.h>
#endif
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// glm(线性代数库)
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/quaternion.hpp>

#include "Log_about.h"
#include "VulkanEngine.h"
#include "Model.h"
#include "Animation.h"
#include "AssimpToGlm.h"

// 启用调试的vulkan验证层
#define VULKAN_VALIDATION_LAYER_DEBUG

int VulkanEngine::InitVulkan(SDL_Window* window)
{
	VkResult result;

	//// 创建vulkan实例
	CreateVkInstance();

	//// 创建窗口表面
	SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

	//// 选择物理设备(GPU)
	CreatePhysicalDevice();

	// 查询当前物理设备的推送常量支持的大小(当前为256字节)
	VkPhysicalDeviceProperties phy_pro;
	//VkPhysicalDeviceLimits;
	vkGetPhysicalDeviceProperties(physicalDevice, &phy_pro);
	SDL_Log(LOG_INFO("The push constant size of this physical device is:%d"), phy_pro.limits.maxPushConstantsSize);
	SDL_Log(LOG_INFO("The max memory allocation count of this physical device is:%d"), phy_pro.limits.maxMemoryAllocationCount);
	SDL_Log(LOG_INFO("The frame buffer color sample counts of this physical device is:%d"), phy_pro.limits.framebufferColorSampleCounts);
	SDL_Log(LOG_INFO("The frame buffer depth sample counts of this physical device is:%d"), phy_pro.limits.framebufferDepthSampleCounts);
	SDL_Log(LOG_INFO("The storage buffer size of this physical device is:%d"), phy_pro.limits.maxStorageBufferRange);
	SDL_Log(LOG_INFO("The uniform buffer size of this physical device is:%d"), phy_pro.limits.maxUniformBufferRange);
	SDL_Log(LOG_INFO("The max compute work group count of this physical device is:%d"), phy_pro.limits.maxComputeWorkGroupCount[0]);
	SDL_Log(LOG_INFO("The max compute work group invocations of this physical device is:%d"), phy_pro.limits.maxComputeWorkGroupInvocations);
	SDL_Log(LOG_INFO("The max compute work group size of this physical device is:%d"), phy_pro.limits.maxComputeWorkGroupSize[0]);
	SDL_Log("");
	
	//// 获取需要的队列信息
	CreateQueue();

	//// 创建逻辑设备
	CreateLogicalDevice();

	/*if (CreateLogicalDevice(Queue_Component))
	{
		SDL_Log(LOG_ERROR("Failed to Create Logical Device!"));
		return 1;
	}*/

	//// 创建队列句柄
	vkGetDeviceQueue(device, graphics_queue_index, 0, &graphicsqueue);
	vkGetDeviceQueue(device, present_queue_index, 0, &presentqueue);

	//// 创建交换链
	CreateSwapchain(window);

	//// 创建图像视图
	CreateSwapchainImageAndViews();
	CreateDepthResources();

	//// 创建图形管线
	CreateDescriptorSetLayout();
	//CreateRenderPass(swapchain_format, &render_pass);

	// 通用渲染管线
	//CreatePipelineLayout(&layout);
	//CreatePipeline(layout, render_pass, &pipeline);
	// 骨骼模型渲染管线
	//CreateBoneAnimModelPipelineLayout(&bone_model_pipeline_layout);
	//CreateBoneAnimModelPipeline(bone_model_pipeline_layout, render_pass, &bone_model_pipeline);

	//// 创建帧缓冲
	//CreateFrameBuffer();

	//// 创建命令池和命令缓冲区
	CreateCmdPool();
	CreateCmdBuffer();

	// 创建纹理采样器
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	CreateTextureSampler(properties.limits.maxSamplerAnisotropy, &texture_sampler);
	// 加载obj文件的顶点、索引数据

	// 创建描述符池
	CreateDescriptorPool(&descriptor_pool);
	// 创建描述符
	/*CreateDescriptorSets(descriptor_set_layout, descriptor_pool,
		uniform_buf, texture.view, texture_sampler,
		&descriptor_set);*/

	//// 创建同步对象
	CreateSyncObjects();

	return 0;
}

void VulkanEngine::WaitPrevFrame(uint8_t frame_index)
{
	// 等待前一帧
	vkWaitForFences(device, 1, &in_flight_fences[frame_index], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &in_flight_fences[frame_index]);
}

void VulkanEngine::Destroy()
{
	// 等待设备的任务全都结束再进行释放
	//vkDeviceWaitIdle(device);

	// 销毁同步信号
	for (int i = 0; i < 2; i++)
	{
		vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
		vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
		vkDestroyFence(device, in_flight_fences[i], nullptr);
	}

	// 销毁命令池和命令缓冲区
	vkDestroyCommandPool(device, cmd_pool, nullptr);
	// 销毁帧缓冲区
	for (auto framebuf: swapchain_frame_buffers)
	{
		vkDestroyFramebuffer(device, framebuf, nullptr);
	}
	// 销毁缓冲区
	/*vkDestroyBuffer(device, uniform_buf, nullptr);
	vkFreeMemory(device, uniform_buf_memory, nullptr);*/
	// 销毁深度资源
	vkDestroyImageView(device, depth_imgview, nullptr);
	vkDestroyImage(device, depth_image, nullptr);
	vkFreeMemory(device, depth_image_mem, nullptr);
	// 销毁纹理采样器
	vkDestroySampler(device, texture_sampler, nullptr);
	// 销毁图像管线
	//vkDestroyPipeline(device, bone_model_pipeline, nullptr);
	//vkDestroyPipeline(device, pipeline, nullptr);
	//vkDestroyPipelineLayout(device, bone_model_pipeline_layout, nullptr);
	//vkDestroyPipelineLayout(device, layout, nullptr);
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(device, one_matrix_descriptor_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, three_texture_sampler_descriptor_layout, nullptr);
	//vkDestroyRenderPass(device, render_pass, nullptr);
	// 销毁交换链图像视图
	for (auto image_view : swapchain_imageviews)
	{
		vkDestroyImageView(device, image_view, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

int VulkanEngine::CreateVkInstance()
{
	VkApplicationInfo appinfo{};
	VkInstanceCreateInfo createinfo{};
	uint32_t SDLExtensioncount = 0;
	const char* const* SDLExtensions;
	VkResult result;

	// 应用程序参数
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pNext = nullptr;
	appinfo.pApplicationName = "Hello Triangle";
	appinfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
	appinfo.pEngineName = "No Engine";
	appinfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
	appinfo.apiVersion = VK_API_VERSION_1_1;

	// 创建实例的参数
	SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensioncount);
	createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createinfo.pApplicationInfo = &appinfo;
	createinfo.enabledExtensionCount = SDLExtensioncount;
	createinfo.ppEnabledExtensionNames = SDLExtensions;

#ifdef VULKAN_VALIDATION_LAYER_DEBUG
	const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
	createinfo.enabledLayerCount = 1;
	createinfo.ppEnabledLayerNames = validationLayers;
#endif // VULKAN_VALIDATION_LAYER_DEBUG

	// 创建实例
	result = vkCreateInstance(&createinfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Vulkan Instance Create Failed!(Error code: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created vulkan instance!"));

	return 0;
}

int VulkanEngine::CreatePhysicalDevice()
{
	uint32_t devicecount = 0;
	VkResult result;

	// 获取可用的GPU的数量
	vkEnumeratePhysicalDevices(instance, &devicecount, nullptr);
	//SDL_Log(LOG_INFO("There are %d GPU can be used"), devicecount);

	// 如果为0则没有任何意义继续下一步
	if (!devicecount)
	{
		SDL_Log(LOG_ERROR("Failed to Find GPUs for vulkan"));
		return 1;
	}

	// 根据得到的数量创建数组
	std::vector<VkPhysicalDevice> physicaldevices(devicecount);
	// 并重新调用以获取所有可用的GPU存在数组中
	result = vkEnumeratePhysicalDevices(instance, &devicecount, physicaldevices.data());
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Get Physical Device! (Error code: %d)"), result);
		return 1;
	}

	// 因为只有一个GPU所以直接使用第一个
	physicalDevice = physicaldevices.front();

	return 0;
}

int VulkanEngine::CreateQueue()
{
	graphics_queue_index = FindQueueFamiles(VK_QUEUE_GRAPHICS_BIT);
	present_queue_index = FindSurfaceSupportQueueFamily(surface);

	return 0;
}

int VulkanEngine::CreateLogicalDevice()
{
	VkDeviceCreateInfo createinfo = {};
	VkPhysicalDeviceFeatures devicefeature = {};
	// 逻辑设备扩展列表
	const std::vector<const char*> logical_device_extension = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,         // 交换链扩展
	};
	VkResult result;

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};
	std::set<uint32_t> queue_familes_index = {
		graphics_queue_index,
		present_queue_index
	};
	float priority = 1.f;
	for (uint32_t index : queue_familes_index)
	{
		VkDeviceQueueCreateInfo queuecreateinfo{};
		queuecreateinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queuecreateinfo.queueFamilyIndex = index;
		queuecreateinfo.queueCount = 1;
		queuecreateinfo.pQueuePriorities = &priority;
		queue_create_infos.push_back(queuecreateinfo);
	}

	// 开启设备的各向异性过滤
	devicefeature.samplerAnisotropy = VK_TRUE;
	// 逻辑设备创建的参数
	createinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createinfo.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	createinfo.pQueueCreateInfos = queue_create_infos.data();
	createinfo.enabledExtensionCount = static_cast<uint32_t>(logical_device_extension.size());
	createinfo.ppEnabledExtensionNames = logical_device_extension.data();
	createinfo.pEnabledFeatures = &devicefeature;

	// 创建逻辑设备
	result = vkCreateDevice(physicalDevice, &createinfo, nullptr, &device);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Logical Device Create Failed!(Error code: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created Logical Device!"));
	return 0;
}

int VulkanEngine::CreateSwapchain(SDL_Window* window)
{
	// 基本表面功能（交换链中图像的最小/最大数量，图像的最小/最大宽度和高度）
	VkSurfaceCapabilitiesKHR capabilities;
	// 表面格式（像素格式，颜色空间）
	std::vector<VkSurfaceFormatKHR> formats;
	// 可用的演示模式
	std::vector<VkPresentModeKHR> present_modes;
	//// 先查询查询一个物理设备的交换链的详细信息(
	//// VkSurfaceCapabilitiesKHR - 基本表面功能（交换链中图像的最小 / 最大数量，图像的最小 / 最大宽度和高度）
	//// VkSurfaceFormatKHR - 表面格式（像素格式，颜色空间）
	//// VkPresentModeKHR - 可用的演示模式
	//// )
	uint32_t count = 0;

	// 查询基本表面功能（交换链中图像的最小/最大数量，图像的最小/最大宽度和高度）
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

	// 查询支持的表面格式
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
	if (!count)
	{
		return 1;
	}
	formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, formats.data());

	// 查询支持的呈现模式
	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);
	if (!count)
	{
		return 1;
	}
	present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, present_modes.data());

	// 检查是否支持交换链
	if (formats.empty() || present_modes.empty())
	{
		SDL_Log(LOG_ERROR("This physical device don't support swapchain!"));
		return 1;
	}

	//// 选择合适的的功能，格式，演示模式
	VkSurfaceFormatKHR use_format = formats[0]; // 若没找到那就返回第一个格式
	VkExtent2D use_extent;
	VkPresentModeKHR use_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	// 若没有合适的则使用"队列模式"(VK_PRESENT_MODE_FIFO_KHR)
	// VK_PRESENT_MODE_FIFO_KHR：交换链是一个队列，显示器会在刷新时
	//     从队列的前面获取图像，程序会将渲染的图像插入队列的后面。如果队列
	//     已满，则程序必须等待。这与现代游戏中发现的垂直同步最相似。显示器刷新的时刻称为“垂直消隐”。

	// 遍历查询到的表面格式
	for (const auto& available_format : formats)
	{
		// 寻找sRGBA的格式(format)和支持sRGB的色彩空间(colorSpace)
		if ((available_format.format == VK_FORMAT_B8G8R8A8_SRGB) && (available_format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR))
		{
			use_format = available_format;
		}
	}

	// 正常情况下，直接使用查询到的VkSurfaceCapabilitiesKHR的当前范围即可
	// 但若某些原因改变了窗口的分辨率，并把capabilities.currentExtent.width设置为了uint32_t的最大值，则自行设置交换范围的宽高，即if后的代码
	int w, h;
	if (capabilities.currentExtent.width != 0xFFFFFFFFu)  // uint32_t的最大值
	{
		use_extent = capabilities.currentExtent;
	}

	SDL_GetWindowSizeInPixels(window, &w, &h);
	// std::clamp函数用于限制宽高在表面基本功能(VkSurfaceCapabilitiesKHR)中的图像范围最小最大值中
	// std::clamp(若给定的值小于最小值则返回最小值，大于最大值则返回最大值，否则返回原值)
	use_extent = {
		static_cast<uint32_t>(std::clamp<int>(w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width)),
		static_cast<uint32_t>(std::clamp<int>(h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height))
	};

	// 遍历查询到的呈现模式
	for (const auto& available_present_mode : present_modes)
	{
		// 选择并返回"三重缓冲模式"(VK_PRESENT_MODE_MAILBOX_KHR)
		// VK_PRESENT_MODE_MAILBOX_KHR：这是第二种模式的另一种变体。当队列已满
		//     时，不是阻塞应用程序，而是简单地将已排队的图像替换为较新的图像。 此模式
		//     可用于尽可能快地渲染帧，同时仍避免撕裂，从而导致比标准垂直同步更少的延迟
		//     问题。这通常被称为“三重缓冲”，尽管仅存在三个缓冲区并不一定意味着帧率已解锁。
		if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			use_present_mode = available_present_mode;
		}
	}


	//// 创建交换链
	VkSwapchainCreateInfoKHR create_info{};
	//VkSurfaceFormatKHR sur_format = use_format;
	std::set<uint32_t> queue_indics = {graphics_queue_index, present_queue_index};
	VkResult result;

	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = capabilities.minImageCount;
	create_info.imageFormat = use_format.format;
	create_info.imageColorSpace = use_format.colorSpace;
	create_info.imageExtent = use_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (queue_indics.size() > 1)
	{
		std::vector<uint32_t> indexs;
		for (const auto& queue_index : queue_indics)
		{
			indexs.push_back(queue_index);
		}
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = static_cast<uint32_t>(indexs.size());
		create_info.pQueueFamilyIndices = indexs.data();
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = use_present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	swapchain_format = create_info.imageFormat;
	swapchain_extent = create_info.imageExtent;

	result = vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Swapchain! (Error code: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created Swapchain!"));
	return 0;
}

int VulkanEngine::CreateSwapchainImageAndViews()
{
	uint32_t image_count = 0;

	vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
	if (!image_count)
	{
		SDL_Log(LOG_ERROR("Don't have available Image!"));
		return 1;
	}
	swapchain_images.resize(image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

	swapchain_imageviews.resize(swapchain_images.size());
	for (int i = 0; i < swapchain_images.size(); i++)
	{
		if (CreateImageView(swapchain_images[i], swapchain_format, 1, VK_IMAGE_ASPECT_COLOR_BIT, &swapchain_imageviews[i])) { return 1; }
	}

	SDL_Log(LOG_SUCCESS("Successfully created Swapchain Image and Imageview!"));
	return 0;
}

int VulkanEngine::CreateDepthResources()
{
	VkFormat depth_format;
	//VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

	depth_format = FindDepthFormat();

	if (CreateImage(swapchain_extent.width, swapchain_extent.height, 1,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&depth_image, &depth_image_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create depth image!"));
		return 1;
	}
	CreateImageView(depth_image, depth_format, 1, VK_IMAGE_ASPECT_DEPTH_BIT, &depth_imgview);

	// cmd_component.TransitionImagelayout(device, queue(graphicsqueue), *depth_img_ptr, depth_format, oldlayout(VK_IMAGE_LAYOUT_UNDEFINED), newlayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

	SDL_Log(LOG_SUCCESS("Successfully created Depth resources!"));
	return 0;
}

int VulkanEngine::CreateSwapchainFrameBuffer(VkRenderPass render_pass)
{
	VkFramebufferCreateInfo frame_buf_info;

	swapchain_frame_buffers.resize(swapchain_imageviews.size());
	for (size_t i = 0; i < swapchain_frame_buffers.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapchain_imageviews[i], depth_imgview };
		frame_buf_info = {};
		frame_buf_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buf_info.renderPass = render_pass;
		frame_buf_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		frame_buf_info.pAttachments = attachments.data();
		frame_buf_info.width = swapchain_extent.width;
		frame_buf_info.height = swapchain_extent.height;
		frame_buf_info.layers = 1;

		VkResult result = vkCreateFramebuffer(device, &frame_buf_info, nullptr, &swapchain_frame_buffers[i]);
		if (result != VK_SUCCESS)
		{
			SDL_Log(LOG_ERROR("Failed to Create Framebuffer! (Error code: %d)"), result);
			return 1;
		}
	}

	SDL_Log(LOG_SUCCESS("Successfully created Frame Buffer!"));
	return 0;
}

int VulkanEngine::Create1x1PixelTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, VkImage* texture_img_ptr, VkImageView* texture_view_ptr, VkDeviceMemory* texture_memory_ptr) const
{
	uint8_t pixels[4] = {r, g, b, a};
	//uint32_t int_pixels = 0xffffffff;
	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;

	if (CreateImage(1, 1,
		1,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture_img_ptr, texture_memory_ptr))
	{
		SDL_Log(LOG_ERROR("Failed to create white module image for texture!"));
		return 1;
	}

	if (CreateBuffer(
		4,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf, &staging_buf_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create staging buffer!"));
		return 1;
	}

	void* data;
	vkMapMemory(device, staging_buf_mem, 0, 4, 0, &data);
	memcpy(data, (void*)pixels, 4);
	vkUnmapMemory(device, staging_buf_mem);

	TransitionImagelayout(graphicsqueue, *texture_img_ptr, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(graphicsqueue, staging_buf, *texture_img_ptr, 1, 1);
	TransitionImagelayout(graphicsqueue, *texture_img_ptr, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device, staging_buf, nullptr);
	vkFreeMemory(device, staging_buf_mem, nullptr);

	if (CreateImageView(*texture_img_ptr, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_ASPECT_COLOR_BIT, texture_view_ptr))
	{
		SDL_Log(LOG_ERROR("Failed to create white module texture image view!"));
		return 1;
	}

	return 0;
}

int VulkanEngine::GetQueueFamiles(std::vector<VkQueueFamilyProperties>& queue_pro)
{
	uint32_t queue_family_count = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, nullptr);

	// 若队列簇数量为0，则无需进行下一步
	if (!queue_family_count)
	{
		SDL_Log(LOG_ERROR("Queue Familes Count of this Physical Device is 0!"));
		return 1;
	}
	queue_pro.resize((size_t)queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, queue_pro.data());

	return 0;
}

uint32_t VulkanEngine::FindQueueFamiles(VkQueueFlagBits queue_flag)
{
	uint32_t index = 0;
	std::vector<VkQueueFamilyProperties> queue_pro;

	GetQueueFamiles(queue_pro);

	for (const auto& temp : queue_pro)
	{
		if (temp.queueFlags & queue_flag)
		{
			return index;
		}
		index++;
	}

	return -1;
}

uint32_t VulkanEngine::FindSurfaceSupportQueueFamily(VkSurfaceKHR surface)
{
	VkBool32 presentsupoort = false;
	VkResult result;
	uint32_t i;
	std::vector<VkQueueFamilyProperties> queue_pro;

	GetQueueFamiles(queue_pro);

	for (i = 0; i < queue_pro.size(); i++)
	{
		result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentsupoort);
		if (result != VK_SUCCESS)
		{
			SDL_Log(LOG_ERROR("Failed to Get Queue Family that Support present Surface! (Error: %d)"), result);
			return -1;
		}
		if (presentsupoort)
		{
			return i;
		}
	}

	return -1;
}

int VulkanEngine::CreateShaderModule(const char* shader_file, VkShaderModule* shader_module_ptr)
{
	std::ifstream file(shader_file, std::ios::ate | std::ios::binary);
	std::vector<char> code_buf;
	size_t file_size;
	VkResult result;

	if (!file.is_open())
	{
		SDL_Log(LOG_ERROR("Failed to open shader file!"));
		return 1;
	}

	file_size = (size_t)file.tellg();
	code_buf.resize(file_size);
	file.seekg(0);

	file.read(code_buf.data(), file_size);

	file.close();

	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = file_size;
	create_info.pCode = reinterpret_cast<uint32_t*>(code_buf.data());
	result = vkCreateShaderModule(device, &create_info, nullptr, shader_module_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Shader Module!"));
		return 1;
	}

	return 0;
}

int VulkanEngine::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding bindings[4]{};
	VkDescriptorSetLayoutCreateInfo layout_info{};
	VkResult result;

	// (set=?, binding=0) ubo MVPmatrix
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &one_matrix_descriptor_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descriptor layout of one uniform buffer!"));

	// (set=?, binding=0) textures array
	// [0] diffuse
	// [1] specular
	// [2] ambient
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 3;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	// (set=1, binding=1) specular texture
	//bindings[1].binding = 1;
	//bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//bindings[1].descriptorCount = 1;
	//bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	//bindings[1].pImmutableSamplers = nullptr;

	//// (set=1, binding=2) ambient texture
	//bindings[2].binding = 2;
	//bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//bindings[2].descriptorCount = 1;
	//bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	//bindings[2].pImmutableSamplers = nullptr;

	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &three_texture_sampler_descriptor_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descriptor layout of three texture!"));

	return 0;
}

int VulkanEngine::CreateDescriptorPool(VkDescriptorPool* pool_ptr)
{
	VkDescriptorPoolSize pool_sizes[3]{};
	VkDescriptorPoolCreateInfo create_info{};
	VkResult result;

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 2;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_sizes[1].descriptorCount = 8;
	pool_sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[2].descriptorCount = 384;

	create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	create_info.poolSizeCount = 3;
	create_info.pPoolSizes = pool_sizes;
	create_info.maxSets = 384 + 8 + 2;

	result = vkCreateDescriptorPool(device, &create_info, nullptr, pool_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create descriptor pool!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descriptor set pool!"));

	return 0;
}

int VulkanEngine::CreateDescriptorSets(VkDescriptorSetLayout set_layout, VkDescriptorPool descriptor_pool, VkDescriptorSet* descriptor_set_ptr) const
{
	VkDescriptorSetAllocateInfo alloc_info{};
	VkResult result;

	VkDescriptorSetLayout set_layouts[] = { set_layout };
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = set_layouts;

	result = vkAllocateDescriptorSets(device, &alloc_info, descriptor_set_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to allocate descriptor set!(Error: %d)"), result);
		return 1;
	}
	//SDL_Log(LOG_SUCCESS("Successfully created Descriptor set!"));

	return 0;
}

int VulkanEngine::CreatePipelineLayout(VkPipelineLayout* layout_ptr)
{
	VkPushConstantRange push_ranges[1]{};
	push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[0].offset = 0;
	push_ranges[0].size = sizeof(glm::mat4) * 2;
	/*push_ranges[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[1].offset = sizeof(glm::mat4);
	push_ranges[1].size = sizeof(glm::mat4);*/
	/*push_ranges[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[2].offset = sizeof(glm::mat4) * 2;
	push_ranges[2].size = sizeof(glm::mat4);*/

	VkDescriptorSetLayout des_layouts[] = { one_matrix_descriptor_layout , three_texture_sampler_descriptor_layout };
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 2;
	layout_info.pSetLayouts = des_layouts;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = push_ranges;

	VkResult result = vkCreatePipelineLayout(device, &layout_info, nullptr, layout_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create pipeline layout!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Pipeline layout for general pipeline!"));

	return 0;
}

int VulkanEngine::CreateBoneAnimModelPipelineLayout(VkPipelineLayout* layout_ptr)
{
	VkPushConstantRange push_ranges[1]{};
	push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[0].offset = 0;
	push_ranges[0].size = sizeof(glm::mat4);

	VkDescriptorSetLayout des_layouts[] = { one_matrix_descriptor_layout, three_texture_sampler_descriptor_layout, one_matrix_descriptor_layout };
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 3;
	layout_info.pSetLayouts = des_layouts;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = push_ranges;

	VkResult result = vkCreatePipelineLayout(device, &layout_info, nullptr, layout_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create pipeline layout!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Pipeline layout for Bone anim pipeline!"));

	return 0;
}

int VulkanEngine::CreateRenderPass(VkFormat format, VkRenderPass* render_pass_ptr)
{
	VkAttachmentDescription color_attachment{};
	color_attachment.format = format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = FindDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// 附件引用
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref{};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	/*dependency[1].srcSubpass = 0;
	dependency[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependency[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;*/

	std::array<VkAttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
	VkRenderPassCreateInfo renderpass_info{};
	renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderpass_info.pAttachments = attachments.data();
	renderpass_info.subpassCount = 1;
	renderpass_info.pSubpasses = &subpass;
	renderpass_info.dependencyCount = 1;
	renderpass_info.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(device, &renderpass_info, nullptr, render_pass_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Render Pass!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Render Pass!"));

	return 0;
}

int VulkanEngine::CreateTextureSampler(float max_anisotropy, VkSampler* smapler_ptr)
{
	VkSamplerCreateInfo sampler_info{};
	VkResult result;

	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = max_anisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.f;
	sampler_info.minLod = 0.f;
	sampler_info.maxLod = VK_LOD_CLAMP_NONE;

	result = vkCreateSampler(device, &sampler_info, nullptr, smapler_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create sampler!(Error: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created texture sampler!"));
	return 0;
}

int VulkanEngine::CreateCmdPool()
{
	VkCommandPoolCreateInfo create_info{};
	VkResult result;

	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	create_info.queueFamilyIndex = graphics_queue_index;

	result = vkCreateCommandPool(device, &create_info, nullptr, &cmd_pool);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Command Pool! (Error code: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created Command Pool!"));
	return 0;
}

int VulkanEngine::CreateCmdBuffer()
{
	VkCommandBufferAllocateInfo alloc_info{};
	VkResult result;

	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = cmd_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 2;

	result = vkAllocateCommandBuffers(device, &alloc_info, cmd_bufs);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Command Buffer!(Error: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created Command buffer!"));
	return 0;
}

VkFormat VulkanEngine::FindDepthFormat()
{
	VkFormatProperties props;
	VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

	for (const auto& format : { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT })
	{
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
		if ((props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	SDL_Log(LOG_ERROR("Can't find support format! Will use VK_FORMAT_UNDEFINED!"));
	return VK_FORMAT_UNDEFINED;
}

int VulkanEngine::CreateImage(uint32_t width, uint32_t height, uint32_t mip_levels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image_ptr, VkDeviceMemory* image_mem_ptr) const
{
	VkImageCreateInfo create_info{};
	VkMemoryRequirements mem_require;
	VkPhysicalDeviceMemoryProperties mem_properties;
	VkMemoryAllocateInfo alloc_info{};
	VkResult result;

	uint32_t type_idx;

	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_info.imageType = VK_IMAGE_TYPE_2D;
	create_info.extent.width = width;
	create_info.extent.height = height;
	create_info.extent.depth = 1;
	create_info.mipLevels = mip_levels;
	create_info.arrayLayers = 1;
	create_info.format = format;//VK_FORMAT_R8G8B8A8_SRGB;
	create_info.tiling = tiling;//VK_IMAGE_TILING_OPTIMAL;
	create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	create_info.usage = usage;//VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	create_info.flags = 0;

	result = vkCreateImage(device, &create_info, nullptr, image_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create image!(Error: %d)"), result);
		return 1;
	}

	vkGetImageMemoryRequirements(device, *image_ptr, &mem_require);

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem_properties);
	for (type_idx = 0; type_idx < mem_properties.memoryTypeCount; type_idx++)
	{
		if (mem_require.memoryTypeBits & (1 << type_idx) &&
			(mem_properties.memoryTypes[type_idx].propertyFlags & properties) == properties)
		{
			break;
		}
	}
	if (type_idx == mem_properties.memoryTypeCount)
	{
		SDL_Log(LOG_ERROR("Failed to find suitable memory type!"));
		return 1;
	}

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_require.size;
	alloc_info.memoryTypeIndex = type_idx;
	result = vkAllocateMemory(device, &alloc_info, nullptr, image_mem_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to allocate image memory!(Error: %d)"), result);
		return 1;
	}

	vkBindImageMemory(device, *image_ptr, *image_mem_ptr, 0);

	return 0;
}

int VulkanEngine::CreateImageView(VkImage image, VkFormat format, uint32_t mip_levels, VkImageAspectFlags aspectflags, VkImageView* image_view_ptr) const
{
	VkImageViewCreateInfo create_info{};
	VkResult result;

	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;

	/*       VK_COMPONENT_SWIZZLE_IDENTITY == 0
	create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	*/

	create_info.subresourceRange.aspectMask = aspectflags;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = mip_levels;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	result = vkCreateImageView(device, &create_info, nullptr, image_view_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Iamge View! (Error code:%d)"), result);
		return 1;
	}

	return 0;
}

int VulkanEngine::CreateModelTexture(const char* texture_file, ModelTexture& texture) const
{
	// 创建纹理图像
	if (CreateTextureImage(texture_file, &texture.img, &texture.memory, &texture.miplevels))
	{
		SDL_Log(LOG_ERROR("Failed to create texture image!"));
		return 1;
	}
	if (CreateTextureImageView(texture.img, VK_FORMAT_R8G8B8A8_SRGB, texture.miplevels, &texture.view))
	{
		SDL_Log(LOG_ERROR("Failed to create texture image view!"));
		return 1;
	}

	return 0;
}

void VulkanEngine::DestroyModelTexture(ModelTexture& texture) const
{
	vkDestroyImageView(device, texture.view, nullptr);
	vkDestroyImage(device, texture.img, nullptr);
	vkFreeMemory(device, texture.memory, nullptr);
}

int VulkanEngine::CreateTextureImage(const char* file, VkImage* image_ptr, VkDeviceMemory* image_mem_ptr, uint32_t* mip_levels_ptr) const
{
	SDL_Surface* img_surface;
	SDL_Surface* temp_surface;

	// 若原图像格式不为RGBA32，需转换成新的RGBA32格式
	temp_surface = IMG_Load(file);
	if (temp_surface->format != SDL_PIXELFORMAT_RGBA32)
	{
		img_surface = SDL_ConvertSurface(temp_surface, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(temp_surface);
		// SDL_Log("format: 0x%x", img_surface->format);
	}
	else
	{
		img_surface = temp_surface;
	}
	*mip_levels_ptr = static_cast<uint32_t>(std::floor(std::log2(std::min(img_surface->w, img_surface->h)))) + 1;

	if (CreateTextureImage(img_surface, image_ptr, image_mem_ptr, mip_levels_ptr))
	{
		SDL_DestroySurface(img_surface);
		return 1;
	}

	// 释放图像表面
	SDL_DestroySurface(img_surface);

	return 0;
}

int VulkanEngine::CreateTextureImage(SDL_Surface* surface, VkImage* image_ptr, VkDeviceMemory* image_mem_ptr, uint32_t* mip_levels_ptr) const
{
	VkDeviceSize image_size;
	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;

	VkFormatProperties format_pro;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &format_pro);

	image_size = (uint64_t)surface->w * (uint64_t)surface->h * 4;
	if (CreateImage(surface->w, surface->h,
		*mip_levels_ptr,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		image_ptr, image_mem_ptr))
	{
		SDL_Log(LOG_ERROR("Failed to create image for texture!"));
		return 1;
	}

	if (CreateBuffer(
		image_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf, &staging_buf_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create staging buffer!"));
		return 1;
	}

	void* data;
	vkMapMemory(device, staging_buf_mem, 0, image_size, 0, &data);
	memcpy(data, surface->pixels, (size_t)image_size);
	vkUnmapMemory(device, staging_buf_mem);

	TransitionImagelayout(graphicsqueue, *image_ptr, VK_FORMAT_R8G8B8A8_SRGB, *mip_levels_ptr, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(graphicsqueue, staging_buf, *image_ptr, surface->w, surface->h);
	if (format_pro.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
	{
		GenerateMipmaps(graphicsqueue, *image_ptr, surface->w, surface->h, *mip_levels_ptr);
	}
	else
	{
		TransitionImagelayout(graphicsqueue, *image_ptr, VK_FORMAT_R8G8B8A8_SRGB, *mip_levels_ptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	// 释放和销毁暂存缓冲区
	vkDestroyBuffer(device, staging_buf, nullptr);
	vkFreeMemory(device, staging_buf_mem, nullptr);

	return 0;
}

int VulkanEngine::CreateTextureImageView(VkImage texture_image, VkFormat format, uint32_t mip_levels, VkImageView* texture_imageview_ptr) const
{
	if (CreateImageView(texture_image, format, mip_levels, VK_IMAGE_ASPECT_COLOR_BIT, texture_imageview_ptr))
	{
		SDL_Log(LOG_ERROR("Failed to create texture image view!"));
		return 1;
	}

	return 0;
}

void VulkanEngine::GenerateMipmaps(VkQueue trans_queue, VkImage img, int32_t texwidth, int32_t texheight, uint32_t mip_levels) const
{
	VkCommandBuffer staging_cmd = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = img;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	for (uint32_t i = 1; i < mip_levels; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(
			staging_cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr,
			0, nullptr,
			1, &barrier
		);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { texwidth, texheight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.layerCount = 1;
		blit.srcSubresource.baseArrayLayer = 0;
		texwidth = texwidth / 2;
		texheight = texheight / 2;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { texwidth, texheight, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.layerCount = 1;
		blit.dstSubresource.baseArrayLayer = 0;
		vkCmdBlitImage(staging_cmd,
			img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_LINEAR
		);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(
			staging_cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}
	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(
		staging_cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, nullptr,
		0, nullptr,
		1, &barrier
	);

	EndAndSubmitSingleTimeCommands(trans_queue, staging_cmd);
}

int VulkanEngine::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer_ptr, VkDeviceMemory* memory_ptr) const
{
	VkBufferCreateInfo buf_info{};
	VkMemoryRequirements mem_require;
	VkPhysicalDeviceMemoryProperties mem_properties;
	VkMemoryAllocateInfo alloc_info{};

	VkResult result;
	uint32_t type_idx;

	buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_info.size = size;
	buf_info.usage = usage;
	buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	result = vkCreateBuffer(device, &buf_info, nullptr, buffer_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create Buffer!(Error: %d)"), result);
		return 1;
	}

	vkGetBufferMemoryRequirements(device, *buffer_ptr, &mem_require);

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem_properties);
	for (type_idx = 0; type_idx < mem_properties.memoryTypeCount; type_idx++)
	{
		if (mem_require.memoryTypeBits & (1 << type_idx) &&
			(mem_properties.memoryTypes[type_idx].propertyFlags & properties) == properties)
		{
			break;
		}
	}
	if (type_idx == mem_properties.memoryTypeCount)
	{
		SDL_Log(LOG_ERROR("Failed to find suitable memory type!"));
		return 1;
	}

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_require.size;
	alloc_info.memoryTypeIndex = type_idx;
	result = vkAllocateMemory(device, &alloc_info, nullptr, memory_ptr);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to allocate buffer memory!(Error: %d)"), result);
		return 1;
	}

	vkBindBufferMemory(device, *buffer_ptr, *memory_ptr, 0);

	return 0;
}

void VulkanEngine::DestroyBuffer(VkBuffer buf, VkDeviceMemory memory) const
{
	vkDestroyBuffer(device, buf, nullptr);
	vkFreeMemory(device, memory, nullptr);
}

VkCommandBuffer VulkanEngine::BeginSingleTimeCommands() const
{
	VkCommandBufferAllocateInfo alloc_info{};
	VkCommandBuffer staging_cmd_buf;

	// 创建一个临时命令缓冲区，用于提交缓冲区复制的命令
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = cmd_pool;
	alloc_info.commandBufferCount = 1;
	vkAllocateCommandBuffers(device, &alloc_info, &staging_cmd_buf);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 只使用一次命令缓冲区
	vkBeginCommandBuffer(staging_cmd_buf, &begin_info);

	return staging_cmd_buf;
}

void VulkanEngine::EndAndSubmitSingleTimeCommands(VkQueue queue, VkCommandBuffer staging_cmd_buf) const
{
	vkEndCommandBuffer(staging_cmd_buf);

	// 提交
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &staging_cmd_buf;
	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

	// 等待传输完成
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, cmd_pool, 1, &staging_cmd_buf);
}

void VulkanEngine::TransitionImagelayout(VkQueue queue, VkImage img, VkFormat format, uint32_t mip_levels, VkImageLayout oldlayout, VkImageLayout newlayout) const
{
	// 开始记录单次命令
	VkCommandBuffer staging_cmd_buf = BeginSingleTimeCommands();
	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldlayout;
	barrier.newLayout = newlayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = img;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;


	if (oldlayout == VK_IMAGE_LAYOUT_UNDEFINED && newlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		// 图像布局从 未定义 => 传输目的 时用到的管线阶段
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	}
	else if (oldlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newlayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		// 图像布局从 传输目的 => 着色器读取 时用到的管线阶段
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		SDL_Log(LOG_ERROR("Unsupported layout transition!"));
		// 结束记录并提交
		EndAndSubmitSingleTimeCommands(queue, staging_cmd_buf);
		return;
	}

	vkCmdPipelineBarrier(staging_cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	// 结束记录并提交
	EndAndSubmitSingleTimeCommands(queue, staging_cmd_buf);
}

void VulkanEngine::CopyBufferToImage(VkQueue queue, VkBuffer buf, VkImage img, uint32_t width, uint32_t height) const
{
	// 开始记录单次命令
	VkCommandBuffer staging_cmd_buf = BeginSingleTimeCommands();

	VkBufferImageCopy copy_region{};
	copy_region.bufferOffset = 0;
	copy_region.bufferRowLength = 0;
	copy_region.bufferImageHeight = 0;

	copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.imageSubresource.mipLevel = 0;
	copy_region.imageSubresource.baseArrayLayer = 0;
	copy_region.imageSubresource.layerCount = 1;

	copy_region.imageOffset.x = 0;
	copy_region.imageOffset.y = 0;
	copy_region.imageOffset.z = 0;

	copy_region.imageExtent.width = width;
	copy_region.imageExtent.height = height;
	copy_region.imageExtent.depth = 1;

	vkCmdCopyBufferToImage(staging_cmd_buf, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	// 结束记录并提交
	EndAndSubmitSingleTimeCommands(queue, staging_cmd_buf);
}

void VulkanEngine::CopyBuffer(VkQueue queue, VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) const
{
	// 开始记录单次命令
	VkCommandBuffer staging_cmd_buf = BeginSingleTimeCommands();

	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = size;
	vkCmdCopyBuffer(staging_cmd_buf, src_buf, dst_buf, 1, &copy_region);

	// 结束记录并提交
	EndAndSubmitSingleTimeCommands(queue, staging_cmd_buf);
}

int VulkanEngine::Write2BufferByStage(VkBuffer dst_buf, const void* src_data, VkDeviceSize buf_size) const
{
	// 创建一个暂存缓冲区
	VkBuffer staging_buf;
	VkDeviceMemory staging_buf_mem;
	if (CreateBuffer(buf_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buf, &staging_buf_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create staging buffer!"));
		return 1;
	}

	// 将顶点数据映射到暂存缓冲区中
	void* data;
	vkMapMemory(device, staging_buf_mem, 0, buf_size, 0, &data);
	memcpy(data, src_data, (size_t)buf_size);
	vkUnmapMemory(device, staging_buf_mem);

	CopyBuffer(graphicsqueue, staging_buf, dst_buf, buf_size);

	// 释放和销毁暂存缓冲区
	vkDestroyBuffer(device, staging_buf, nullptr);
	vkFreeMemory(device, staging_buf_mem, nullptr);

	return 0;
}

int VulkanEngine::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphore_info{};
	VkFenceCreateInfo fence_info{};
	VkResult result;

	for (int i=0; i < 2; i++)
	{
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		result = vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]);
		if (result != VK_SUCCESS)
		{
			SDL_Log(LOG_ERROR("Failed to Create Image Semaphore!([%d], Error: %d)"), i, result);
			return 1;
		}
		result = vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]);
		if (result != VK_SUCCESS)
		{
			SDL_Log(LOG_ERROR("Failed to Create Render Finished Semaphore!([%d], Error: %d)"), i, result);
			return 1;
		}
		result = vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
		if (result != VK_SUCCESS)
		{
			SDL_Log(LOG_ERROR("Failed to Create Fence!([%d], Error: %d)"), i, result);
			return 1;
		}
	}

	SDL_Log(LOG_SUCCESS("Successfully created Sync Object!"));
	return 0;
}
