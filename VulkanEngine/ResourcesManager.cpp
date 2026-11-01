#include <vector>
#include <unordered_set>
#include <algorithm>

// assimp(模型加载库)
#ifdef _MSC_VER
#include <assimp/Importer.hpp>
#else
#include <assimp/cimport.h>
#endif
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vulkan/vulkan.h>

#include <SDL3/SDL_log.h>

#include "Model.h"
#include "VkRenderer.h"
#include "AssimpToGlm.h"
#include "Log_about.h"
#include "ResourcesManager.h"

#include "GlobalObject.h"

int ResourcesManager::Init()
{
	Global_vk_renderer.vk_engine.Create1x1PixelTexture(255, 255, 255, 255, &WhiteTexture.img, &WhiteTexture.view, &WhiteTexture.memory);
	Global_vk_renderer.vk_engine.Create1x1PixelTexture(0, 0, 0, 255, &BlackTexture.img, &BlackTexture.view, &BlackTexture.memory);
	WhiteTexture.ref_count = -1;
	BlackTexture.ref_count = -1;

	return 0;
}

void ResourcesManager::Destroy()
{
	Global_vk_renderer.vk_engine.DestroyModelTexture(WhiteTexture);
	Global_vk_renderer.vk_engine.DestroyModelTexture(BlackTexture);
}

int ResourcesManager::LoadModel(const char* objfile, Model& model, const VkRenderer& vk_renderer, bool is_create_data, bool is_print_information)
{
#ifdef _MSC_VER
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(objfile,
		aiProcess_Triangulate |           // 把多边形三角形化
		aiProcess_FlipUVs |               // 翻转 UV 的 Y 轴坐标
		aiProcess_JoinIdenticalVertices | // 顶点去重
		aiProcess_GenNormals |            // 如果没有法线就自动生成
		aiProcess_CalcTangentSpace        // 自动计算切线
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		SDL_Log(LOG_ERROR("Assimp Load Mesh error!: %s"), importer.GetErrorString());
		return 1;
	}
#else
	const aiScene* scene = aiImportFile(objfile,
		aiProcess_Triangulate |           // 把多边形三角形化
		aiProcess_FlipUVs |               // 翻转 UV 的 Y 轴坐标
		aiProcess_JoinIdenticalVertices | // 顶点去重
		aiProcess_GenNormals |            // 如果没有法线就自动生成
		aiProcess_CalcTangentSpace        // 自动计算切线
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		SDL_Log(LOG_ERROR("Assimp Load Mesh error!: %s"), aiGetErrorString());
		return 1;
	}
#endif

	model.rootdir = std::string(objfile);
	model.rootdir = model.rootdir.substr(0, model.rootdir.rfind("/") + 1);

	// 输出模型具体信息
	if (is_print_information)
	{
		OutputMeshInformation(scene, model.rootdir, objfile);
	}

	// 如果仅查看信息标志为false，就直接退出
	if (!is_create_data) { return 0; }

	//// 遍历层级结构，叠加给网格的变换矩阵
	GetNodeTreeMatrix(scene, model);

	//// 获取材质纹理
	model.materials.resize(scene->mNumMaterials);
	// 先统计纹理的数量来设置好纹理池的容量
	model.textures_pool.reserve((size_t)CountTexturesOfAllMaterial(scene, model));
	for (uint32_t i = 0; i < scene->mNumMaterials; i++)
	{
		LoadMaterial(scene, i, model.materials[i], model, vk_renderer);
	}

	//// 获取网格数据
	model.vertex_meshs.resize(scene->mNumMeshes);
	for (uint32_t i = 0; i < scene->mNumMeshes; i++)
	{
		ModelMesh& mesh = model.vertex_meshs[i];

		// 绑定材质
		mesh.material = &model.materials[scene->mMeshes[i]->mMaterialIndex];

		// 创建有骨骼绑定的网格
		if (scene->mMeshes[i]->mNumBones)
		{
			mesh.vertex_flags |= 1;
			if (!model.bone_component)
			{
				model.bone_component = new ModelBoneComponent;
			}

			if (mesh.material->normal != &BlackTexture)
			{
				// 如果有法线贴图，则需要给顶点加上切线
				mesh.vertex_flags |= 0x10000;
				LoadMeshWithBoneAndTangent(scene, model, i, vk_renderer, objfile);
			}
			else
			{
				LoadMeshWithBone(scene, model, i, vk_renderer, objfile);
			}
		}
		// 创建没有骨骼绑定的网格
		else
		{
			if (mesh.material->normal != &BlackTexture)
			{
				// 如果有法线贴图，则需要给顶点加上切线
				mesh.vertex_flags |= 0x10000;
				LoadMeshWithTangent(scene, model, i, vk_renderer, objfile);
			}
			else
			{
				LoadMesh(scene, model, i, vk_renderer, objfile);
			}
		}
	}
	if (model.bone_component)
	{
		// 给骨骼组件创建统一缓冲区描述符集
		model.bone_component->CreateDescriptSet();
		// 准备好要存放叠加好的最终矩阵的数组
		model.bone_component->final_matrix.resize(model.bone_component->bones.size());
	}

	//// 获取动画
	model.animations.resize(scene->mNumAnimations);
	for (uint32_t i = 0; i < scene->mNumAnimations; i++)
	{
		LoadAnimation(scene, model, i);
	}

	//// 未被任何网格引用的材质可以省略掉
	std::vector<uint32_t> material_ref_count{};
	material_ref_count.resize(model.materials.size());

	for (uint32_t i = 0; i < scene->mNumMeshes; i++)
	{
		material_ref_count[scene->mMeshes[i]->mMaterialIndex] += 1;
	}
	for (uint32_t i = 0; i < material_ref_count.size(); i++)
	{
		if (!material_ref_count[i])
		{
			model.materials[i].Destroy();
			model.materials.erase(model.materials.begin() + i);
		}
	}

	return 0;
}

int ResourcesManager::LoadMesh(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile)
{
	VkDeviceSize buf_size;
	const VulkanEngine& vk_engine = vk_renderer.vk_engine;
	ModelMesh& model_mesh = model.vertex_meshs[idx];

	aiMesh* mesh = scene->mMeshes[idx];
	//// 顶点
	std::vector<VulkanEngine::Vertex> vertices;
	vertices.reserve(mesh->mNumVertices);
	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		// 位置坐标
		glm::vec3&& pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		// 法线
		glm::vec3&& nor = (mesh->HasNormals()) ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// UV坐标
		glm::vec2&& UV = (mesh->HasTextureCoords(0)) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.f, 0.f);

		vertices.push_back({ pos, nor, UV });
	}
	buf_size = sizeof(VulkanEngine::Vertex) * vertices.size();

	// 创建顶点缓冲区
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.vertex_buf, &model_mesh.vertex_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}
	if (vk_engine.Write2BufferByStage(model_mesh.vertex_buf, vertices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write vertex data to vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}


	//// 面索引
	std::vector<uint32_t> indices{};
	indices.reserve(mesh->mNumFaces);
	for (uint32_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace& face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	model_mesh.indices_size = (uint32_t)indices.size();


	// 创建索引缓冲区
	buf_size = sizeof(uint32_t) * indices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.index_buf, &model_mesh.index_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	if (vk_engine.Write2BufferByStage(model_mesh.index_buf, indices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write index data to index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	return 0;
}

int ResourcesManager::LoadMeshWithBone(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile)
{
	VkDeviceSize buf_size;
	const VulkanEngine& vk_engine = vk_renderer.vk_engine;
	ModelMesh& model_mesh = model.vertex_meshs[idx];

	aiMesh* mesh = scene->mMeshes[idx];
	//// 顶点
	std::vector<VulkanEngine::BoneVertex> vertices{};
	vertices.reserve(mesh->mNumVertices);
	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		// 位置坐标
		glm::vec3&& pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		// 法线
		glm::vec3&& nor = (mesh->HasNormals()) ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// UV坐标
		glm::vec2&& UV = (mesh->HasTextureCoords(0)) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.f, 0.f);

		vertices.push_back({
			pos, nor, UV ,
			{ {0,0,0,0},{0.f,0.f,0.f,0.f} }
			});
	}

	//// 创建骨骼数组，并给顶点复制骨骼权重
	for (uint32_t i = 0; i < mesh->mNumBones; i++)
	{
		aiBone* bone = mesh->mBones[i];
		uint32_t boneidx = model.FindBone(bone);

		uint32_t nodeidx = model.FindStructTreeNode(model.bone_component->bones[boneidx].boneid);
		model.structTree[nodeidx].bones_idx = boneidx;

		for (uint32_t w = 0; w < bone->mNumWeights; w++)
		{
			auto& vertex = vertices[bone->mWeights[w].mVertexId];
			uint8_t w_idx = vertex.bones.FindMinWeight();
			if (vertex.bones.weights[w_idx] < bone->mWeights[w].mWeight)
			{
				vertex.bones.boneidx[w_idx] = boneidx;
				vertex.bones.weights[w_idx] = bone->mWeights[w].mWeight;
			}
		}
	}

	// 创建顶点缓冲区
	buf_size = sizeof(VulkanEngine::BoneVertex) * vertices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.vertex_buf, &model_mesh.vertex_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}
	if (vk_engine.Write2BufferByStage(model_mesh.vertex_buf, vertices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write vertex data to vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}


	//// 面索引
	std::vector<uint32_t> indices{};
	indices.reserve(mesh->mNumFaces);
	for (uint32_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace& face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	model_mesh.indices_size = (uint32_t)indices.size();


	// 创建索引缓冲区
	buf_size = sizeof(uint32_t) * indices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.index_buf, &model_mesh.index_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	if (vk_engine.Write2BufferByStage(model_mesh.index_buf, indices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write index data to index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	return 0;
}

int ResourcesManager::LoadMeshWithTangent(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile)
{
	VkDeviceSize buf_size;
	const VulkanEngine& vk_engine = vk_renderer.vk_engine;
	ModelMesh& model_mesh = model.vertex_meshs[idx];

	aiMesh* mesh = scene->mMeshes[idx];
	//// 顶点
	// 有法线贴图则使用含切线的顶点
	std::vector<VulkanEngine::TangentVertex> vertices{};
	vertices.reserve(mesh->mNumVertices);
	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		// 位置坐标
		glm::vec3&& pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		// 法线
		glm::vec3&& nor = (mesh->HasNormals()) ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// 切线
		glm::vec3&& tangent = (mesh->HasTangentsAndBitangents()) ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// UV坐标
		glm::vec2&& UV = (mesh->HasTextureCoords(0)) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.f, 0.f);

		vertices.push_back({ pos, nor, UV , tangent });
	}
	buf_size = sizeof(VulkanEngine::TangentVertex) * vertices.size();

	// 创建顶点缓冲区
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.vertex_buf, &model_mesh.vertex_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}
	if (vk_engine.Write2BufferByStage(model_mesh.vertex_buf, vertices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write vertex data to vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}


	//// 面索引
	std::vector<uint32_t> indices{};
	indices.reserve(mesh->mNumFaces);
	for (uint32_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace& face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	model_mesh.indices_size = (uint32_t)indices.size();


	// 创建索引缓冲区
	buf_size = sizeof(uint32_t) * indices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.index_buf, &model_mesh.index_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	if (vk_engine.Write2BufferByStage(model_mesh.index_buf, indices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write index data to index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	return 0;
}

int ResourcesManager::LoadMeshWithBoneAndTangent(const aiScene* scene, Model& model, uint32_t idx, const VkRenderer& vk_renderer, const char* objfile)
{
	VkDeviceSize buf_size;
	const VulkanEngine& vk_engine = vk_renderer.vk_engine;
	ModelMesh& model_mesh = model.vertex_meshs[idx];

	aiMesh* mesh = scene->mMeshes[idx];
	//// 顶点
	std::vector<VulkanEngine::BoneTangentVertex> vertices{};
	vertices.reserve(mesh->mNumVertices);
	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		// 位置坐标
		glm::vec3&& pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		// 法线
		glm::vec3&& nor = (mesh->HasNormals()) ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// 切线
		glm::vec3&& tangent = (mesh->HasTangentsAndBitangents()) ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z) : glm::vec3(0.f, 0.f, 0.f);

		// UV坐标
		glm::vec2&& UV = (mesh->HasTextureCoords(0)) ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.f, 0.f);

		vertices.push_back({
			pos, nor, UV, tangent,
			{ {0,0,0,0},{0.f,0.f,0.f,0.f} }
			});
	}

	//// 创建骨骼数组，并给顶点复制骨骼权重
	for (uint32_t i = 0; i < mesh->mNumBones; i++)
	{
		aiBone* bone = mesh->mBones[i];
		uint32_t boneidx = model.FindBone(bone);

		uint32_t nodeidx = model.FindStructTreeNode(model.bone_component->bones[boneidx].boneid);
		model.structTree[nodeidx].bones_idx = boneidx;

		for (uint32_t w = 0; w < bone->mNumWeights; w++)
		{
			auto& vertex = vertices[bone->mWeights[w].mVertexId];
			uint8_t w_idx = vertex.bones.FindMinWeight();
			if (vertex.bones.weights[w_idx] < bone->mWeights[w].mWeight)
			{
				vertex.bones.boneidx[w_idx] = boneidx;
				vertex.bones.weights[w_idx] = bone->mWeights[w].mWeight;
			}
		}
	}

	// 创建顶点缓冲区
	buf_size = sizeof(VulkanEngine::BoneTangentVertex) * vertices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.vertex_buf, &model_mesh.vertex_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}
	if (vk_engine.Write2BufferByStage(model_mesh.vertex_buf, vertices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write vertex data to vertex buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}


	//// 面索引
	std::vector<uint32_t> indices{};
	indices.reserve(mesh->mNumFaces);
	for (uint32_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace& face = mesh->mFaces[i];
		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	model_mesh.indices_size = (uint32_t)indices.size();


	// 创建索引缓冲区
	buf_size = sizeof(uint32_t) * indices.size();
	if (vk_engine.CreateBuffer(buf_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&model_mesh.index_buf, &model_mesh.index_buf_memory))
	{
		SDL_Log(LOG_ERROR("Failed to create index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	if (vk_engine.Write2BufferByStage(model_mesh.index_buf, indices.data(), buf_size))
	{
		SDL_Log(LOG_ERROR("Failed to that write index data to index buffer(%s) of (%s)!"), mesh->mName.C_Str(), objfile);
	}

	return 0;
}

uint32_t ResourcesManager::CountTexturesOfAllMaterial(const aiScene* scene, Model& model)
{
	aiString path;
	std::unordered_set<uint64_t> id_table{};

	aiTextureType type_list[6] = { 
		aiTextureType_DIFFUSE,
		aiTextureType_SPECULAR,
		aiTextureType_AMBIENT,
		aiTextureType_EMISSIVE,
		aiTextureType_NORMALS,
		aiTextureType_DIFFUSE_ROUGHNESS
	};
	aiColor4D color_list[6];
	color_list[4] = {0.f, 0.f, 0.f, 0.f};
	color_list[5] = { 0.f, 0.f, 0.f, 0.f };

	for (uint32_t i = 0; i < scene->mNumMaterials; i++)
	{
		aiMaterial* material = scene->mMaterials[i];
		uint64_t id = 0;

		material->Get(AI_MATKEY_COLOR_DIFFUSE, color_list[0]);
		material->Get(AI_MATKEY_COLOR_SPECULAR, color_list[1]);
		material->Get(AI_MATKEY_COLOR_AMBIENT, color_list[2]);
		material->Get(AI_MATKEY_COLOR_EMISSIVE, color_list[3]);
		/*             normals don't have color                */
		//material->Get(AI_MATKEY_METALLIC_FACTOR, color_list[5]);

		for (uint32_t j=0; j < 6; j++)
		{
			// 先看是否又纹理图像
			if (material->GetTextureCount(type_list[j]))
			{
				material->GetTexture(type_list[j], 0, &path);
				id = (uint64_t)std::hash<aiString>()(path) << 32;
			}
			// 再看颜色
			else
			{
				aiColor4D& color = color_list[i];
				// 白色
				if (color.r + color.g + color.b + color.a == 1.f) { break; }
				// 黑色，但还要看alpha通道是否为1，如果为0则说明没有该纹理
				if (color.r * color.g * color.b == 0.f)
				{ 
					if (color.a == 1.f || color.a == 0.f) { break; }
				}
				uint8_t rgba[4];
				rgba[0] = (uint8_t)glm::round(color.r * 255);
				rgba[1] = (uint8_t)glm::round(color.g * 255);
				rgba[2] = (uint8_t)glm::round(color.b * 255);
				rgba[3] = (uint8_t)glm::round(color.a * 255);
				id = *(uint32_t*)rgba;
			}

			auto ret = id_table.find(id);
			if (ret == id_table.end())
			{
				id_table.insert(id);
			}
		}

	}

	return (uint32_t)id_table.size();
}

int ResourcesManager::LoadMaterialTexture(const aiMaterial* material, aiColor4D& tex_color, aiTextureType type, Model& model, const VkRenderer& vk_renderer)
{
	//// 获取纹理
	aiString path;
	aiReturn result;
	uint32_t ret_idx = -1;
	const VulkanEngine& vk_engine = vk_renderer.vk_engine;

	// 先看是否有纹理图像
	if (material->GetTextureCount(type))
	{
		result = material->GetTexture(type, 0, &path);
		if (result == aiReturn_FAILURE)
		{
			SDL_Log(LOG_ERROR("Can't get texture path by assimp(%d)!"), result);
			return -1;
		}

		// 根据纹理路径从该模型的纹理池中直接获取指针
		for (uint32_t i=0; i < model.textures_pool.size(); i++)
		{
			ModelTexture& tex = model.textures_pool[i];
			if (std::hash<aiString>()(path) == (uint32_t)(tex.id >> 32))
			{
				tex.ref_count++;
				return i;
			}
		}

		// 若纹理池中没有就创建一个新的放进纹理池中
		std::string texture_path = model.rootdir + path.C_Str();
		ModelTexture new_texture{};
		vk_engine.CreateModelTexture(texture_path.c_str(), new_texture);
		new_texture.id = (uint64_t)std::hash<aiString>()(path) << 32;
		new_texture.ref_count++;
		ret_idx = (uint32_t)model.textures_pool.size();
		model.textures_pool.push_back(new_texture);
		return ret_idx;
	}
	// 再看是否有纹理颜色
	else
	{
		// 如果颜色为白色或黑色，则直接引用白模黑模即可
		// 判断是否为白色
		if ((tex_color.r * tex_color.g * tex_color.b * tex_color.a) == 1.f)
		{
			return -1;
		}
		// 判断是否为黑色
		if ((tex_color.r + tex_color.g + tex_color.b) == 0.f)
		{
			// 只有alpha通道为1才是黑色，如果为0，那说明没有纹理，引用默认纹理即可
			if (tex_color.a == 1.f)
			{
				return -2;
			}
			if (tex_color.a == 0.f) { return -3; }
		}
		// 若不是黑或白，则需自行创建纹理
		else
		{
			uint8_t rgba[4];
			rgba[0] = (uint8_t)glm::round(tex_color.r * 255);
			rgba[1] = (uint8_t)glm::round(tex_color.g * 255);
			rgba[2] = (uint8_t)glm::round(tex_color.b * 255);
			rgba[3] = (uint8_t)glm::round(tex_color.a * 255);
			// 根据颜色id在该模型纹理池中直接获取颜色相同的单色纹理指针
			for (uint32_t i=0; i < model.textures_pool.size(); i++)
			{
				ModelTexture& tex = model.textures_pool[i];
				if (*(uint32_t*)rgba == (uint32_t)(tex.id & 0xffffffff))
				{
					tex.ref_count++;
					return i;
				}
			}

			// 没有则创建一个放进纹理池中
			ModelTexture new_texture;
			vk_engine.Create1x1PixelTexture(
				rgba[0], rgba[1], rgba[2], rgba[3],
				&new_texture.img, &new_texture.view, &new_texture.memory
			);
			new_texture.id = *(uint32_t*)rgba;
			new_texture.ref_count++;
			ret_idx = (uint32_t)model.textures_pool.size();
			model.textures_pool.push_back(new_texture);
			return ret_idx;
		}
	}

	return ret_idx;
}

int ResourcesManager::LoadMaterial(const aiScene* scene, uint32_t idx, ModelMaterial& model_material, Model& model, const VkRenderer& vk_renderer)
{
	//// 获取纹理
	aiString path;
	aiReturn result;
	aiColor4D tex_color;
	int diffuse;
	int specular;
	int ambient;
	int emissive;
	int normal;
	int metallicRoughness;

	// 先引用对应的默认纹理
	model_material.diffuse = &WhiteTexture;
	model_material.specular = &WhiteTexture;
	model_material.ambient = &WhiteTexture;
	model_material.emissive = &BlackTexture;
	model_material.normal = &BlackTexture;
	model_material.metallicRoughness = &BlackTexture;

	
	//// diffuse 漫反射纹理(默认使用白模)
	result = scene->mMaterials[idx]->Get(AI_MATKEY_COLOR_DIFFUSE, tex_color);
	diffuse = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_DIFFUSE, model, vk_renderer);

	//// sepcular 镜面反射纹理(默认使用白模)
	result = scene->mMaterials[idx]->Get(AI_MATKEY_COLOR_SPECULAR, tex_color);
	specular = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_SPECULAR, model, vk_renderer);

	//// ambient 环境光纹理(默认使用白模)
	result = scene->mMaterials[idx]->Get(AI_MATKEY_COLOR_AMBIENT, tex_color);
	ambient = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_AMBIENT, model, vk_renderer);

	//// emissive 自发光纹理(默认使用黑模，因为是直接加载漫反射纹理上的)
	result = scene->mMaterials[idx]->Get(AI_MATKEY_COLOR_EMISSIVE, tex_color);
	emissive = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_EMISSIVE, model, vk_renderer);

	//// normal 法线贴图纹理(默认使用黑模)
	tex_color = { 0.f, 0.f, 0.f, 0.f };
	normal = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_NORMALS, model, vk_renderer);

	//// metallicRoughness 金属度和粗糙度纹理(默认使用黑模)
	tex_color = { 0.f, 0.f, 0.f, 0.f };
	metallicRoughness = LoadMaterialTexture(scene->mMaterials[idx], tex_color, aiTextureType_DIFFUSE_ROUGHNESS, model, vk_renderer);

	// 判断默认白模的纹理是否为黑膜
	if (diffuse == -2) { model_material.diffuse = &BlackTexture; }
	if (specular == -2) { model_material.specular = &BlackTexture; }
	if (ambient == -2) { model_material.ambient = &BlackTexture; }
	// 判断默认黑模的纹理是否为白膜
	if (emissive == -1) { model_material.diffuse = &WhiteTexture; }
	if (normal == -1) { model_material.specular = &WhiteTexture; }
	if (metallicRoughness == -1) { model_material.ambient = &WhiteTexture; }

	if (diffuse >= 0) { model_material.diffuse = &model.textures_pool[diffuse]; }
	if (specular >= 0) { model_material.specular = &model.textures_pool[specular]; }
	if (ambient >= 0) { model_material.ambient = &model.textures_pool[ambient]; }
	if (emissive >= 0) { model_material.emissive = &model.textures_pool[emissive]; }
	if (normal >= 0) { model_material.normal = &model.textures_pool[normal]; }
	if (metallicRoughness >= 0) { model_material.metallicRoughness = &model.textures_pool[metallicRoughness]; }

	// 创建描述符集
	vk_renderer.vk_engine.CreateDescriptorSets(vk_renderer.GetGeneral3DModelRenderSystem().descriptor_layout[0], vk_renderer.vk_engine.descriptor_pool, &model_material.descriptor_set);
	model_material.UpdateDescriptorSets();

	return 0;
}

// 关键帧的时刻比较函数
// reutrn a.time < b.time
bool KeyFrameTimeCompare(const Animation3D::KeyFrame& a, const Animation3D::KeyFrame& b)
{
	return a.time < b.time;
}

int ResourcesManager::LoadAnimation(const aiScene* scene, Model& model, uint32_t idx)
{

	const aiAnimation* animation = scene->mAnimations[idx];
	ModelAnimation& model_anim = model.animations[idx];

	for (uint32_t c = 0; c < animation->mNumChannels; c++)
	{
		ModelAnimation::Channel module_anim_channel{};
		const aiNodeAnim* channel = animation->mChannels[c];
		module_anim_channel.node_idx = model.FindStructTreeNode(std::hash<aiString>()(channel->mNodeName));
		module_anim_channel.anim_idx = idx;
		module_anim_channel.channel_idx = c;

		for (uint32_t p = 0; p < channel->mNumPositionKeys; p++)
		{
			aiVectorKey& pos_key = channel->mPositionKeys[p];
			Animation3D::KeyFrame& keyframe = module_anim_channel.anim3D.FindKeyFrame(pos_key.mTime, 0);
			keyframe.transform.translate = aiToGlm(pos_key.mValue);
		}

		for (uint32_t r = 0; r < channel->mNumRotationKeys; r++)
		{
			aiQuatKey& quat_key = channel->mRotationKeys[r];
			Animation3D::KeyFrame& keyframe = module_anim_channel.anim3D.FindKeyFrame(quat_key.mTime, 1);
			keyframe.transform.rotation = aiToGlm(quat_key.mValue);
		}

		for (uint32_t s = 0; s < channel->mNumScalingKeys; s++)
		{
			aiVectorKey& scal_key = channel->mScalingKeys[s];
			Animation3D::KeyFrame& keyframe = module_anim_channel.anim3D.FindKeyFrame(scal_key.mTime, 2);
			keyframe.transform.scaling = aiToGlm(scal_key.mValue);
		}

		double ticks_persecond = (animation->mTicksPerSecond != 0.0) ? animation->mTicksPerSecond : 25.0;
		// 给位移、旋转、缩放关键帧数组按时间排序，并设置好时间
		for (uint8_t f=0; f < 3; f++)
		{
			std::vector<Animation3D::KeyFrame>& key_frames = module_anim_channel.anim3D.GetKeyFrames(f);
			std::sort(key_frames.begin(), key_frames.end(), KeyFrameTimeCompare);
			for (Animation3D::KeyFrame& key_frame : key_frames)
			{
				key_frame.time /= ticks_persecond;
			}
			module_anim_channel.anim3D.SetAllTime(animation->mDuration / ticks_persecond);
		}
		model_anim.channels.push_back(module_anim_channel);

	}

	model_anim.anim_idx = idx;
	model_anim.update_queue.resize(model_anim.channels.size());

	return 0;
}

int ResourcesManager::GetNodeTreeMatrix(const aiScene* scene, Model& model)
{
	struct stack_node
	{
		aiNode* node;
		glm::mat4 matrix;
		uint32_t treeidx;
		uint32_t parentidx;

		uint32_t idx;
		bool ret;

		stack_node(aiNode* node, const glm::mat4& matrix, uint32_t parentidx) :
			node(node), matrix(matrix), treeidx(0), parentidx(parentidx), idx(0), ret(false) {
		}
	};

	std::vector<stack_node> stack{};


	stack.emplace_back(scene->mRootNode, glm::mat4(1.f), -1);

	while (!stack.empty())
	{
		stack_node& current = stack.back();
		aiMatrix4x4& aimat = current.node->mTransformation;
		glm::mat4 cur_mat(1.f);
		if (!aimat.IsIdentity())
		{
			cur_mat = aiToGlm(aimat);
		}
		//cur_mat = current.matrix * cur_mat;

		if (!current.ret)
		{
			current.treeidx = (uint32_t)model.structTree.size();

			model.structTree.emplace_back(cur_mat, std::hash<aiString>()(current.node->mName), current.parentidx);

			model.structTree.back().Childrens.reserve(current.node->mNumChildren);

			model.structTree.back().meshs.resize(current.node->mNumMeshes);
			memcpy_s(model.structTree.back().meshs.data(), current.node->mNumMeshes * 4, current.node->mMeshes, current.node->mNumMeshes * 4);

			if (current.parentidx < (uint32_t)model.structTree.size())
			{
				model.structTree[current.parentidx].Childrens.push_back(current.treeidx);
			}

			if (!current.node->mNumChildren)
			{
				stack.pop_back();
				if (stack.empty()) { continue; }
				stack.back().ret = true;
				continue;
			}
			stack.emplace_back(current.node->mChildren[current.idx], cur_mat, current.treeidx);
		}
		else
		{
			current.idx += 1;
			if (current.idx >= current.node->mNumChildren)
			{
				stack.pop_back();
				if (stack.empty()) { continue; }
				stack.back().ret = true;
				continue;
			}

			stack.emplace_back(current.node->mChildren[current.idx], cur_mat, current.treeidx);
		}
	}

	return 0;
}


/**
 * @brief 输出模型材质包含的纹理
 * @param material - Assimp加载的模型材质
 * @param index - 该材质在模型中的索引
 */
void OutputMaterialTexture(aiMaterial* material, uint32_t index)
{
	aiString path;
	uint32_t diffuse = material->GetTextureCount(aiTextureType_DIFFUSE);
	uint32_t specular = material->GetTextureCount(aiTextureType_SPECULAR);
	uint32_t ambient = material->GetTextureCount(aiTextureType_AMBIENT);
	uint32_t emissive = material->GetTextureCount(aiTextureType_EMISSIVE);
	uint32_t normal = material->GetTextureCount(aiTextureType_NORMALS);
	uint32_t roughness = material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS);
	uint32_t metallic = material->GetTextureCount(aiTextureType_METALNESS);

	// diffuse
	if (diffuse)
	{
		SDL_Log("    material[%d] have %d diffuse Texture", index, diffuse);
		for (uint32_t i = 0; i < diffuse; i++)
		{
			material->GetTexture(aiTextureType_DIFFUSE, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_COLOR_DIFFUSE, tex_color);
		SDL_Log("    material[%d] have diffuse color (%d,%d,%d,%d)", index,
			(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
		);
	}
	// specular
	if (specular)
	{
		SDL_Log("    material[%d] have %d specular Texture", index, specular);
		for (uint32_t i = 0; i < specular; i++)
		{
			material->GetTexture(aiTextureType_SPECULAR, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_COLOR_SPECULAR, tex_color);

		if (tex_color.r + tex_color.g + tex_color.b + tex_color.a != 0.f)
		{
			SDL_Log("    material[%d] have specular color (%d,%d,%d,%d)", index,
				(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
			);
		}
	}
	// ambient
	if (ambient)
	{
		SDL_Log("    material[%d] have %d ambient Texture", index, ambient);
		for (uint32_t i = 0; i < ambient; i++)
		{
			material->GetTexture(aiTextureType_AMBIENT, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_COLOR_AMBIENT, tex_color);

		if (tex_color.r + tex_color.g + tex_color.b + tex_color.a != 0.f)
		{
			SDL_Log("    material[%d] have ambient color (%d,%d,%d,%d)", index,
				(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
			);
		}
	}
	// emissive
	if (emissive)
	{
		SDL_Log("    material[%d] have %d emissive Texture", index, emissive);
		for (uint32_t i = 0; i < emissive; i++)
		{
			material->GetTexture(aiTextureType_EMISSIVE, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_COLOR_EMISSIVE, tex_color);

		if (tex_color.r + tex_color.g + tex_color.b != 0.f)
		{
			SDL_Log("    material[%d] have emissive color (%d,%d,%d,%d)", index,
				(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
			);
		}
	}
	// normal
	if (normal)
	{
		SDL_Log("    material[%d] have %d normal Texture", index, normal);
		for (uint32_t i = 0; i < normal; i++)
		{
			material->GetTexture(aiTextureType_NORMALS, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	// roughness
	if (roughness)
	{
		SDL_Log("    material[%d] have %d roughness Texture", index, roughness);
		for (uint32_t i = 0; i < roughness; i++)
		{
			material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_ROUGHNESS_FACTOR, tex_color);

		if (tex_color.r != 1.f || (tex_color.g + tex_color.b != 0.f))
		{
			SDL_Log("    material[%d] have roughness color (%d,%d,%d,%d)", index,
				(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
			);
		}
	}
	// metallic
	if (metallic)
	{
		SDL_Log("    material[%d] have %d metallic Texture", index, metallic);
		for (uint32_t i = 0; i < metallic; i++)
		{
			material->GetTexture(aiTextureType_METALNESS, i, &path);
			SDL_Log("      (%s)", path.C_Str());
		}
	}
	else
	{
		aiColor4D tex_color;
		material->Get(AI_MATKEY_METALLIC_FACTOR, tex_color);

		if (tex_color.r + tex_color.g + tex_color.b != 0.f)
		{
			SDL_Log("    material[%d] have metallic color (%d,%d,%d,%d)", index,
				(int)glm::round(tex_color.r * 255), (int)glm::round(tex_color.g * 255), (int)glm::round(tex_color.b * 255), (int)glm::round(tex_color.a * 255)
			);
		}
	}

}
/**
 * @brief 输出模型的层级结构
 * @param scene - Assimp加载的模型
 */
void OutputSceneNodeStruct(const aiScene* scene)
{
	struct temp_node
	{
		aiNode* node;
		uint32_t idx;
		bool ret;

		temp_node(aiNode* node) : node(node), idx(0), ret(false) {}
	};
	std::vector<temp_node> stack;
	std::string space("  ");
	std::string structure{};

	stack.emplace_back(scene->mRootNode);

	// 递归循环写法
	while (!stack.empty())
	{
		temp_node& current = stack.back();
		if (!current.ret)
		{
			if ((stack.size() - 1) * 2 > space.size())
			{
				space += space;
			}
			structure = space.substr(0, (stack.size() - 1) * 2);
			SDL_Log("%s- %s", structure.c_str(), current.node->mName.C_Str());
			if (current.node->mNumMeshes)
			{
				for (uint32_t i = 0; i < current.node->mNumMeshes; i++)
				{
					SDL_Log("%s  mesh[%d]", structure.c_str(), current.node->mMeshes[i]);
				}
			}
			if (!current.node->mNumChildren)
			{
				stack.pop_back();
				if (stack.empty()) { continue; }
				stack.back().ret = true;
				continue;
			}
			stack.emplace_back(current.node->mChildren[current.idx]);
		}
		else
		{
			current.idx += 1;
			if (current.idx >= current.node->mNumChildren)
			{
				stack.pop_back();
				if (stack.empty()) { continue; }
				stack.back().ret = true;
				continue;
			}
			stack.emplace_back(current.node->mChildren[current.idx]);
		}
	}
}
/**
 * @brief 输出动画相关信息
 * @param animation - 要输出的动画
 * @param index - 动画的索引
 */
void OutputAnimation(const aiAnimation* animation, uint32_t index)
{
	SDL_Log("    Animation[%d] have %d Channels", index, animation->mNumChannels);
	for (uint32_t i = 0; i < animation->mNumChannels; i++)
	{
		SDL_Log("        Channel[%d] (node:%s) PositionKeys: %d", i, animation->mChannels[i]->mNodeName.C_Str(), animation->mChannels[i]->mNumPositionKeys);
		/*for (uint32_t p = 0; p < animation->mChannels[i]->mNumPositionKeys; p++)
		{
			aiVectorKey& pos_key = animation->mChannels[i]->mPositionKeys[p];
			SDL_Log("          Position key[%d] time: %.2f position: (%.2f,%.2f,%.2f) enum: %d",
				p,
				pos_key.mTime,
				pos_key.mValue.x, pos_key.mValue.y, pos_key.mValue.z,
				pos_key.mInterpolation
			);
		}*/

		SDL_Log("                   RotationKeys: %d", animation->mChannels[i]->mNumRotationKeys);
		/*for (uint32_t r = 0; r < animation->mChannels[i]->mNumRotationKeys; r++)
		{
			aiQuatKey& quat_key = animation->mChannels[i]->mRotationKeys[r];
			SDL_Log("          Rotation key[%d] time: %.2f quaternion: (%.4f,%.4f,%.4f,%.4f) enum: %d",
				r,
				quat_key.mTime,
				quat_key.mValue.w, quat_key.mValue.x,quat_key.mValue.y,quat_key.mValue.z,
				quat_key.mInterpolation
			);
		}*/

		SDL_Log("                   ScalingKeys: %d", animation->mChannels[i]->mNumScalingKeys);
		/*for (uint32_t s = 0; s < animation->mChannels[i]->mNumScalingKeys; s++)
		{
			aiVectorKey& scal_key = animation->mChannels[i]->mScalingKeys[s];
			SDL_Log("          Scaling Key[%d] time: %.2f position: (%.2f,%.2f,%.2f) enum: %d",
				s,
				scal_key.mTime,
				scal_key.mValue.x, scal_key.mValue.y, scal_key.mValue.z,
				scal_key.mInterpolation
			);
		}*/
	}

	SDL_Log("    Animation[%d] Duration: %.8f", index, animation->mDuration);
	SDL_Log("    Animation[%d] 1 second ticks(frame): %.4f", index, animation->mTicksPerSecond);
}

void ResourcesManager::OutputMeshInformation(const aiScene* scene, const std::string& mesh_dir, const char* path)
{
	SDL_Log("");
	SDL_Log(LOG_INFO("Model(%s):"), path);
	if (scene->mRootNode->mNumChildren)
	{
		SDL_Log("Node struct:");
		OutputSceneNodeStruct(scene);
		/*aiMatrix4x4& matrix = scene->mRootNode->mChildren[0]->mTransformation;
		SDL_Log("[%.4f,%.4f,%.4f,%.4f]", matrix.a1, matrix.a2, matrix.a3, matrix.a4);
		SDL_Log("[%.4f,%.4f,%.4f,%.4f]", matrix.b1, matrix.b2, matrix.b3, matrix.b4);
		SDL_Log("[%.4f,%.4f,%.4f,%.4f]", matrix.c1, matrix.c2, matrix.c3, matrix.c4);
		SDL_Log("[%.4f,%.4f,%.4f,%.4f]", matrix.d1, matrix.d2, matrix.d3, matrix.d4);*/
	}
	SDL_Log("  Have %d Meshes", scene->mNumMeshes);
	for (uint32_t i = 0; i < scene->mNumMeshes; i++)
	{
		SDL_Log("    mesh[%d] (%s)", i, scene->mMeshes[i]->mName.C_Str());
		SDL_Log("        have %d Vertices", scene->mMeshes[i]->mNumVertices);
		SDL_Log("        have %d Faces", scene->mMeshes[i]->mNumFaces);
		SDL_Log("        have %d Bones", scene->mMeshes[i]->mNumBones);
		if (scene->mMeshes[i]->mNumBones)
		{
			for (uint32_t b = 0; b < scene->mMeshes[i]->mNumBones; b++)
			{
				SDL_Log("            Bones[%d] name:(%s)", b, scene->mMeshes[i]->mBones[b]->mName.C_Str());
			}
		}
		SDL_Log("        Material index [%d]", scene->mMeshes[i]->mMaterialIndex);
	}
	SDL_Log("  Have %d Materials", scene->mNumMaterials);
	for (uint32_t i = 0; i < scene->mNumMaterials; i++)
	{
		OutputMaterialTexture(scene->mMaterials[i], i);
	}
	SDL_Log("  Have %d Animations", scene->mNumAnimations);
	for (uint32_t i = 0; i < scene->mNumAnimations; i++)
	{
		OutputAnimation(scene->mAnimations[i], i);
	}
	SDL_Log("  Have %d Textures", scene->mNumTextures);
	SDL_Log("  Have %d Lights", scene->mNumLights);
	SDL_Log("  Have %d Cameras", scene->mNumCameras);
}
