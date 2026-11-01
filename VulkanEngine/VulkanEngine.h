#ifndef _VULKAN_ENGINE_H
#define _VULKAN_ENGINE_H
//#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <assimp/material.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class ModelTexture;
class ModelMaterial;
class ModelMesh;
class ModelAnimation;
class Model;
struct aiScene;

class VulkanEngine
{
public:
	struct Vertex
	{
		glm::vec3 position;  // 坐标
		glm::vec3 normal;    // 法线
		glm::vec2 tex_coord; // uv坐标
	};

	struct TangentVertex
	{
		glm::vec3 position;  // 坐标
		glm::vec3 normal;    // 法线
		glm::vec2 tex_coord; // uv坐标
		glm::vec3 tangent;   // uv的u方向切线
	};

	struct BoneIdxAndWeight
	{
		int      boneidx[4];  // 骨骼索引(最多4个)
		float    weights[4];  // 每个骨骼的权重

		// 找到4个权重中最小的那个的索引
		inline uint8_t FindMinWeight()
		{
			uint8_t result = 0;
			float temp = weights[0];

			for (uint8_t i=1; i < 4; i++)
			{
				if (temp > weights[i])
				{
					temp = weights[i];
					result = i;
				}
			}

			return result;
		}
	};

	struct BoneVertex
	{
		glm::vec3 position;     // 坐标
		glm::vec3 normal;       // 法线
		glm::vec2 tex_coord;    // uv坐标
		BoneIdxAndWeight bones; // 骨骼索引和权重
	};

	struct BoneTangentVertex
	{
		glm::vec3 position;     // 坐标
		glm::vec3 normal;       // 法线
		glm::vec2 tex_coord;    // uv坐标
		glm::vec3 tangent;      // uv的u方向切线
		BoneIdxAndWeight bones; // 骨骼索引和权重
	};

	struct TextVertex
	{
		glm::vec2 position;
		glm::vec2 uv;
		uint32_t tex_idx;
		uint32_t vertex_idx;
	};

	struct MVPMatrix
	{
		glm::mat4 model; // 模型矩阵
		glm::mat4 view;  // 视图矩阵
		glm::mat4 proj;  // 透视投影矩阵
	};

public:
	VulkanEngine() = default;
	~VulkanEngine() = default;

	VulkanEngine(const VulkanEngine&) = delete;
	VulkanEngine& operator=(const VulkanEngine&) = delete;

	// 初始化自定义的vulkan引擎
	int InitVulkan(SDL_Window* window);
	// 等待索引的帧绘制完毕
	void WaitPrevFrame(uint8_t frame_index);
	// 销毁vulkan引擎
	void Destroy();

public:
	// vulkan实例
	VkInstance instance;
	// 物理设备(GPU)
	VkPhysicalDevice physicalDevice;
	// 窗口表面
	VkSurfaceKHR surface;
	// 逻辑设备
	VkDevice device;
	// 队列句柄
	uint32_t graphics_queue_index;
	VkQueue graphicsqueue;  // 图形能力的队列句柄
	uint32_t present_queue_index;
	VkQueue presentqueue;   // 呈现能力的队列句柄

	// 交换链
	VkSwapchainKHR swapchain;
	VkFormat swapchain_format;  // 交换链格式
	VkExtent2D swapchain_extent;  // 交换链范围
	std::vector<VkImage> swapchain_images;  // 交换链图像数组
	std::vector<VkImageView> swapchain_imageviews;  // 交换链图像视图数组

	// 纹理图像采样器
	VkSampler texture_sampler;

	// 深度图像和视图
	VkImage depth_image;
	VkImageView depth_imgview;
	VkDeviceMemory depth_image_mem;

	// 描述符池和描述符布局
	VkDescriptorSetLayout one_matrix_descriptor_layout;  // 只有一个缓冲区的描述符布局
	VkDescriptorSetLayout three_texture_sampler_descriptor_layout;  // 有三个材质纹理的描述符布局
	VkDescriptorPool descriptor_pool;  // 描述符池

	// 图形管线
	//VkRenderPass render_pass;  // 渲染过程
	//VkPipelineLayout layout;  // 通用渲染管线布局
	//VkPipeline pipeline;  // 通用渲染管线句柄
	//VkPipelineLayout bone_model_pipeline_layout;  // 骨骼模型渲染管线布局
	//VkPipeline bone_model_pipeline;  // 骨骼模型渲染管线句柄

	// 帧缓冲
	std::vector<VkFramebuffer> swapchain_frame_buffers;

	// 命令池
	VkCommandPool cmd_pool;
	// 命令缓冲区
	VkCommandBuffer cmd_bufs[2];

	// 信号同步对象
	VkSemaphore image_available_semaphores[2];  // 表示已从交换链获取图像并准备好进行渲染的信号
	VkSemaphore render_finished_semaphores[2];  // 表示渲染已完成并且可以进行呈现的信号
	VkFence in_flight_fences[2];  // 表示等待前一帧的栅栏

	// 当前正在使用的帧的索引(目前允许2个帧同时飞行，当GPU在渲染第1个帧时，CPU可以准备第2个帧的渲染信息，等GPU渲染完成后直接提交命令)
	uint8_t current_frame = 0;

	/**
	 * @brief 创建一个vulkan实例
	 * @return 成功则返回0，否则返回1
	 */
	int CreateVkInstance();

	/**
	 * @brief 创建一个物理设备
	 * @return 成功则返回0，否则返回1
	 */
	int CreatePhysicalDevice();

	/**
	 * @brief 创建所需队列簇(获取对应队列簇的索引)
	 * @return 成功则返回0，否则返回1
	 */
	int CreateQueue();

	/**
	 * @brief 创建一个逻辑设备
	 * @return 成功则返回0，否则返回1
	 */
	int CreateLogicalDevice();

	/**
	 * @brief 创建交换链
	 * @param window - 程序中用到的窗体指针
	 * @return 成功则返回0，否则返回1
	 */
	int CreateSwapchain(SDL_Window* window);

	/**
	 * @brief 创建交换链的图像和视图
	 * @return 成功则返回0，否则返回1
	 */
	int CreateSwapchainImageAndViews();

	/**
	 * @brief 创建深度相关资源
	 * @return 成功则返回0，否则返回1
	 */
	int CreateDepthResources();

	/**
	 * @brief 创建帧缓冲
	 * @param render_pass - 用于帧缓冲区绑定的渲染通道
	 * @return 成功则返回0，否则返回1
	 */
	int CreateSwapchainFrameBuffer(VkRenderPass render_pass);

	/**
	 * @brief 创建一个只有1x1像素的纹理
	 * @param r - 像素颜色的RGBA值
	 * @param g - 像素颜色的RGBA值
	 * @param b - 像素颜色的RGBA值
	 * @param a - 像素颜色的RGBA值
	 * @return 成功则返回0，否则返回1
	 */
	int Create1x1PixelTexture(
		uint8_t r, uint8_t g, uint8_t b, uint8_t a,
		VkImage* texture_img_ptr, VkImageView* texture_view_ptr, VkDeviceMemory* texture_memory_ptr
	) const;

	/**
	 * @brief 获取物理设备中所有队列簇的信息
	 * @param queue_pro - 存放队列簇信息的数组的引用
	 * @return 成功则返回0，否则返回1
	 */
	int GetQueueFamiles(std::vector<VkQueueFamilyProperties>& queue_pro);
	/**
	 * @brief 根据队列簇的枚举标志(VkQueueFlagBits)找到具备相应能力的队列簇(例如：找到具备图形能力的队列簇，可传入 VK_QUEUE_GRAPHICS_BIT 枚举值来寻找)
	 * @param queue_flag - 队列簇的枚举标志(VkQueueFlagBits)
	 * @return 返回符合条件的队列簇的下标，若没找到则返回-1
	 */
	uint32_t FindQueueFamiles(VkQueueFlagBits queue_flag);
	/**
	 * @brief 找到具备向窗口表面呈现的能力的队列簇的下标
	 * @param surface - 创建好的窗口表面
	 * @return 返回符合条件的队列簇的下标，若没找到则返回-1
	 */
	uint32_t FindSurfaceSupportQueueFamily(VkSurfaceKHR surface);

	/**
	 * @brief 根据着色器文件创建着色器模块
	 * @param shader_file - shader文件路径
	 * @param shader_module_ptr - 存放着色器模块的地方
	 * @return 成功则返回0，否则返回1
	 */
	int CreateShaderModule(const char* shader_file, VkShaderModule* shader_module_ptr);

	/**
	 * @brief 创建描述符集布局
	 * @param set_layout_ptr - 存放描述符集布局的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateDescriptorSetLayout();

	/**
	 * @brief 创建描述符池
	 * @param device - 创建好的逻辑设备
	 * @param pool_ptr - 存放描述符池的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateDescriptorPool(VkDescriptorPool* pool_ptr);
	/**
	 * @brief 创建描述符集
	 * @param device - 创建好的逻辑设备
	 * @param set_layout - 创建好的描述符布局
	 * @param descriptor_pool - 创建好的描述符池
	 * @param descriptor_set_ptr - 存放描述符集的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateDescriptorSets(
		VkDescriptorSetLayout set_layout, VkDescriptorPool descriptor_pool,
		VkDescriptorSet* descriptor_set_ptr
	) const;

	/**
	 * @brief 创建通用渲染管线布局
	 * @param layout_ptr - 存放管线布局的地方
	 * @return 成功则返回0，否则返回1
	 */
	int CreatePipelineLayout(VkPipelineLayout* layout_ptr);

	/**
	 * @brief 创建骨骼模型渲染管线布局
	 * @param layout_ptr - 存放管线布局的地方
	 * @return 成功则返回0，否则返回1
	 */
	int CreateBoneAnimModelPipelineLayout(VkPipelineLayout* layout_ptr);

	/**
	 * @brief 创建渲染通道
	 * @param format - 颜色附件的格式
	 * @param render_pass_ptr - 存放渲染通道的地方
	 * @return 成功则返回0，否则返回1
	 */
	int CreateRenderPass(VkFormat format, VkRenderPass* render_pass_ptr);

	/**
	 * @brief 创建纹理采样器
	 * @param max_anisotropy - 用于计算最终颜色的纹素采样数量
	 * @param smapler_ptr - 存放采样器的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateTextureSampler(float max_anisotropy, VkSampler* smapler_ptr);

	/**
	 * @brief 创建命令池
	 * @return 成功返回0，否则返回1
	 */
	int CreateCmdPool();
	/**
	 * @brief 创建命令缓冲区
	 * @return 成功返回0，否则返回1
	 */
	int CreateCmdBuffer();


	/**
	 * @brief 寻找合适的深度格式
	 * @return 从
	 * @return VK_FORMAT_D32_SFLOAT,
	 * @return VK_FORMAT_D32_SFLOAT_S8_UINT,
	 * @return VK_FORMAT_D24_UNORM_S8_UINT
	 * @return 中返回合适的格式，若都不支持则返回 VK_FORMAT_UNDEFINED
	 */
	VkFormat FindDepthFormat();

	/**
	 * @brief 创建图像
	 * @param width - 图像的宽
	 * @param height - 图像的高
	 * @param mip_levels - mip级别数量
	 * @param format - 图像格式
	 * @param tiling - 图像平铺的模式
	 * @param usage - 图像用途
	 * @param properties - 图像的特性
	 * @param image_ptr - 存放图像的地方
	 * @param image_mem_ptr - 存放图像内存的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateImage(
		uint32_t width, uint32_t height,
		uint32_t mip_levels,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage* image_ptr, VkDeviceMemory* image_mem_ptr
	) const;

	/**
	 * @brief 创建图像视图
	 * @param image - 需要创建视图的图像
	 * @param format - 图像格式
	 * @param mip_levels - mip级别数量
	 * @param aspectflags - 指定视图用在哪些方面(例如颜色方面，深度方面)
	 * @param image_view_ptr 存放图像视图的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateImageView(
		VkImage image,
		VkFormat format,
		uint32_t mip_levels,
		VkImageAspectFlags aspectflags,
		VkImageView* image_view_ptr
	) const;

	/**
	 * @brief 根据模型创建纹理
	 * @param texture_file - 纹理的路径
	 * @param texture - 纹理的相关数据会存放在该纹理引用中
	 * @return 成功返回0，否则返回1
	 */
	int CreateModelTexture(const char* texture_file, ModelTexture& texture) const;
	/**
	 * @brief 销毁纹理对象
	 * @param texture - 被销毁的纹理
	 */
	void DestroyModelTexture(ModelTexture& texture) const;

	/**
	 * @brief 通过图片创建图像纹理
	 * @param file - 文件路径
	 * @param image_ptr - 存放图像的地方
	 * @param image_mem_ptr - 存放图像内存的地方
	 * @param mip_levels_ptr - 存放mip级别数量的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateTextureImage(
		const char* file,
		VkImage* image_ptr, VkDeviceMemory* image_mem_ptr, uint32_t* mip_levels_ptr
	) const;
	/**
	 * @brief 通过创建好的SDL表面，创建图像纹理
	 * @param surface - 基于SDL创建的surface表面(像素格式应为SDL_PIXELFORMAT_RGBA32，这样的RGBA的32位格式)
	 * @param image_ptr - 存放图像的地方
	 * @param image_mem_ptr - 存放图像内存的地方
	 * @param mip_levels_ptr - 存放mip级别数量的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateTextureImage(
		SDL_Surface* surface,
		VkImage* image_ptr, VkDeviceMemory* image_mem_ptr, uint32_t* mip_levels_ptr
	) const;
	/**
	 * @brief 创建纹理图像视图
	 * @param texture_image - 用来创建图像视图的图像
	 * @param format - 图像格式
	 * @param mip_levels - mip级别数量
	 * @param texture_imageview_ptr - 存放图像视图的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateTextureImageView(VkImage texture_image, VkFormat format, uint32_t mip_levels, VkImageView* texture_imageview_ptr) const;

	/**
	 * @brief 生成纹理的mipmaps
	 * @param trans_queue - 具备传输能力的队列簇
	 * @param img - 要生成的图像
	 * @param texwidth - 图像宽度
	 * @param texheight - 图像高度
	 * @param mip_levels - mip级别的数量
	 */
	void GenerateMipmaps(
		VkQueue trans_queue,
		VkImage img,
		int32_t texwidth, int32_t texheight,
		uint32_t mip_levels
	) const;

	/**
	 * @brief 创建缓冲区
	 * @param size - 缓冲区大小
	 * @param usage - 缓冲区用途，例如：用作顶点缓冲区(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
	 * @param properties - 缓冲区的特性
	 * @param buffer_ptr - 存放缓冲区的地方
	 * @param memory_ptr - 存放缓冲区内存的地方
	 * @return 成功返回0，否则返回1
	 */
	int CreateBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkBuffer* buffer_ptr,
		VkDeviceMemory* memory_ptr
	) const;
	/**
	 * @brief 销毁缓冲区并释放内存(GPU)
	 * @param buf - 要销毁的缓冲区
	 * @param memory - 缓冲区对应的内存
	 */
	void DestroyBuffer(VkBuffer buf, VkDeviceMemory memory) const;

	/**
	 * @brief 开始记录一个单次命令
	 * @return 返回一个临时的命令缓冲区
	 */
	VkCommandBuffer BeginSingleTimeCommands() const;
	/**
	 * @brief 结束记录一个单次命令并提交该命令
	 * @param queue - 具备传输能力的队列簇
	 * @param staging_cmd_buf - 记录命令时使用的临时命令缓冲区
	 */
	void EndAndSubmitSingleTimeCommands(VkQueue queue, VkCommandBuffer staging_cmd_buf) const;

	/**
	 * @brief 转换图像布局
	 * @param queue - 具备转换能力的队列簇
	 * @param img - 要转换的图像
	 * @param format - 图像的格式(暂未被使用，在深度缓冲中使用)
	 * @param mip_levels - mip级别数量
	 * @param oldlayout - 旧布局
	 * @param newlayout - 新布局
	 */
	void TransitionImagelayout(
		VkQueue queue,
		VkImage img,
		VkFormat format,
		uint32_t mip_levels,
		VkImageLayout oldlayout, VkImageLayout newlayout
	) const;
	/**
	 * @brief 从缓冲区复制数据到图像中
	 * @param queue - 具备传输能力的队列簇
	 * @param buf - 源缓冲区
	 * @param img - 目的图像
	 * @param width - 图像的宽
	 * @param height - 图像的高
	 */
	void CopyBufferToImage(VkQueue queue, VkBuffer buf, VkImage img, uint32_t width, uint32_t height) const;
	/**
	 * @brief 复制传输缓冲区的数据
	 * @param queue - 具备传输能力的队列簇
	 * @param src_buf - 源缓冲区
	 * @param dst_buf - 目的缓冲区
	 * @param size - 数据大小
	 */
	void CopyBuffer(VkQueue queue, VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) const;
	/**
	 * @brief 通过暂存缓冲区来写入缓冲区数据
	 * @param dst_buf - 目标缓冲区
	 * @param src_data - 要写入的源数据
	 * @param buf_size - 数据大小
	 * @return 成功返回0，否则返回1
	 */
	int Write2BufferByStage(VkBuffer dst_buf, const void* src_data, VkDeviceSize buf_size) const;

	/**
	 * @brief 创建需要的同步对象(信号以及栅栏)
	 * @return 成功返回0，否则返回1
	 */
	int CreateSyncObjects();

private:

};

// 需要被渲染器(VkRenderer)使用的渲染系统
// DesCount - 管线的描述符集布局的数量
template <int DesCount = 1>
struct RenderSystem
{
	char pipeline_name[32];
	// 渲染通道引用
	VkRenderPass render_pass = nullptr;
	// 描述符布局
	VkDescriptorSetLayout descriptor_layout[DesCount]{};
	//uint32_t descriptor_layout_count = DesCount;
	// 渲染管线布局
	VkPipelineLayout pipeline_layout = nullptr;
	// 渲染管线句柄
	VkPipeline pipeline = nullptr;

	// 销毁(描述符布局，渲染管线布局，渲染管线句柄)
	void Destroy(const VulkanEngine& vk_engine);
};

#endif // !_VULKAN_ENGINE_H