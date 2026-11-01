//#pragma once
#ifndef _VK_RENDERER_H
#define _VK_RENDERER_H

#include <array>

#include <glm/glm.hpp>

#include "VulkanEngine.h"
#include "Camera.h"
#include "Model.h"

typedef ModelTexture TxtTexture;
//typedef SDL_Rect TextRect;

struct TextColor
{
	aiColor3D color;

	TextColor(float r, float g, float b) : color(r, g, b) {};

	inline static TextColor Red()
	{
		return TextColor(1.f, 0.f, 0.f);
	}

	inline static TextColor Green()
	{
		return TextColor(0.f, 1.f, 0.f);
	}

	inline static TextColor Blue()
	{
		return TextColor(0.f, 0.f, 1.f);
	}

	inline static TextColor White()
	{
		return TextColor(1.f, 1.f, 1.f);
	}
};

class VkRenderer
{
public:
	VkRenderer() = default;
	~VkRenderer() = default;

	VkRenderer(const VkRenderer&) = delete;
	VkRenderer& operator=(const VkRenderer&) = delete;

	// 初始化和构建基于vulkan引擎的渲染器
	int Init(SDL_Window* window);

	// 销毁基于vulkan引擎渲染器
	void Destroy();


	/**
	 * @brief 开始记录绘制命令
	 * @return 成功返回0，否则返回1
	 */
	int BeginDraw();
	/**
	 * @brief 结束记录绘制命令，并提交命令，以及呈现绘制结果
	 * @return 成功返回0，否则返回1
	 */
	int EndDraw();

	/**
	 * @brief 开始默认的3D图形渲染通道
	 * @return 成功返回0，否则返回1
	 */
	int BeginRenderPass();
	/**
	 * @brief 结束渲染通道
	 * @return 成功返回0，否则返回1
	 */
	int EndRenderPass();

	/**
	 * @brief 记录完整的绘制模型的命令
	 * @param module - 要绘制的模型
	 * @param camera - 绘制用到的摄像机
	 * @return 成功返回0，否则返回1
	 */
	int DrawModel(Model& module, Camera& camera);

	/**
	 * @brief 绘制文本
	 * @param text_img_set - 绑定了一个由文本生成的纹理的描述符集
	 * @param color - 文本颜色
	 * @param view - 视图窗口
	 * @param scissor - 裁剪矩形
	 * @return 成功返回0，否则返回1
	 */
	int DrawText(VkDescriptorSet text_img_set, TextColor color, VkRect2D view, VkRect2D scissor);

	// 绘制HUD(显示帧率)
	int DrawHUD();


	/**
	 * @brief 更新HUD的内容
	 * @param fps - 被转换成字符串的fps的指针
	 * @return 成功返回0，否则返回1
	 */
	int UpdateHUD(const char* fps);

	// vulkan引擎的组合
	VulkanEngine vk_engine;

	// ([0], set=1) - textures, ([1], set=2) - (storage buffer)bones matrixess
	inline const RenderSystem<2>& GetGeneral3DModelRenderSystem() const
	{
		return general_3Dmodel_render_system;
	}

private:

	int CreateGeneral3DModelRenderPass();

	int CreateTextRenderSystem();
	void DestroyTextRenderSystem();

	int CreateGeneral3DModelRenderSystem();
	int CreateGeneral3DModelDescriptorLayout();
	int CreateGeneral3DModelPipelineLayout();
	int CreateGeneral3DModelPipeline();
	void DestroyGeneral3DModelRenderSystem();

	// HUD组件
	struct HUDComponent
	{
		// HUD显示的网格顶点
		//std::array<VulkanEngine::TextVertex, 28> vertex{};

		VkBuffer vertex_buf{};
		VkDeviceMemory vertex_buf_mem{};
		VkBuffer index_buf{};
		VkDeviceMemory index_buf_mem{};
		uint32_t index_size = 0;

		// 数字文本的纹理
		TxtTexture number_txt_tex{};
		// "FPS"文本的纹理
		TxtTexture FPS_txt_tex{};
		// 数字文本的纹理的描述符
		VkDescriptorSet number_txt_tex_set{};

		// 视图窗口和裁剪矩形
		VkViewport view_port{};
		VkRect2D scissor{};

		// 组件专用渲染系统
		VkRenderPass HUDrender_pass = nullptr;
		VkFramebuffer frame_buf{};
		RenderSystem<1> HUD_render_system{};

		// 渲染后输出的图像
		VkImage HUD_texture{};
		VkDeviceMemory HUD_texture_mem{};
		VkImageView HUD_texture_view{};
		VkExtent2D texture_extent{ 200, 100 };
		// 输出的图像纹理的描述符
		VkDescriptorSet HUD_texture_set{};

		int Create(VkRenderer& vk_renderer);
		void Destroy(VulkanEngine& vk_engine);
		int CreateHUDRenderSystem(VkRenderer& vk_renderer);

	} HUD_component;

	// 全屏四边形顶点
	VkBuffer full_screen_rect_vertex{};
	VkDeviceMemory full_screen_rect_vertex_mem{};
	VkBuffer full_screen_rect_index{};
	VkDeviceMemory full_screen_rect_index_mem{};
	const uint32_t full_screen_rect_index_size = 6;

	// 文本渲染系统
	VkDescriptorSetLayout txt_tex_descriptor_layout{};  // 文本纹理的描述符布局
	VkPipelineLayout txt_layout{};  // 文本渲染管线布局
	VkPipeline txt_pipeline{};  // 文本渲染管线句柄
	VkDescriptorSet txt_tex_set{};  // 文本纹理描述符集

	// 通用3D模型渲染系统
	VkRenderPass general_3Dmodel_renderpass = nullptr;
	// ([0], set=1) - textures, ([1], set=2) - (storage buffer)bones matrixess
	RenderSystem<2> general_3Dmodel_render_system{};
	VkPipeline vertex_pipeline = nullptr;
	VkPipeline tangent_vertex_pipeline = nullptr;
	VkPipeline bone_vertex_pipeline = nullptr;
	VkPipeline bone_tangent_vertex_pipeline = nullptr;

	// 存储可用的交换链图像索引
	uint32_t swapchain_image_index = 0;

	// 视图窗口和裁剪矩形
	VkViewport view_port{};
	VkRect2D scissor{};

};



#endif // !_VK_RENDERER_H