#include "TestModel.h"

TestModel::TestModel(const char* meshfile, const VkRenderer& vk_renderer)
{
	model = new Model(meshfile, vk_renderer, true);
	if (!model->animations.empty())
	{
		model->cur_animation = &model->animations[0];
	}
}

TestModel::~TestModel()
{
	if (model)
	{
		delete model;
	}
}

void TestModel::GetInput(const SDL_Event* event)
{
	if (event->type == SDL_EVENT_KEY_UP)
	{
		if (event->key.key == SDLK_Z && model->cur_animation)
		{
			model->cur_animation = &model->animations[0];
			model->TriggerAnimation();
			return;
		}
		if (event->key.key == SDLK_X && model->cur_animation)
		{
			model->cur_animation = &model->animations[1];
			model->TriggerAnimation();
			return;
		}
		if (event->key.key == SDLK_C && model->cur_animation)
		{
			model->cur_animation = &model->animations[2];
			model->TriggerAnimation();
			return;
		}
	}
}

void TestModel::Update(double delata_time)
{
	model->Update(delata_time);
}

void TestModel::DestroyGPUData()
{
	model->DestroyGPUData();
}
