#include <cstdio>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Log_about.h"
#include "VkRenderer.h"
#include "ResourcesManager.h"
#include "Camera.h"
#include "Model.h"
#include "TestModel.h"

// 全局资源管理器
ResourcesManager Global_resources_manager{};
// 全局渲染器
VkRenderer Global_vk_renderer{};

int main()
{
	SDL_Window* window;
	SDL_Event event;
	bool running = true;
	bool input_ready = false;
	//VulkanEngine vkEngine;

	Uint64 current_counter;
	Uint64 cur_fps_counter = 0;
	Uint64 last_counter = SDL_GetPerformanceCounter();
	Uint64 last_fps_counter = last_counter;
	Uint64 start_counter = last_counter;
	Uint64 counter_freq = SDL_GetPerformanceFrequency();
	double total_time = 0;
	double delta_time = 0;
	//Uint32 remain_time;
	bool is_update_HUD = false;
	int fps = 0;
	char fps_txt[8] = { 0 };

	Camera camera;

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
	//TTF_Font* font;

	//font = TTF_OpenFont("Font/ebrima.ttf", 60);

	//SDL_Surface* text_sur = TTF_RenderText_Blended_Wrapped(
		//font,
		//"0123456789",
		//10,
		//{255,255,255,255},
		//64*5
	//);
	//IMG_SavePNG(text_sur, "textures/Text_1.png");
	//SDL_DestroySurface(text_sur);
	//text_sur = TTF_RenderText_Blended(font, "FPS:", 4, {255,255,255,255});
	//IMG_SavePNG(text_sur, "textures/FPS.png");
	//SDL_DestroySurface(text_sur);

	window = SDL_CreateWindow("Vulkan Engine", 1280, 720, SDL_WINDOW_VULKAN);
	
	// 初始化和构建全局渲染器
	Global_vk_renderer.Init(window);
	// 初始化资源管理器
	Global_resources_manager.Init();

	camera.Initial(Global_vk_renderer.vk_engine);

	Model VirtualCity_model("models/gltf_test/VirtualCity/VC.gltf", Global_vk_renderer, true);
	//VirtualCity_model.trans_mat = glm::scale(glm::mat4(1.f), glm::vec3(10.f, 10.f, 10.f));
	VirtualCity_model.cur_animation = &VirtualCity_model.animations[0];
	VirtualCity_model.cur_animation->is_loop = true;
	VirtualCity_model.TriggerAnimation();

	TestModel test_model("models/gltf_test/Fox/Fox.gltf", Global_vk_renderer);
	test_model.GetModel()->trans_mat = glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f));

	//Model model_yamato("models/yamato_dmc5/scene.gltf", Global_vk_renderer);
	//model_yamato.position = glm::vec3(1.f, 0.f, 2.f);
	//model_yamato.trans_mat = glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f));

	//Model model_anon("models/anno/anno.gltf", Global_vk_renderer, true, false);

	while (running)
	{
		// 输入处理
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_KEY_UP:
				if (event.key.key == SDLK_ESCAPE)
				{
					SDL_SetWindowRelativeMouseMode(window, false);
					input_ready = false;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				SDL_SetWindowRelativeMouseMode(window, true);
				input_ready = true;
				break;
			}
			if (input_ready)
			{
				camera.GetInput(&event);
				test_model.GetInput(&event);
			}
		}

		// 更新数据
		//vkEngine.UpdateUniformBuffer(total_time);
		camera.Update(delta_time);
		memcpy(camera.uniform_map[Global_vk_renderer.vk_engine.current_frame], &camera.mvp_ubo, sizeof(camera.mvp_ubo));
		test_model.Update(delta_time);
		//model_yamato.Update(delta_time);
		VirtualCity_model.Update(delta_time);
		//SDL_Log(LOG_INFO("camera:(%.2f,%.2f,%.2f)"), camera.position.x, camera.position.y, camera.position.z);

		// 渲染绘制
		// 开始绘制
		Global_vk_renderer.BeginDraw();

		if (is_update_HUD)
		{
			Global_vk_renderer.UpdateHUD(fps_txt);
			is_update_HUD = false;
		}

		// 开始图形渲染通道
		Global_vk_renderer.BeginRenderPass();
		// 渲染绘制模型
		//Global_vk_renderer.DrawModel(VirtualCity_model, camera);
		Global_vk_renderer.DrawModel(*test_model.GetModel(), camera);
		//Global_vk_renderer.DrawModel(model_yamato, camera);
		Global_vk_renderer.DrawModel(VirtualCity_model, camera);

		// 帧率显示
		Global_vk_renderer.DrawHUD();
		
		// 结束图形渲染通道
		Global_vk_renderer.EndRenderPass();

		// 结束绘制，并呈现绘制结果
		Global_vk_renderer.EndDraw();

		fps += 1;
		//fps_txt = {};
		if (cur_fps_counter - last_fps_counter >= counter_freq / 4)
		{
			snprintf(fps_txt, 6, "%d", fps*4);
			//printf("\r%s", fps_txt);
			//fflush(stdout);
			is_update_HUD = true;
			last_fps_counter = cur_fps_counter;
			fps = 0;
		}

		// 动态延迟
		// 先等待当前帧绘制完毕
		//vkEngine.WaitPrevFrame();

		cur_fps_counter = current_counter = SDL_GetPerformanceCounter();
		delta_time = (double)((current_counter - last_counter) / (double)counter_freq);
		total_time = (double)((current_counter - start_counter) / (double)counter_freq);
		last_counter = current_counter;


		/*remain_time = (1000.0 / (double)fps) - (delta_time * 1000.0);
		if (remain_time > 0)
		{
			SDL_Delay(remain_time);
		}*/

	}

	// 等待设备的任务全都结束再进行释放
	vkDeviceWaitIdle(Global_vk_renderer.vk_engine.device);

	VirtualCity_model.DestroyGPUData();
	test_model.DestroyGPUData();
	//model_yamato.DestroyGPUData();
	camera.Destroy(Global_vk_renderer.vk_engine);

	Global_resources_manager.Destroy();
	Global_vk_renderer.Destroy();

	SDL_DestroyWindow(window);
	//TTF_CloseFont(font);
	//TTF_Quit();
	SDL_Quit();

	return 0;
}