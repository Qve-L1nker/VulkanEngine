//#pragma once
#ifndef _MODEL_H
#define _MODEL_H

#include <string>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "Animation.h"

struct aiBone;
class VulkanEngine;
class VkRenderer;

class ModelTexture
{
public:
	ModelTexture() = default;
	~ModelTexture() = default;

	ModelTexture(const ModelTexture& obj)
	{
		id = obj.id;
		img = obj.img;
		view = obj.view;
		memory = obj.memory;
		miplevels = obj.miplevels;
		ref_count = obj.ref_count;
	}
	inline ModelTexture& operator=(const ModelTexture& obj)
	{
		id = obj.id;
		img = obj.img;
		view = obj.view;
		memory = obj.memory;
		miplevels = obj.miplevels;
		ref_count = obj.ref_count;
		return *this;
	};

	/**
	 * @brief 判断texture对象是否加载成功
	 * @return 只有当img, view, memory都被成功赋值才返回true，否则返回false
	 */
	inline bool IsLoad() const
	{
		return img && view && memory;
	}

	/** hash(tex path) |    color
	 *   00 00 00 00   | 00 00 00 00
	 * heigh <------------------- low
	 * e.g.:
	 * 0x01ef8c21|00000000 - texture image, no color
	 * 0x00000000|00ff00ff - texture of green color(rgba:0 255 0 255)
	**/ 
	uint64_t id = 0;

	VkImage img = nullptr;
	VkImageView view = nullptr;
	VkDeviceMemory memory = nullptr;
	uint32_t miplevels = 1;
	// 若一个模型没有某种纹理，则会把该标志位置为true，因为没有的纹理一般会用其他的纹理来占位，可能会导致多次释放
	uint32_t ref_count = 0;

private:

};


class ModelMaterial
{
public:
	ModelMaterial() = default;
	~ModelMaterial() = default;

	ModelMaterial(const ModelMaterial& obj)
	{
		diffuse = obj.diffuse;
		specular = obj.specular;
		ambient = obj.ambient;
		emissive = obj.emissive;
		normal = obj.normal;
		metallicRoughness = obj.metallicRoughness;
		descriptor_set = obj.descriptor_set;
	}
	ModelMaterial& operator=(const ModelMaterial& obj)
	{
		diffuse = obj.diffuse;
		specular = obj.specular;
		ambient = obj.ambient;
		emissive = obj.emissive;
		normal = obj.normal;
		metallicRoughness = obj.metallicRoughness;
		descriptor_set = obj.descriptor_set;
		return *this;
	}

	/**
	 * @brief 更新设置纹理描述符集的资源
	 * @param vkEngine - Vulkan渲染引擎
	 */
	void UpdateDescriptorSets();

	// 释放销毁材质以及未被引用的纹理纹理
	void Destroy();

	//// 材质对应纹理

	// 漫反射纹理
	ModelTexture* diffuse = nullptr;
	// 镜面反射纹理
	ModelTexture* specular = nullptr;
	// 环境光纹理
	ModelTexture* ambient = nullptr;

	// 自发光纹理
	ModelTexture* emissive = nullptr;
	// 法线贴图
	ModelTexture* normal = nullptr;
	// 金属度和粗糙度
	ModelTexture* metallicRoughness = nullptr;

	// 材质的纹理描述符集
	VkDescriptorSet descriptor_set = nullptr;

private:

};


class ModelMesh
{
public:
	ModelMesh() = default;
	~ModelMesh() = default;

	ModelMesh(const ModelMesh& obj);
	ModelMesh& operator=(const ModelMesh&) = delete;

	// 利用Vulkan Engine释放数据
	void DestroyGPUData() const;

	/**
	 * @brief 检查是否有texture对象
	 * @return 若texture被成功赋值则返回true，否则返回false
	 */
	inline bool HasTexture() const
	{
		return material->diffuse->IsLoad() && material->specular->IsLoad() && material->ambient->IsLoad();
	}

	//std::string rootdir;

	// 网格顶点数据
	//std::vector<VulkanEngine::Vertex> vertices{};
	VkBuffer vertex_buf = nullptr;
	VkDeviceMemory vertex_buf_memory = nullptr;

	// 顶点索引数据
	//std::vector<uint32_t> indices{};
	VkBuffer index_buf = nullptr;
	VkDeviceMemory index_buf_memory = nullptr;
	uint32_t indices_size = 0;

	uint32_t vertex_flags = 0;

	// 网格对应材质纹理的索引
	ModelMaterial* material = nullptr;

	// 节点和动画的叠加变换矩阵
	glm::mat4 node_and_anim_matrix{1.f};

	// 是否有骨骼绑定的标记
	bool have_bones = false;

private:

};

class ModelAnimation
{
public:
	struct Channel
	{
		// 该轨道的动画数据
		Animation3D anim3D{};
		// 该轨道所涉及到的节点
		uint32_t node_idx = -1;

		// 该轨道的动画在数组中的索引
		uint32_t anim_idx = -1;
		// 该轨道的索引
		uint32_t channel_idx = -1;

		Channel(): anim3D(), node_idx(-1), anim_idx(-1), channel_idx(-1) {};
		Channel(const Channel& obj) = default;
		Channel& operator=(const Channel& obj) = default;
	};

public:
	ModelAnimation() = default;
	~ModelAnimation() = default;

	ModelAnimation(const ModelAnimation& obj)
	{
		channels = {};
	}
	ModelAnimation& operator=(const ModelAnimation&) = delete;

	// 对动画的每个轨道进行帧更新，若所有轨道都播放结束且没有循环，则返回1
	int Update(double delta_time);

	// 开始动画
	void Begin();
	// 结束动画
	void End();

	// 重置动画
	void Reset();

	// 模型动画的轨道数组
	std::vector<Channel> channels;
	// 动画是否被触发，被触发的动画才会进行每帧更新
	bool is_trigger = false;
	// 动画是否循环
	bool is_loop = false;

	// 该动画在动画数组中的索引
	uint32_t anim_idx = -1;

	// 动画更新时需要更新的轨道队列
	std::vector<Channel*> update_queue{};

private:


};

// 模型骨骼组件
class ModelBoneComponent
{
public:
	struct ModelBone
	{
		// 骨骼id
		uint32_t boneid = -1;
		// 骨骼的偏移矩阵的指针，指向骨骼矩阵数组中对应的矩阵
		glm::mat4 offset_mat{1.f};
	};

public:
	ModelBoneComponent() = default;
	~ModelBoneComponent() = default;

	ModelBoneComponent(const ModelBoneComponent&) = delete;
	ModelBoneComponent& operator=(const ModelBoneComponent&) = delete;

	/**
	 * @brief 创建统一缓冲区描述符集
	 * @param vk_renderer - 渲染器
	 * @return 成功返回0，否则返回1
	 */
	int CreateDescriptSet();

	// 将所有骨骼矩阵同步更新到统一缓冲区中
	void SyncBonesMatrixToUBO(uint8_t vkEngine_cur_frame);

	// 销毁GPU缓冲区数据
	void DestroyGPUData();

	// 骨骼数组，会根据模型是否有骨骼来创建
	std::vector<ModelBone> bones{};
	// 骨骼矩阵讲过节点叠加后的最终矩阵数组，方便复制到统一缓冲区中上传给GPU
	std::vector<glm::mat4> final_matrix{};

	// 骨骼矩阵统一缓冲区
	VkBuffer uniform_buf[2]{};
	// 骨骼矩阵统一缓冲区的内存(GPU中)
	VkDeviceMemory uniform_buf_memory[2]{};
	// 骨骼矩阵统一缓冲区内存在CPU内存中的持久映射
	void* uniform_map[2]{};
	// 骨骼矩阵数组的描述符集
	VkDescriptorSet bones_matrix_set[2]{};

private:

};

class Model
{
public:
	struct Node
	{
		glm::mat4 matrix;
		std::vector<uint32_t> meshs;
		std::vector<uint32_t> Childrens;

		uint32_t anim_idx;
		uint32_t channel_idx;
		uint32_t bones_idx;

		uint32_t nodeid;
		uint32_t parent;

		Node(const glm::mat4& matrix, uint32_t nodeid, uint32_t parent) :
			matrix(matrix), meshs(), Childrens(), anim_idx(-1), channel_idx(-1), bones_idx(-1), nodeid(nodeid), parent(parent) { }
	};

public:
	Model() = default;
	Model(const char* meshfile, const VkRenderer& vk_renderer, bool is_print_information = false, bool is_create_mesh = true);
	~Model() = default;

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	void Update(double delata_time);

	/**
	 * @brief 根据节点id在层级结构树中查找节点
	 * @param node_id - 通过Assimp中的aiNode的`aiString name`计算(std::hash<aiString>())得到的哈希值，也就意味着该函数仅可在加载模型的过程中使用
	 * @return 返回对应id的节点的索引
	 * @return 若返回-1，则说明不存在这个id的节点
	 * @return 若返回-2，则表示当前模型节点树为空
	 */
	uint32_t FindStructTreeNode(uint32_t node_id);

	/**
	 * @brief 在骨骼数组中查找骨骼，通过aiBone的mName计算得到的hash值查找，若不存在则创建一个
	 * @param bone - Assimp加载的scene中的aiBone指针
	 * @return 返回对应id的骨骼的索引
	 * @return 若返回-2，则说明当前模型没有骨骼
	 */
	uint32_t FindBone(aiBone* bone);

	/**
	 * @brief 给节点树的某个节点递归计算模型空间矩阵和动画插值矩阵，并更新给该节点和该节点的所有子节点的网格
	 * 或给整个节点树递归计算模型空间矩阵和动画插值矩阵，并更新给所有子节点的网格
	 * @param node - 要设置的节点，可通过FindStructTreeNide()来查找，一般传入根节点(structTree[0])来遍历整个节点树
	 */
	void UpdateStructTreeNodeAndAnimMatrix(Node& node);

	// 触发动画
	void TriggerAnimation();

	// 结束动画
	void EndAnimation();

	// 利用Vulkan Engine释放数据
	void DestroyGPUData();


	// 模型的根目录
	std::string rootdir;

	// 模型层级结构树(将所有节点放在内存连续的数组中，节点之间的连接用数组索引来代替指针)
	std::vector<Node> structTree{};

	// 顶点网格数组
	std::vector<ModelMesh> vertex_meshs;

	// 材质数组
	std::vector<ModelMaterial> materials;

	// 动画数组
	std::vector<ModelAnimation> animations;

	// 模型纹理池
	std::vector<ModelTexture> textures_pool;

	// 骨骼组件，会根据模型是否有骨骼来创建
	ModelBoneComponent* bone_component = nullptr;

	// 模型变换矩阵，包含模型缩放，位置坐标偏移
	glm::mat4 trans_mat{1.f};
	glm::vec3 position{ 0.f,0.f,0.f };
	// 当前动画状态
	ModelAnimation* cur_animation = nullptr;

private:

};

#endif // !_MODEL_H