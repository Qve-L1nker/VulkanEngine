//#pragma once
#ifndef _GLOBAL_OBJECT_H
#define _GLOBAL_OBJECT_H

#include "ResourcesManager.h"
#include "VkRenderer.h"

/*
 * 所有的全局变量的声明
**/

extern ResourcesManager Global_resources_manager;
extern VkRenderer Global_vk_renderer;

inline const ResourcesManager& GlobalResourcesManager()
{
	return Global_resources_manager;
}

inline const VkRenderer& GlobalVkRenderer()
{
	return Global_vk_renderer;
}

#endif // !_GLOBAL_OBJECT_H
