# VulkanEngine

一个基于 [*SDL3*](https://github.com/libsdl-org/SDL)相关库、[*Vulkan*](https://www.vulkan.org/)图形库、[*glm*](https://github.com/icaven/glm)数学库、[*Assimp*](https://github.com/assimp/assimp)模型加载库 的简单3D渲染器

## 功能

- 实现了基础的Vulkan实例、物理和逻辑设备、交换链、队列句柄的创建；渲染通道、图形管线的配置与构建；缓冲区资源、描述符集的创建与绑定；纹理的加载与采样(图像加载由*SDL3_image*库负责)；3D模型的加载(*Assimp*库)与顶点、索引缓冲区的创建；CPU 与 GPU 之间的同步机制
- 实现了模型节点动画和骨骼蒙皮动画效果
- 在通用模型的片段着色器`general_3Dmodel_shader.frag`中实现了**PBR**渲染的部分
- 实现了对法线贴图的支持
