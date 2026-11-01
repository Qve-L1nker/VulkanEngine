//#pragma once
#ifndef _RESOURCES_MANAGER_H
#define _RESOURCES_MANAGER_H

#include "Model.h"

struct aiScene;

class VkRenderer;

// 资源管理器
class ResourcesManager
{
public:
	ResourcesManager() = default;
	~ResourcesManager() = default;

	ResourcesManager(const ResourcesManager&) = delete;
	ResourcesManager& operator=(const ResourcesManager&) = delete;

	// 初始化
	int Init();

	void Destroy();

	/**
	 * @brief 模型加载函数
	 * @param objfile - 模型文件的路径(const char*类型)
	 * @param module - 负责存放模型数据(网格，材质...)
	 * @param vk_renderer - 创建好的渲染器
	 * @param is_create_data - 仅查看信息标志，是否要创建模型相关数据(可用于查看模型信息但不创建顶点缓冲区等数据，减轻debug时的内存占用)
	 * @return 成功返回0，否则返回1
	 */
	int LoadModel(const char* objfile, Model& model, const VkRenderer& vk_renderer, bool is_create_data, bool is_print_information);

	ModelTexture WhiteTexture{};
	ModelTexture BlackTexture{};

private:
	/**
	 * @brief 模型网格加载函数
	 * @param scene - Assimp加载模型文件得到的aiScene
	 * @param model - 该网格所属的模型
	 * @param idx - 要加载的网格在scene中的索引
	 * @param vk_renderer - 创建好的渲染器
	 * @param objfile - 模型的文件路径
	 * @return 成功返回0，否则返回1
	 */
	int LoadMesh(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile);
	// 有骨骼的模型的网格加载函数
	int LoadMeshWithBone(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile);
	// 有法线贴图材质，即需要切线的网格加载函数
	int LoadMeshWithTangent(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile);
	// 有骨骼且需要切线的网格加载函数
	int LoadMeshWithBoneAndTangent(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile);

	uint32_t CountTexturesOfAllMaterial(const aiScene* scene, Model& model);

	/**
	 * @brief 材质的纹理加载函数
	 * @param material - 要加载的材质
	 * @param tex_color - 在加载纹理之前，先获取该纹理的固定颜色
	 * @param type - 纹理的类型(diffuse,specular,ambient,emissive)
	 * @param model_rootdir - 模型的根目录，用于通过路径加载纹理
	 * @param vk_renderer - 创建好的渲染器
	 * @return 返回从纹理中得到的纹理索引(返回-2为黑模，-1为白模)，若纹理池中没有则会创建并放入池中
	 */
	int LoadMaterialTexture(
		const aiMaterial* material,
		aiColor4D& tex_color,
		aiTextureType type,
		Model& model,
		const VkRenderer& vk_renderer
	);

	/**
	 * @brief 网格材质加载函数
	 * @param scene - Assimp加载模型文件得到的aiScene
	 * @param idx - 要加载的材质在scene中的索引
	 * @param module_material - 负责存放模型材质的纹理
	 * @param module_rootdir - 模型的根目录
	 * @param vk_renderer - 创建好的渲染器
	 * @return 成功返回0，否则返回1
	 */
	int LoadMaterial(const aiScene* scene, uint32_t idx, ModelMaterial& model_material, Model& model, const VkRenderer& vk_renderer);

	/**
	 * @brief 模型自带的动画加载函数
	 * @param scene - Assimp加载模型文件得到的aiScene
	 * @param module - 要加载动画的模型
	 * @param idx - 要加载的动画在scene中的索引
	 * @return 成功返回0，否则返回1
	 */
	int LoadAnimation(const aiScene* scene, Model& model, uint32_t idx);

	/**
	 * @brief 遍历模型层级结构树，来获取每个网格自己的变换矩阵
	 * @param scene - Assimp加载的aiScene
	 * @param module - 模型的引用
	 * @return 成功返回0，否则返回1
	 */
	int GetNodeTreeMatrix(const aiScene* scene, Model& model);

	/**
	 * @brief 输出由Assimp加载的模型的信息
	 * @param scene - Assimp加载模型文件得到的aiScene*
	 * @param mesh_dir - Assimp加载的模型的目录
	 * @param path - Assimp加载的模型的路径
	 */
	void OutputMeshInformation(const aiScene* scene, const std::string& mesh_dir, const char* path);

};

// 对Assimp里的aiString字符串结构体构造一个哈希函数
template <>
struct std::hash<aiString>
{
	// DJBHash 字符串哈希函数
	uint32_t operator()(const aiString& str)
	{
		uint32_t hash = 5381;

		for (uint32_t i = 0; i < str.length; i++)
		{
			hash += (hash << 5) + str.data[i];
		}

		return (hash & 0x7fffffff);
	}
};

#endif // !_RESOURCES_MANAGER_H