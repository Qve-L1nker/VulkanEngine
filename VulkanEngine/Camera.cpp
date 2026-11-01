#include <SDL3/SDL.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Log_about.h"
#include "VulkanEngine.h"
#include "Camera.h"

int Camera::Initial(VulkanEngine& vkEngine)
{
	mvp_ubo.model = glm::mat4(1.f);
	mvp_ubo.view = glm::mat4(1.f);
	// 选择使用垂直视野为 45 度的透视投影。其他参数是宽高比、近和远视图平面。
	// 重要的是使用当前交换链范围来计算宽高比，以考虑调整大小后窗口的新宽度和高度。
	mvp_ubo.proj = glm::perspective(glm::radians(45.f), 1280.f / 720.f, 0.1f, 2000.f);
	// GLM 最初是为 OpenGL 设计的，其中裁剪坐标的 Y 坐标是反转的。
	// 补偿这种情况的最简单方法是翻转投影矩阵中 Y 轴缩放因子的符号。如果不这样做，则图像将倒置渲染。
	mvp_ubo.proj[1][1] *= -1;

	for (int i=0; i < 2; i++)
	{
		//// 创建MVP矩阵缓冲区
		VkDeviceSize buf_size = sizeof(VulkanEngine::MVPMatrix);

		if (vkEngine.CreateBuffer(buf_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniform_buf[i], &uniform_buf_memory[i]))
		{
			SDL_Log(LOG_ERROR("Failed to create MVP uniform buffer!"));
			return 1;
		}
		vkMapMemory(vkEngine.device, uniform_buf_memory[i], 0, buf_size, 0, &uniform_map[i]);

		//// 创建描述符集
		vkEngine.CreateDescriptorSets(vkEngine.one_matrix_descriptor_layout, vkEngine.descriptor_pool, &MVPubo_descriptor_set[i]);

		VkDescriptorBufferInfo buf_info{};
		buf_info.buffer = uniform_buf[i];
		buf_info.offset = 0;
		buf_info.range = sizeof(VulkanEngine::MVPMatrix);

		VkWriteDescriptorSet descriptor_writes{};
		descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes.dstSet = MVPubo_descriptor_set[i];
		descriptor_writes.dstBinding = 0;
		descriptor_writes.dstArrayElement = 0;
		descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes.descriptorCount = 1;
		descriptor_writes.pBufferInfo = &buf_info;
		//descriptor_writes.pImageInfo = nullptr;
		//descriptor_writes.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(vkEngine.device, 1, &descriptor_writes, 0, nullptr);
	}
	
	return 0;
}

void Camera::GetInput(const SDL_Event* event)
{
	switch (event->type)
	{
	case SDL_EVENT_MOUSE_MOTION:
		MouseMoveAction(event->motion.xrel, -event->motion.yrel);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		MouseWheelAction(event->wheel.y);
		break;
	case SDL_EVENT_KEY_UP:
		switch (event->key.key)
		{
		case SDLK_W:
			W_key_status = false;
			break;
		case SDLK_A:
			A_key_status = false;
			break;
		case SDLK_S:
			S_key_status = false;
			break;
		case SDLK_D:
			D_key_status = false;
			break;
		case SDLK_Q:
			Q_key_status = false;
			break;
		case SDLK_E:
			E_key_status = false;
			break;
		default:
			break;
		}
		break;
	case SDL_EVENT_KEY_DOWN:
		switch (event->key.key)
		{
		case SDLK_W:
			W_key_status = true;
			break;
		case SDLK_A:
			A_key_status = true;
			break;
		case SDLK_S:
			S_key_status = true;
			break;
		case SDLK_D:
			D_key_status = true;
			break;
		case SDLK_Q:
			Q_key_status = true;
			break;
		case SDLK_E:
			E_key_status = true;
			break;
		default:
			break;
		}
		break;
	}
}

void Camera::Update(double delta_time)
{
	// 更新绑定点的坐标
	MoveAction(nullptr);
	bind_point += move_vector * (move_speed * (float)delta_time);
	position += move_vector * (move_speed * (float)delta_time);
	// 更新摄像机的世界坐标
	position = bind_point + glm::normalize(position - bind_point) * cur_rod_long;
	// 对于视图转换，决定从摄像机位置看向摄像机的绑定点位置。glm::lookAt()函数采用眼睛位置、中心位置和向上轴作为参数。
	mvp_ubo.view = glm::lookAt(position, bind_point, glm::vec3(0.f, 0.f, 1.f));
}

void Camera::Destroy(VulkanEngine& vkEngine)
{
	// 销毁缓冲区
	for (int i=0; i < 2; i++)
	{
		vkDestroyBuffer(vkEngine.device, uniform_buf[i], nullptr);
		vkFreeMemory(vkEngine.device, uniform_buf_memory[i], nullptr);
	}
}

void Camera::MouseMoveAction(float x, float y)
{
	glm::vec3 zm = glm::normalize(bind_point - position);
	glm::vec3 xm = glm::normalize(glm::cross(zm, glm::vec3(0.f, 0.f, 1.f)));
	glm::vec3 ym = glm::normalize(glm::cross(xm, zm));
	glm::mat3 trans_mat{xm, ym, zm};
	glm::vec3 result = trans_mat * (glm::vec3(x, y, 0.f) * 0.1f);

	/*if (glm::abs(glm::dot(glm::normalize(bind_point - result), glm::vec3(0.f, 0.f, 1.f))) >= 0.9f)
	{ 
		SDL_Log(LOG_ERROR("Will Trigger universal joint lock!"));
		return;
	}*/

	position -= result;
}

void Camera::MouseWheelAction(float size)
{
	float temp = cur_rod_long;

	temp -= size * 0.1f;
	if (temp <= 0 || temp >= rod_long)
	{
		return;
	}
	cur_rod_long = temp;
}

void Camera::MoveAction(const SDL_Event* event)
{
	glm::vec3 front = (bind_point - position) / cur_rod_long;
	move_vector = glm::vec3(0.f);

	if (W_key_status) { move_vector += front; }
	if (A_key_status) { move_vector += glm::cross(glm::vec3(0.f, 0.f, 1.f), front); }
	if (S_key_status) { move_vector -= front; }
	if (D_key_status) { move_vector -= glm::cross(glm::vec3(0.f, 0.f, 1.f), front); }
	if (Q_key_status) { move_vector += glm::vec3(0.f, 0.f, 1.f); }
	if (E_key_status) { move_vector += glm::vec3(0.f, 0.f, -1.f); }

	if (glm::length(move_vector) != 0.f)
	{
		move_vector = glm::normalize(move_vector);
	}
}
