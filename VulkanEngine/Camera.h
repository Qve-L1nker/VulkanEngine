//#pragma once
#ifndef _CAMERA_H
#define _CAMERA_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "VulkanEngine.h"

class Camera
{
public:
	Camera() = default;
	~Camera() = default;
	Camera(const Camera&) = delete;
	Camera& operator=(const Camera&) = delete;

	int Initial(VulkanEngine& vkEngine);
	void GetInput(const SDL_Event* event);
	void Update(double delta_time);
	void Destroy(VulkanEngine& vkEngine);

	// mvp投影矩阵
	VulkanEngine::MVPMatrix mvp_ubo;
	// 投影矩阵统一缓冲区
	VkBuffer uniform_buf[2]{};
	// 投影矩阵统一缓冲区的内存(GPU中)
	VkDeviceMemory uniform_buf_memory[2]{};
	// 投影矩阵统一缓冲区内存在CPU内存中的持久映射
	void* uniform_map[2]{};
	// MVP投影矩阵描述符集
	VkDescriptorSet MVPubo_descriptor_set[2]{};

	// 位置坐标
	glm::vec3 position{0.f, -10.f, 0.f};
	// 视线方向
	//glm::vec3 sight{1.f, 0.f, 0.f};
	// 绑定点
	glm::vec3 bind_point{0.f, 0.f, 0.f};
	// 摄像机杆长度(固定)
	const float rod_long = 50.f;
	float cur_rod_long = 10.f;

private:
	// 鼠标移动动作
	void MouseMoveAction(float x, float y);
	// 鼠标滚轮动作
	void MouseWheelAction(float size);

	// 摄像机移动动作，移动的是绑定点
	void MoveAction(const SDL_Event* event);

	// 移动的方向
	glm::vec3 move_vector{0.f};
	// 移动速度
	float move_speed = 10.f;

	// 交互按键的状态
	bool W_key_status = false;
	bool A_key_status = false;
	bool S_key_status = false;
	bool D_key_status = false;
	bool Q_key_status = false;
	bool E_key_status = false;

};

#endif // !_CAMERA_H