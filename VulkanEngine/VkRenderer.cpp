#include <array>

#include <vulkan/vulkan_extension_inspection.hpp>

#include "Model.h"
#include "Log_about.h"
#include "VkRenderer.h"

int VkRenderer::Init(SDL_Window* window)
{
	vk_engine.InitVulkan(window);

	VulkanEngine::TextVertex full_screen_vertex[4] = {
		{{-1.f, 1.f}, {0.f, 1.f}, 0, 0},
		{{1.f, 1.f}, {1.f, 1.f}, 0, 0},
		{{1.f, -1.f}, {1.f, 0.f}, 0, 0},
		{{-1.f, -1.f}, {0.f, 0.f}, 0, 0},
	};
	uint32_t full_screen_index[6] = { 0, 1, 2, 0, 2, 3 };

	// 创建全屏四边形顶点缓冲区
	constexpr VkDeviceSize vertex_size = sizeof(VulkanEngine::TextVertex) * 4;
	if (vk_engine.CreateBuffer(vertex_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&full_screen_rect_vertex, &full_screen_rect_vertex_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create full screen rect vertex buffer!"));
	}
	if (vk_engine.Write2BufferByStage(full_screen_rect_vertex, full_screen_vertex, vertex_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write full screen rect vertex data to vertex buffer!"));
	}

	// 创建全屏四边形索引缓冲区
	constexpr VkDeviceSize index_size = sizeof(uint32_t) * 6;
	if (vk_engine.CreateBuffer(index_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&full_screen_rect_index, &full_screen_rect_index_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create full screen rect index buffer!"));
	}
	if (vk_engine.Write2BufferByStage(full_screen_rect_index, full_screen_index, index_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write full screen rect index data to index buffer!"));
	}
	SDL_Log(LOG_SUCCESS("Successfully Created full screen rect vertex and index buffer"));

	CreateGeneral3DModelRenderPass();
	vk_engine.CreateSwapchainFrameBuffer(general_3Dmodel_renderpass);

	CreateGeneral3DModelRenderSystem();
	CreateTextRenderSystem();

	HUD_component.Create(*this);

	char temp_fps[8] = { 0 };
	VkResult result;

	// 重置命令缓冲区并开始记录命令
	vkResetCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], 0);
	VkCommandBufferBeginInfo buf_begin_info{};
	buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	buf_begin_info.flags = 0;
	buf_begin_info.pInheritanceInfo = nullptr;
	result = vkBeginCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], &buf_begin_info);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Begin recording Command Buffer!(Error: %d)"), result);
		return 1;
	}

	UpdateHUD(temp_fps);

	// 结束记录命令
	result = vkEndCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to recording Command Buffer!(Error: %d)"), result);
		return 1;
	}

	// 提交命令
	VkSubmitInfo submit_info{};
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = nullptr;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk_engine.cmd_bufs[vk_engine.current_frame];
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = nullptr;

	result = vkQueueSubmit(vk_engine.graphicsqueue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Submit Draw Command!(Error: %d)"), result);
		return 1;
	}

	return 0;
}

void VkRenderer::Destroy()
{
	HUD_component.Destroy(vk_engine);

	DestroyTextRenderSystem();
	DestroyGeneral3DModelRenderSystem();

	vkDestroyRenderPass(vk_engine.device, general_3Dmodel_renderpass, nullptr);

	vk_engine.DestroyBuffer(full_screen_rect_index, full_screen_rect_index_mem);
	vk_engine.DestroyBuffer(full_screen_rect_vertex, full_screen_rect_vertex_mem);

	vk_engine.Destroy();
}

int VkRenderer::BeginDraw()
{
	VkResult result;

	// 等待前一帧
	vk_engine.WaitPrevFrame(vk_engine.current_frame);
	/*vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &in_flight_fence);*/

	// 获取可用的交换链图像索引
	vkAcquireNextImageKHR(vk_engine.device, vk_engine.swapchain, UINT64_MAX, vk_engine.image_available_semaphores[vk_engine.current_frame], VK_NULL_HANDLE, &swapchain_image_index);

	// 重置命令缓冲区
	vkResetCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], 0);

	// 开始命令记录
	//std::array<VkClearValue, 2> clear_values{};
	//clear_values[0].color = { {0.f, 0.f, 0.f, 1.f} };
	//clear_values[1].depthStencil = { 1.f, 0 };

	VkCommandBufferBeginInfo buf_begin_info{};
	buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	buf_begin_info.flags = 0;
	buf_begin_info.pInheritanceInfo = nullptr;
	result = vkBeginCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], &buf_begin_info);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Begin recording Command Buffer!(Error: %d)"), result);
		return 1;
	}

	return 0;
}

int VkRenderer::EndDraw()
{
	VkResult result;

	//vkCmdEndRenderPass(vk_engine.cmd_bufs[vk_engine.current_frame]);
	result = vkEndCommandBuffer(vk_engine.cmd_bufs[vk_engine.current_frame]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to recording Command Buffer!(Error: %d)"), result);
		return 1;
	}

	// 提交命令
	VkSubmitInfo submit_info{};
	VkSemaphore wait_semaphores[] = { vk_engine.image_available_semaphores[vk_engine.current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signal_semaphore[] = { vk_engine.render_finished_semaphores[vk_engine.current_frame] };

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk_engine.cmd_bufs[vk_engine.current_frame];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphore;

	result = vkQueueSubmit(vk_engine.graphicsqueue, 1, &submit_info, vk_engine.in_flight_fences[vk_engine.current_frame]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Submit Draw Command!(Error: %d)"), result);
		return 1;
	}

	// 将结果返回给交换链以呈现
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphore;

	VkSwapchainKHR swapchains[] = { vk_engine.swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &swapchain_image_index;
	present_info.pResults = nullptr;

	result = vkQueuePresentKHR(vk_engine.presentqueue, &present_info);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Present images!(Error: %d)"), result);
		return 1;
	}

	vk_engine.current_frame = (vk_engine.current_frame + 1) & 1;

	return 0;
}

int VkRenderer::BeginRenderPass()
{
	// 开始命令记录
	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color = { {0.f, 0.f, 0.f, 1.f} };
	clear_values[1].depthStencil = { 1.f, 0 };

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = general_3Dmodel_renderpass;
	render_pass_info.framebuffer = vk_engine.swapchain_frame_buffers[swapchain_image_index];
	render_pass_info.renderArea.offset = { 0,0 };
	render_pass_info.renderArea.extent = vk_engine.swapchain_extent;
	render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(vk_engine.cmd_bufs[vk_engine.current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	// 绑定视图窗口和裁剪矩形
	VkViewport& view_port = this->view_port;
	view_port.x = 0;
	view_port.y = 0;
	view_port.width = static_cast<float>(vk_engine.swapchain_extent.width);
	view_port.height = static_cast<float>(vk_engine.swapchain_extent.height);
	view_port.minDepth = 0.f;
	view_port.maxDepth = 1.f;

	VkRect2D& scissor = this->scissor;
	scissor.offset = { 0,0 };
	scissor.extent = vk_engine.swapchain_extent;

	vkCmdSetViewport(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &view_port);
	vkCmdSetScissor(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &scissor);

	return 0;
}

int VkRenderer::EndRenderPass()
{
	vkCmdEndRenderPass(vk_engine.cmd_bufs[vk_engine.current_frame]);

	return 0;
}

int VkRenderer::DrawModel(Model& model, Camera& camera)
{
	model.trans_mat[3] = glm::vec4(model.position, 1.f);
	VkCommandBuffer cmd_buf = vk_engine.cmd_bufs[vk_engine.current_frame];
	uint8_t cur_frame_idx = vk_engine.current_frame;
	
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, general_3Dmodel_render_system.pipeline_layout, 0, 1, &camera.MVPubo_descriptor_set[cur_frame_idx], 0, nullptr);
	vkCmdPushConstants(cmd_buf, general_3Dmodel_render_system.pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(glm::mat4),
		&model.trans_mat
	);

	//// 对骨骼组件进行同步数据上传
	VkDescriptorSet bone_martix_ssbo = nullptr;
	uint32_t des_sets_count = 1;
	if (model.bone_component)
	{
		model.bone_component->SyncBonesMatrixToUBO(cur_frame_idx);
		bone_martix_ssbo = model.bone_component->bones_matrix_set[cur_frame_idx];
		des_sets_count = 2;
	}

	// 绘制渲染每一个网格
	for (const ModelMesh& mesh : model.vertex_meshs)
	{
		VkPipeline pipeline = nullptr;
		uint32_t vertex_flags = mesh.vertex_flags & 0xffff;
		uint32_t texture_flags = (mesh.vertex_flags >> 16) & 0xffff;

		switch (mesh.vertex_flags)
		{
		case 0x0:      // 基础网格顶点(position,normal,texCoord)
			pipeline = vertex_pipeline;
			break;
		case 0x1:      // 骨骼网格顶点(position,normal,texCoord,boneidx,boneweight)
			pipeline = bone_vertex_pipeline;
			break;
		case 0x10000:  // 仅切线网格顶点(position,normal,texCoord,tangent)
			pipeline = tangent_vertex_pipeline;
			break;
		case 0x10001:  // 骨骼和切线网格顶点(position,normal,texCoord,tangent,boneidx,boneweight)
			pipeline = bone_tangent_vertex_pipeline;
			break;
		default:
			pipeline = vertex_pipeline;
			break;
		}

		// 绑定通用3D模型渲染管线
		vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// 推送常量
		vkCmdPushConstants(cmd_buf, general_3Dmodel_render_system.pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT,
			64, sizeof(glm::mat4),
			&mesh.node_and_anim_matrix
		);
		vkCmdPushConstants(cmd_buf, general_3Dmodel_render_system.pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT,
			128, sizeof(uint32_t),
			&vertex_flags
		);
		vkCmdPushConstants(cmd_buf, general_3Dmodel_render_system.pipeline_layout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			132, sizeof(uint32_t),
			&texture_flags
		);

		// 绑定顶点缓冲区
		VkBuffer vertex_buffers[] = { mesh.vertex_buf };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd_buf, 0, 1, vertex_buffers, offsets);

		// 绑定描述符集
		VkDescriptorSet descriptor_sets[2] = { mesh.material->descriptor_set, bone_martix_ssbo };
		vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, general_3Dmodel_render_system.pipeline_layout, 1, des_sets_count, descriptor_sets, 0, nullptr);

		// 绑定索引缓冲区
		vkCmdBindIndexBuffer(cmd_buf, mesh.index_buf, 0, VK_INDEX_TYPE_UINT32);

		// 记录绘制索引命令
		vkCmdDrawIndexed(cmd_buf, mesh.indices_size, 1, 0, 0, 0);
	}

	return 0;
}

int VkRenderer::DrawText(VkDescriptorSet text_img_set, TextColor color, VkRect2D view, VkRect2D scissor)
{
	// 绑定视图窗口和裁剪矩形
	VkViewport view_port;
	view_port.x = (float)view.offset.x;
	view_port.y = (float)view.offset.y;
	view_port.width = (float)view.extent.width;
	view_port.height = (float)view.extent.height;
	view_port.minDepth = 0.f;
	view_port.maxDepth = 1.f;

	VkRect2D scissor_rect = scissor;

	vkCmdSetViewport(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &view_port);
	vkCmdSetScissor(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &scissor_rect);

	/*vkCmdPushConstants(vk_engine.cmd_bufs[vk_engine.current_frame], txt_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(glm::uvec2),
		fps
	);*/

	// 绑定文本渲染管线
	vkCmdBindPipeline(vk_engine.cmd_bufs[vk_engine.current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, txt_pipeline);

	// 绑定描述符集
	VkDescriptorSet descriptor_sets[] = { text_img_set };
	vkCmdBindDescriptorSets(vk_engine.cmd_bufs[vk_engine.current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, txt_layout, 0, 1, descriptor_sets, 0, nullptr);

	// 绑定顶点缓冲区
	VkBuffer vertex_buffers[] = { full_screen_rect_vertex };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, vertex_buffers, offsets);

	// 绑定索引缓冲区
	vkCmdBindIndexBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], full_screen_rect_index, 0, VK_INDEX_TYPE_UINT32);

	// 记录绘制索引命令
	vkCmdDrawIndexed(vk_engine.cmd_bufs[vk_engine.current_frame], full_screen_rect_index_size, 1, 0, 0, 0);

	return 0;
}

int VkRenderer::DrawHUD()
{
	DrawText(HUD_component.HUD_texture_set, TextColor::White(), {{0,0},HUD_component.texture_extent}, {{0,0},HUD_component.texture_extent});

	return 0;
}

int VkRenderer::UpdateHUD(const char* fps)
{
	// 开始渲染通道
	VkClearValue clear_values[1]{};
	clear_values[0].color = { {0.f, 0.f, 0.f, 0.f} };

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = HUD_component.HUDrender_pass;
	render_pass_info.framebuffer = HUD_component.frame_buf;
	render_pass_info.renderArea.offset = { 0,0 };
	render_pass_info.renderArea.extent = HUD_component.texture_extent;
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = clear_values;

	vkCmdBeginRenderPass(vk_engine.cmd_bufs[vk_engine.current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	// 绑定视图窗口和裁剪矩形
	vkCmdSetViewport(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &HUD_component.view_port);
	vkCmdSetScissor(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, &HUD_component.scissor);

	vkCmdPushConstants(vk_engine.cmd_bufs[vk_engine.current_frame], txt_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(glm::uvec2),
		fps
	);

	// 绑定文本渲染管线
	vkCmdBindPipeline(vk_engine.cmd_bufs[vk_engine.current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, HUD_component.HUD_render_system.pipeline);

	// 绑定描述符集
	VkDescriptorSet descriptor_sets[] = { HUD_component.number_txt_tex_set};
	vkCmdBindDescriptorSets(vk_engine.cmd_bufs[vk_engine.current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, HUD_component.HUD_render_system.pipeline_layout, 0, 1, descriptor_sets, 0, nullptr);

	// 绑定顶点缓冲区
	VkBuffer vertex_buffers[] = { HUD_component.vertex_buf};
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(vk_engine.cmd_bufs[vk_engine.current_frame], 0, 1, vertex_buffers, offsets);

	// 绑定索引缓冲区
	vkCmdBindIndexBuffer(vk_engine.cmd_bufs[vk_engine.current_frame], HUD_component.index_buf, 0, VK_INDEX_TYPE_UINT32);

	// 记录绘制索引命令
	vkCmdDrawIndexed(vk_engine.cmd_bufs[vk_engine.current_frame], HUD_component.index_size, 1, 0, 0, 0);

	// 结束渲染通道
	vkCmdEndRenderPass(vk_engine.cmd_bufs[vk_engine.current_frame]);

	return 0;
}


int VkRenderer::CreateGeneral3DModelRenderPass()
{
	VkAttachmentDescription color_attachment{};
	color_attachment.format = vk_engine.swapchain_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = vk_engine.FindDepthFormat();
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

	VkResult result = vkCreateRenderPass(vk_engine.device, &renderpass_info, nullptr, &general_3Dmodel_renderpass);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model Render Pass!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created general 3D model Render Pass!"));

	return 0;
}

int VkRenderer::CreateTextRenderSystem()
{
	//// descriptor set layout
	VkDescriptorSetLayoutBinding bindings[1]{};
	VkDescriptorSetLayoutCreateInfo des_layout_info{};
	VkResult result;

	// (set=0, binding=0) texture
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	des_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	des_layout_info.bindingCount = 1;
	des_layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(vk_engine.device, &des_layout_info, nullptr, &txt_tex_descriptor_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create text texture descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descrtiptor layout of Txt textures!"));

	//// pipeline layout
	VkPushConstantRange push_ranges[1]{};
	push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[0].offset = 0;
	push_ranges[0].size = sizeof(glm::uvec2);

	VkDescriptorSetLayout des_layouts[] = { txt_tex_descriptor_layout };
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = des_layouts;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = push_ranges;

	result = vkCreatePipelineLayout(vk_engine.device, &layout_info, nullptr, &txt_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create text pipeline layout!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Pipeline layout for Txt pipeline!"));

	//// pipeline
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};

	//// 着色器模块阶段
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;
	if (vk_engine.CreateShaderModule("shaders/text_shader.vert.spv", &vert_shader_module)) { return 1; }
	if (vk_engine.CreateShaderModule("shaders/text_shader.frag.spv", &frag_shader_module)) { return 1; }

	// vertex shader 顶点着色器模块
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vert_shader_module;
	shader_stages[0].pName = "main";

	// fragment shader 片段着色器模块
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = frag_shader_module;
	shader_stages[1].pName = "main";

	//// 动态状态
	std::array<VkDynamicState, 2> dynamic_states = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info{};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
	dynamic_state_info.pDynamicStates = dynamic_states.data();

	//// 顶点输入
	VkVertexInputBindingDescription vertex_input_binding{}; // 顶点绑定的描述
	vertex_input_binding.binding = 0;
	vertex_input_binding.stride = sizeof(VulkanEngine::TextVertex);
	vertex_input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertex_input_attribute[3]{}; // 顶点属性的描述
	vertex_input_attribute[0].binding = 0;
	vertex_input_attribute[0].location = 0;
	vertex_input_attribute[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_input_attribute[0].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::position);

	vertex_input_attribute[1].binding = 0;
	vertex_input_attribute[1].location = 1;
	vertex_input_attribute[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_input_attribute[1].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::uv);

	vertex_input_attribute[2].binding = 0;
	vertex_input_attribute[2].location = 2;
	vertex_input_attribute[2].format = VK_FORMAT_R32_UINT;
	vertex_input_attribute[2].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::tex_idx);

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding;
	vertex_input_info.vertexAttributeDescriptionCount = 3;
	vertex_input_info.pVertexAttributeDescriptions = vertex_input_attribute;

	//// 输入汇编
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 每 3 个顶点组成的三角形，不复用
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	//// 视口和裁剪矩形
	VkPipelineViewportStateCreateInfo view_port_state_info{};
	// 由于开启了动态视图和裁剪矩形，所以在记录命令缓冲区时设置视图和裁剪矩形即可
	/*view_port.x = 0.f;
	view_port.y = 0.f;
	view_port.width = (float)swapchain_extent.width;
	view_port.height = (float)swapchain_extent.height;
	view_port.minDepth = 0.f;
	view_port.maxDepth = 1.f;

	scissor.offset = {0, 0};
	scissor.extent = swapchain_extent;*/

	view_port_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	view_port_state_info.viewportCount = 1;
	//view_port_state_info.pViewports = &view_port;
	view_port_state_info.scissorCount = 1;
	//view_port_state_info.pScissors = &scissor;

	//// 光栅化器
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;  // 关闭背面剔除(VK_CULL_MODE_NONE) 开启(VK_CULL_MODE_BACK_BIT)
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.f;
	rasterizer.depthBiasClamp = 0.f;
	rasterizer.depthBiasSlopeFactor = 0.f;

	//// 多重采样
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	//// 深度和模板测试
	VkPipelineDepthStencilStateCreateInfo depth_stencil{};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.f;
	depth_stencil.maxDepthBounds = 1.f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	//depth_stencil.front = {};
	//depth_stencil.back = {};

	//// 颜色混合
	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;  // 启用颜色混合
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  // 混合透明通道
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // color = src * src.alpha + dst * (1 - src.alpha)
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_state{};
	color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state.logicOpEnable = VK_FALSE;
	color_blend_state.logicOp = VK_LOGIC_OP_COPY;
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &color_blend_attachment;
	color_blend_state.blendConstants[0] = 0.f;
	color_blend_state.blendConstants[1] = 0.f;
	color_blend_state.blendConstants[2] = 0.f;
	color_blend_state.blendConstants[3] = 0.f;

	//// 创建管线
	VkGraphicsPipelineCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	create_info.pStages = shader_stages.data();
	// 管线功能
	create_info.pVertexInputState = &vertex_input_info;
	create_info.pInputAssemblyState = &input_assembly_info;
	create_info.pViewportState = &view_port_state_info;
	create_info.pRasterizationState = &rasterizer;
	create_info.pMultisampleState = &multisampling;
	create_info.pDepthStencilState = &depth_stencil;
	create_info.pColorBlendState = &color_blend_state;
	create_info.pDynamicState = &dynamic_state_info;

	create_info.layout = txt_layout;
	create_info.renderPass = general_3Dmodel_renderpass;
	create_info.subpass = 0;

	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &txt_pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create txt render PipeLine!(Error code: %d)"), result);
		return 1;
	}

	// 创建完管线后即可销毁着色器模块
	vkDestroyShaderModule(vk_engine.device, vert_shader_module, nullptr);
	vkDestroyShaderModule(vk_engine.device, frag_shader_module, nullptr);

	//// 创建绑定文本纹理的描述符集
	vk_engine.CreateDescriptorSets(txt_tex_descriptor_layout, vk_engine.descriptor_pool, &txt_tex_set);

	SDL_Log(LOG_SUCCESS("Successfully created Txt pipeline!"));
	return 0;
}

void VkRenderer::DestroyTextRenderSystem()
{
	vkDestroyPipeline(vk_engine.device, txt_pipeline, nullptr);
	vkDestroyPipelineLayout(vk_engine.device, txt_layout, nullptr);
	vkDestroyDescriptorSetLayout(vk_engine.device, txt_tex_descriptor_layout, nullptr);
}

int VkRenderer::CreateGeneral3DModelRenderSystem()
{
	if (CreateGeneral3DModelDescriptorLayout()) { return 1; }
	if (CreateGeneral3DModelPipelineLayout()) { return 1; }
	if (CreateGeneral3DModelPipeline()) { return 1; }

	return 0;
}

int VkRenderer::CreateGeneral3DModelDescriptorLayout()
{
	VkDescriptorSetLayoutBinding bindings[1]{};
	VkDescriptorSetLayoutCreateInfo layout_info{};
	VkResult result;

	// texture descriptor set
	// [1] diffuse(baseColor)
	// [2] specular
	// [3] ambient
	// [4] emissive
	// [5] normal
	// [6] metallicRoughness
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 6;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(vk_engine.device, &layout_info, nullptr, &general_3Dmodel_render_system.descriptor_layout[0]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descriptor layout of general 3D model textures!"));

	// bones matrixes SSBO
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(vk_engine.device, &layout_info, nullptr, &general_3Dmodel_render_system.descriptor_layout[1]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descriptor layout of general 3D model bone matrixs(SSBO)!"));

	return 0;
}

int VkRenderer::CreateGeneral3DModelPipelineLayout()
{
	VkPushConstantRange push_ranges[4]{};
	push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[0].offset = 0;
	push_ranges[0].size = sizeof(uint32_t) + sizeof(glm::mat4) + sizeof(glm::mat4);
	push_ranges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_ranges[1].offset = 132;
	push_ranges[1].size = sizeof(uint32_t);

	VkDescriptorSetLayout des_layouts[] = { vk_engine.one_matrix_descriptor_layout, general_3Dmodel_render_system.descriptor_layout[0], general_3Dmodel_render_system.descriptor_layout[1]};
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 3;
	layout_info.pSetLayouts = des_layouts;
	layout_info.pushConstantRangeCount = 2;
	layout_info.pPushConstantRanges = push_ranges;

	VkResult result = vkCreatePipelineLayout(vk_engine.device, &layout_info, nullptr, &general_3Dmodel_render_system.pipeline_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create general 3D model pipeline layout!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created general 3D model Pipeline layout!"));

	return 0;
}

int VkRenderer::CreateGeneral3DModelPipeline()
{
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	VkResult result;

	//// 着色器模块阶段
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;
	if (vk_engine.CreateShaderModule("shaders/general_3Dmodel_shader.vert.spv", &vert_shader_module)) { return 1; }
	if (vk_engine.CreateShaderModule("shaders/general_3Dmodel_shader.frag.spv", &frag_shader_module)) { return 1; }

	// vertex shader 顶点着色器模块
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vert_shader_module;
	shader_stages[0].pName = "main";

	// fragment shader 片段着色器模块
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = frag_shader_module;
	shader_stages[1].pName = "main";

	//// 动态状态
	std::array<VkDynamicState, 2> dynamic_states = {
		VK_DYNAMIC_STATE_VIEWPORT,          // 动态视图
		VK_DYNAMIC_STATE_SCISSOR            // 动态裁剪矩形
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info{};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
	dynamic_state_info.pDynamicStates = dynamic_states.data();

	//// 顶点输入
	VkVertexInputBindingDescription vertex_binding_description{};
	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = sizeof(VulkanEngine::Vertex);
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription vertex_attribute_description[6]{};
	vertex_attribute_description[0].binding = 0;
	vertex_attribute_description[0].location = 0;
	vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attribute_description[0].offset = 0;                                        // 0 - position
	vertex_attribute_description[1].binding = 0;
	vertex_attribute_description[1].location = 1;
	vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attribute_description[1].offset = sizeof(glm::vec3);                        // 1 - normal	
	vertex_attribute_description[2].binding = 0;
	vertex_attribute_description[2].location = 2;
	vertex_attribute_description[2].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_attribute_description[2].offset = sizeof(glm::vec3) * 2;                       // 2 - tex_coord
	vertex_attribute_description[3].binding = 0;
	vertex_attribute_description[3].location = 3;
	vertex_attribute_description[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attribute_description[3].offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2);   // 3 - tangent
	vertex_attribute_description[4].binding = 0;
	vertex_attribute_description[4].location = 4;
	vertex_attribute_description[4].format = VK_FORMAT_R32G32B32A32_SINT;
	vertex_attribute_description[4].offset = sizeof(glm::vec3) * 3 + sizeof(glm::vec2);   // 4 - boneidx
	vertex_attribute_description[5].binding = 0;
	vertex_attribute_description[5].location = 5;
	vertex_attribute_description[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertex_attribute_description[5].offset = sizeof(glm::vec3) * 3 + sizeof(glm::vec2) * 3;  // 5 - boneweight
	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &vertex_binding_description;
	vertex_input_info.vertexAttributeDescriptionCount = 6;
	vertex_input_info.pVertexAttributeDescriptions = vertex_attribute_description;

	//// 输入汇编
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 每 3 个顶点组成的三角形，不复用
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	//// 视口和裁剪矩形
	// 由于开启了动态视图和裁剪矩形，所以在记录命令缓冲区时设置视图和裁剪矩形即可
	VkPipelineViewportStateCreateInfo view_port_state_info{};
	view_port_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	view_port_state_info.viewportCount = 1;
	view_port_state_info.scissorCount = 1;

	//// 光栅化器
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;  // 关闭背面剔除(VK_CULL_MODE_NONE) 开启(VK_CULL_MODE_BACK_BIT)
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.f;
	rasterizer.depthBiasClamp = 0.f;
	rasterizer.depthBiasSlopeFactor = 0.f;

	//// 多重采样
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	//// 深度和模板测试
	VkPipelineDepthStencilStateCreateInfo depth_stencil{};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.f;
	depth_stencil.maxDepthBounds = 1.f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	//depth_stencil.front = {};
	//depth_stencil.back = {};

	//// 颜色混合
	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_TRUE;  // 启用颜色混合
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  // 混合透明通道
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // color = src * src.alpha + dst * (1 - src.alpha)
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_state{};
	color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state.logicOpEnable = VK_FALSE;
	color_blend_state.logicOp = VK_LOGIC_OP_COPY;
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &color_blend_attachment;
	color_blend_state.blendConstants[0] = 0.f;
	color_blend_state.blendConstants[1] = 0.f;
	color_blend_state.blendConstants[2] = 0.f;
	color_blend_state.blendConstants[3] = 0.f;

	//// 创建管线
	VkGraphicsPipelineCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	create_info.pStages = shader_stages.data();
	// 管线功能
	create_info.pVertexInputState = &vertex_input_info;
	create_info.pInputAssemblyState = &input_assembly_info;
	create_info.pViewportState = &view_port_state_info;
	create_info.pRasterizationState = &rasterizer;
	create_info.pMultisampleState = &multisampling;
	create_info.pDepthStencilState = &depth_stencil;
	create_info.pColorBlendState = &color_blend_state;
	create_info.pDynamicState = &dynamic_state_info;

	create_info.layout = general_3Dmodel_render_system.pipeline_layout;
	create_info.renderPass = general_3Dmodel_renderpass;
	create_info.subpass = 0;

	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &general_3Dmodel_render_system.pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model PipeLine!(Error code: %d)"), result);
		return 1;
	}

	////////////////
	// vertex pipeline
	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = sizeof(VulkanEngine::Vertex);
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	create_info.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	create_info.basePipelineHandle = general_3Dmodel_render_system.pipeline;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &vertex_pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model vertex PipeLine!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created general 3D model vertex Pipeline!"));

	////////////////
	// tangent vertex pipeline
	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = sizeof(VulkanEngine::TangentVertex);
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	create_info.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	create_info.basePipelineHandle = general_3Dmodel_render_system.pipeline;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &tangent_vertex_pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model tangent vertex PipeLine!(Error code: %d)"), result);
		return 1;
	}

	SDL_Log(LOG_SUCCESS("Successfully created general 3D model tangent vertex Pipeline!"));

	////////////////
	// bone tangent vertex pipeline
	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = sizeof(VulkanEngine::BoneTangentVertex);
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	create_info.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	create_info.basePipelineHandle = general_3Dmodel_render_system.pipeline;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &bone_tangent_vertex_pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model bone tangent vertex PipeLine!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created general 3D model bone tangent vertex Pipeline!"));

	////////////////
	// bone vertex pipeline
	vertex_attribute_description[4].offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2);   // 4 - boneidx
	vertex_attribute_description[5].offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2) + sizeof(glm::vec4);  // 5 - boneweight

	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = sizeof(VulkanEngine::BoneVertex);
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	create_info.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	create_info.basePipelineHandle = general_3Dmodel_render_system.pipeline;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &bone_vertex_pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create general 3D model bone vertex PipeLine!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created general 3D model bone vertex Pipeline!"));


	// 创建完管线后即可销毁着色器模块
	vkDestroyShaderModule(vk_engine.device, vert_shader_module, nullptr);
	vkDestroyShaderModule(vk_engine.device, frag_shader_module, nullptr);

	SDL_Log(LOG_SUCCESS("Successfully created general 3D model Pipeline!"));
	return 0;
}

void VkRenderer::DestroyGeneral3DModelRenderSystem()
{
	vkDestroyPipeline(vk_engine.device, bone_tangent_vertex_pipeline, nullptr);
	vkDestroyPipeline(vk_engine.device, bone_vertex_pipeline, nullptr);
	vkDestroyPipeline(vk_engine.device, tangent_vertex_pipeline, nullptr);
	vkDestroyPipeline(vk_engine.device, vertex_pipeline, nullptr);
	vkDestroyPipeline(vk_engine.device, general_3Dmodel_render_system.pipeline, nullptr);
	vkDestroyPipelineLayout(vk_engine.device, general_3Dmodel_render_system.pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(vk_engine.device, general_3Dmodel_render_system.descriptor_layout[0], nullptr);
	vkDestroyDescriptorSetLayout(vk_engine.device, general_3Dmodel_render_system.descriptor_layout[1], nullptr);
}

int VkRenderer::HUDComponent::Create(VkRenderer& vk_renderer)
{
	VulkanEngine& vk_engine = vk_renderer.vk_engine;

	CreateHUDRenderSystem(vk_renderer);

	std::array<VulkanEngine::TextVertex, 28> vertex{};

	vertex[0]  = { {-1.f, 0.4f}, {}, 0 };
	vertex[1]  = { {-0.2f, 0.4f}, {}, 0 };
	vertex[2]  = { {-0.2f, -0.4f}, {}, 0 };
	vertex[3]  = { {-1.f, -0.4f}, {}, 0 };
							 
	vertex[4]  = { {-0.2f, 0.4f}, {}, 1 };
	vertex[5]  = { {0.f, 0.4f}, {}, 1 };
	vertex[6]  = { {0.f, -0.4f}, {}, 1 };
	vertex[7]  = { {-0.2f, -0.4f}, {}, 1 };
							 
	vertex[8]  = { {0.f, 0.4f}, {}, 1 };
	vertex[9]  = { {0.2f, 0.4f}, {}, 1 };
	vertex[10] = { {0.2f, -0.4f}, {}, 1 };
	vertex[11] = { {0.f, -0.4f}, {}, 1 };

	vertex[12] = { {0.2f, 0.4f}, {}, 1 };
	vertex[13] = { {0.4f, 0.4f}, {}, 1 };
	vertex[14] = { {0.4f, -0.4f}, {}, 1 };
	vertex[15] = { {0.2f, -0.4f}, {}, 1 };

	vertex[16] = { {0.4f, 0.4f}, {}, 1 };
	vertex[17] = { {0.6f, 0.4f}, {}, 1 };
	vertex[18] = { {0.6f, -0.4f}, {}, 1 };
	vertex[19] = { {0.4f, -0.4f}, {}, 1 };

	vertex[20] = { {0.6f, 0.4f}, {}, 1 };
	vertex[21] = { {0.8f, 0.4f}, {}, 1 };
	vertex[22] = { {0.8f, -0.4f}, {}, 1 };
	vertex[23] = { {0.6f, -0.4f}, {}, 1 };

	vertex[24] = { {0.8f, 0.4f}, {}, 1 };
	vertex[25] = { {1.f, 0.4f}, {}, 1 };
	vertex[26] = { {1.f, -0.4f}, {}, 1 };
	vertex[27] = { {0.8f, -0.4f}, {}, 1 };

	for (uint32_t i=0; i < vertex.size(); i++)
	{
		vertex[i].vertex_idx = i;
	}

	if (vk_engine.CreateModelTexture("textures/Number.png", number_txt_tex))
	{
		SDL_Log("Failed to create number texture!");
		return 1;
	}
	if (vk_engine.CreateModelTexture("textures/FPS.png", FPS_txt_tex))
	{
		SDL_Log("Failed to create texture of txt 'FPS'!");
		return 1;
	}

	VkDeviceSize vectex_buf_size = vertex.size() * sizeof(VulkanEngine::TextVertex);
	if (vk_engine.CreateBuffer(vectex_buf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vertex_buf, &vertex_buf_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create text vertex buffer!"));
		return 1;
	}
	if (vk_engine.Write2BufferByStage(vertex_buf, vertex.data(), vectex_buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to write text vectex buffer!"));
		return 1;
	}

	std::array<uint32_t, 48> indices{
		0, 1, 3,
		1, 2, 3,
		4, 5, 7,
		5, 6, 7,
		8, 9, 11,
		9, 10, 11,
		12, 13, 15,
		13, 14, 15,
		16, 17, 19,
		17, 18, 19,
		20, 21, 23,
		21, 22, 23,
		24, 25, 27,
		25, 26, 27,
		28, 29, 31,
		29, 30, 31
	};
	index_size = (uint32_t)indices.size();

	VkDeviceSize index_buf_size = indices.size() * sizeof(uint32_t);
	if (vk_engine.CreateBuffer(index_buf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&index_buf, &index_buf_mem))
	{
		SDL_Log(LOG_ERROR("Failed to create text vertex buffer!"));
		return 1;
	}
	if (vk_engine.Write2BufferByStage(index_buf, indices.data(), index_buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write text index buffer!"));
		return 1;
	}

	// 创建描述符集
	vk_engine.CreateDescriptorSets(HUD_render_system.descriptor_layout[0], vk_engine.descriptor_pool, &number_txt_tex_set);

	VkDescriptorImageInfo img_info[2]{};
	img_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_info[0].imageView = FPS_txt_tex.view;
	img_info[0].sampler = vk_engine.texture_sampler;

	img_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_info[1].imageView = number_txt_tex.view;
	img_info[1].sampler = vk_engine.texture_sampler;

	VkWriteDescriptorSet descriptor_writes{};
	descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes.dstSet = number_txt_tex_set;
	descriptor_writes.dstBinding = 0;
	descriptor_writes.dstArrayElement = 0;
	descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_writes.descriptorCount = 2;
	descriptor_writes.pImageInfo = img_info;
	//descriptor_writes.pBufferInfo = &buf_info;
	//descriptor_writes.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vk_engine.device, 1, &descriptor_writes, 0, nullptr);

	// 设置视图窗口和裁剪矩形
	VkViewport& view_port = this->view_port;
	view_port.x = 0;
	view_port.y = 0;
	view_port.width = static_cast<float>(texture_extent.width);
	view_port.height = static_cast<float>(texture_extent.height);
	view_port.minDepth = 0.f;
	view_port.maxDepth = 1.f;

	VkRect2D& scissor = this->scissor;
	scissor.offset = { 0,0 };
	scissor.extent = vk_engine.swapchain_extent;

	return 0;
}

void VkRenderer::HUDComponent::Destroy(VulkanEngine& vk_engine)
{
	vk_engine.DestroyBuffer(vertex_buf, vertex_buf_mem);
	vk_engine.DestroyBuffer(index_buf, index_buf_mem);

	vk_engine.DestroyModelTexture(number_txt_tex);
	vk_engine.DestroyModelTexture(FPS_txt_tex);

	vkDestroyPipeline(vk_engine.device, HUD_render_system.pipeline, nullptr);
	vkDestroyPipelineLayout(vk_engine.device, HUD_render_system.pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(vk_engine.device, HUD_render_system.descriptor_layout[0], nullptr);
	vkDestroyFramebuffer(vk_engine.device, frame_buf, nullptr);
	vkDestroyRenderPass(vk_engine.device, HUDrender_pass, nullptr);

	vkDestroyImage(vk_engine.device, HUD_texture, nullptr);
	vkDestroyImageView(vk_engine.device, HUD_texture_view, nullptr);
	vkFreeMemory(vk_engine.device, HUD_texture_mem, nullptr);
}

int VkRenderer::HUDComponent::CreateHUDRenderSystem(VkRenderer& vk_renderer)
{
	VulkanEngine& vk_engine = vk_renderer.vk_engine;
	VkResult result;

	// 创建用于存放HUD绘制结果的图像
	if (vk_engine.CreateImage(texture_extent.width, texture_extent.height,
		1,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&HUD_texture, &HUD_texture_mem))
	{
		SDL_Log(LOG_ERROR("Failed to Create HUD Render Image!"));
		return 1;
	}
	if (vk_engine.CreateImageView(
		HUD_texture,
		VK_FORMAT_R8G8B8A8_SRGB,
		1,
		VK_IMAGE_ASPECT_COLOR_BIT,
		&HUD_texture_view
	))
	{
		SDL_Log(LOG_ERROR("Failed to Create HUD Render Image view!"));
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully Created HUD Render Image and view!"));

	// 创建和绑定描述符集
	vk_engine.CreateDescriptorSets(vk_renderer.txt_tex_descriptor_layout, vk_engine.descriptor_pool, &HUD_texture_set);

	VkDescriptorImageInfo img_infos[1]{};
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[0].imageView = HUD_texture_view;
	img_infos[0].sampler = vk_engine.texture_sampler;

	VkWriteDescriptorSet descriptor_writes{};
	descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes.dstSet = HUD_texture_set;
	descriptor_writes.dstBinding = 0;
	descriptor_writes.dstArrayElement = 0;
	descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_writes.descriptorCount = 1;
	descriptor_writes.pImageInfo = img_infos;

	vkUpdateDescriptorSets(vk_engine.device, 1, &descriptor_writes, 0, nullptr);

	//// render pass
	VkAttachmentDescription color_attachment{};
	color_attachment.format = VK_FORMAT_R8G8B8A8_SRGB;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency[2]{};
	dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency[0].dstSubpass = 0;
	dependency[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependency[1].srcSubpass = 0;
	dependency[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependency[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkAttachmentDescription renderpass_attachments[1] = {color_attachment};
	VkRenderPassCreateInfo renderpass_info{};
	renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = 1;
	renderpass_info.pAttachments = renderpass_attachments;
	renderpass_info.subpassCount = 1;
	renderpass_info.pSubpasses = &subpass;
	renderpass_info.dependencyCount = 2;
	renderpass_info.pDependencies = dependency;
	
	result = vkCreateRenderPass(vk_engine.device, &renderpass_info, nullptr, &HUDrender_pass);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create HUD Render Pass!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created HUD Render Pass!"));

	// 给HUD渲染系统绑定好渲染通道
	HUD_render_system.render_pass = HUDrender_pass;

	//// frame buffer
	VkFramebufferCreateInfo frame_buf_info{};
	VkImageView framebuf_attachments[1] = { HUD_texture_view };
	frame_buf_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frame_buf_info.renderPass = HUDrender_pass;
	frame_buf_info.attachmentCount = 1;
	frame_buf_info.pAttachments = framebuf_attachments;
	frame_buf_info.width = texture_extent.width;
	frame_buf_info.height = texture_extent.height;
	frame_buf_info.layers = 1;

	result = vkCreateFramebuffer(vk_engine.device, &frame_buf_info, nullptr, &frame_buf);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create HUD Framebuffer! (Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully Created HUD Framebuffer! (Error code: %d)"), result);

	//// descriptor set layout
	VkDescriptorSetLayoutBinding bindings[1]{};
	VkDescriptorSetLayoutCreateInfo des_layout_info{};

	// (set=0, binding=0) texture[]
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 2;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	des_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	des_layout_info.bindingCount = 1;
	des_layout_info.pBindings = bindings;
	result = vkCreateDescriptorSetLayout(vk_engine.device, &des_layout_info, nullptr, &HUD_render_system.descriptor_layout[0]);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create text texture descriptor set layout!(Error: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Descrtiptor layout of Txt textures!"));

	//// pipeline layout
	VkPushConstantRange push_ranges[1]{};
	push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_ranges[0].offset = 0;
	push_ranges[0].size = sizeof(glm::uvec2);

	VkDescriptorSetLayout des_layouts[] = { HUD_render_system.descriptor_layout[0]};
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = des_layouts;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = push_ranges;

	result = vkCreatePipelineLayout(vk_engine.device, &layout_info, nullptr, &HUD_render_system.pipeline_layout);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to create HUD Render pipeline layout!(Error code: %d)"), result);
		return 1;
	}
	SDL_Log(LOG_SUCCESS("Successfully created Pipeline layout for HUD Render pipeline!"));

	//// pipeline
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};

	//// 着色器模块阶段
	VkShaderModule vert_shader_module;
	VkShaderModule frag_shader_module;
	if (vk_engine.CreateShaderModule("shaders/HUD_shader.vert.spv", &vert_shader_module)) { return 1; }
	if (vk_engine.CreateShaderModule("shaders/HUD_shader.frag.spv", &frag_shader_module)) { return 1; }

	// vertex shader 顶点着色器模块
	shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stages[0].module = vert_shader_module;
	shader_stages[0].pName = "main";

	// fragment shader 片段着色器模块
	shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[1].module = frag_shader_module;
	shader_stages[1].pName = "main";

	//// 动态状态
	std::array<VkDynamicState, 2> dynamic_states = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info{};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
	dynamic_state_info.pDynamicStates = dynamic_states.data();

	//// 顶点输入
	VkVertexInputBindingDescription vertex_input_binding{}; // 顶点绑定的描述
	vertex_input_binding.binding = 0;
	vertex_input_binding.stride = sizeof(VulkanEngine::TextVertex);
	vertex_input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertex_input_attribute[3]{}; // 顶点属性的描述
	vertex_input_attribute[0].binding = 0;
	vertex_input_attribute[0].location = 0;
	vertex_input_attribute[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertex_input_attribute[0].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::position);

	vertex_input_attribute[1].binding = 0;
	vertex_input_attribute[1].location = 1;
	vertex_input_attribute[1].format = VK_FORMAT_R32_UINT;
	vertex_input_attribute[1].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::tex_idx);

	vertex_input_attribute[2].binding = 0;
	vertex_input_attribute[2].location = 2;
	vertex_input_attribute[2].format = VK_FORMAT_R32_UINT;
	vertex_input_attribute[2].offset = offsetof(VulkanEngine::TextVertex, VulkanEngine::TextVertex::vertex_idx);

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding;
	vertex_input_info.vertexAttributeDescriptionCount = 3;
	vertex_input_info.pVertexAttributeDescriptions = vertex_input_attribute;

	//// 输入汇编
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 每 3 个顶点组成的三角形，不复用
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	//// 视口和裁剪矩形
	VkPipelineViewportStateCreateInfo view_port_state_info{};
	// 由于开启了动态视图和裁剪矩形，所以在记录命令缓冲区时设置视图和裁剪矩形即可
	/*view_port.x = 0.f;
	view_port.y = 0.f;
	view_port.width = (float)swapchain_extent.width;
	view_port.height = (float)swapchain_extent.height;
	view_port.minDepth = 0.f;
	view_port.maxDepth = 1.f;

	scissor.offset = {0, 0};
	scissor.extent = swapchain_extent;*/

	view_port_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	view_port_state_info.viewportCount = 1;
	//view_port_state_info.pViewports = &view_port;
	view_port_state_info.scissorCount = 1;
	//view_port_state_info.pScissors = &scissor;

	//// 光栅化器
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;  // 关闭背面剔除(VK_CULL_MODE_NONE) 开启(VK_CULL_MODE_BACK_BIT)
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.f;
	rasterizer.depthBiasClamp = 0.f;
	rasterizer.depthBiasSlopeFactor = 0.f;

	//// 多重采样
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	//// 深度和模板测试

	//// 颜色混合
	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;  // 是否启用颜色混合
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;  // 混合透明通道
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // color = src * src.alpha + dst * (1 - src.alpha)
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_state{};
	color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state.logicOpEnable = VK_FALSE;
	color_blend_state.logicOp = VK_LOGIC_OP_COPY;
	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &color_blend_attachment;
	color_blend_state.blendConstants[0] = 0.f;
	color_blend_state.blendConstants[1] = 0.f;
	color_blend_state.blendConstants[2] = 0.f;
	color_blend_state.blendConstants[3] = 0.f;

	//// 创建管线
	VkGraphicsPipelineCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
	create_info.pStages = shader_stages.data();
	// 管线功能
	create_info.pVertexInputState = &vertex_input_info;
	create_info.pInputAssemblyState = &input_assembly_info;
	create_info.pViewportState = &view_port_state_info;
	create_info.pRasterizationState = &rasterizer;
	create_info.pMultisampleState = &multisampling;
	create_info.pDepthStencilState = nullptr;
	create_info.pColorBlendState = &color_blend_state;
	create_info.pDynamicState = &dynamic_state_info;

	create_info.layout = HUD_render_system.pipeline_layout;
	create_info.renderPass = HUDrender_pass;
	create_info.subpass = 0;

	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(vk_engine.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &HUD_render_system.pipeline);
	if (result != VK_SUCCESS)
	{
		SDL_Log(LOG_ERROR("Failed to Create HUD Render PipeLine!(Error code: %d)"), result);
		return 1;
	}

	// 创建完管线后即可销毁着色器模块
	vkDestroyShaderModule(vk_engine.device, vert_shader_module, nullptr);
	vkDestroyShaderModule(vk_engine.device, frag_shader_module, nullptr);

	SDL_Log(LOG_SUCCESS("Successfully created HUD Render pipeline!"));
	return 0;
}
