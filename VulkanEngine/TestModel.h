//#pragma once
#ifndef _TEST_MODEL_H
#define _TEST_MODEL_H

#include <SDL3/SDL.h>
#include "Model.h"

class TestModel
{
public:
	TestModel() = default;
	TestModel(const char* meshfile, const VkRenderer& vk_renderer);
	~TestModel();

	TestModel(const TestModel&) = delete;
	TestModel& operator=(const TestModel&) = delete;

	void GetInput(const SDL_Event* event);
	void Update(double delata_time);

	void DestroyGPUData();

	inline Model* const GetModel()
	{
		return model;
	}

private:
	Model* model = nullptr;
};


#endif // !_TEST_MODEL_H