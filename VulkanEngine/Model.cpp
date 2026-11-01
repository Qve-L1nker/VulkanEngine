#include <assimp/scene.h>

#include "Log_about.h"
#include "VkRenderer.h"
#include "AssimpToGlm.h"
#include "ResourcesManager.h"
#include "Model.h"
#include "GlobalObject.h"

ModelMesh::ModelMesh(const ModelMesh& obj)
{
	// 网格顶点数据
	vertex_buf = nullptr;
	vertex_buf_memory = nullptr;

	// 顶点索引数据
	indices_size = 0;
	index_buf = nullptr;
	index_buf_memory = nullptr;

	// 网格对应材质纹理的索引
	material = nullptr;
}

void ModelMesh::DestroyGPUData() const
{
	const VulkanEngine& vkEngine = Global_vk_renderer.vk_engine;
	vkEngine.DestroyBuffer(vertex_buf, vertex_buf_memory);
	vkEngine.DestroyBuffer(index_buf, index_buf_memory);
}

void ModelMaterial::UpdateDescriptorSets()
{
	const VulkanEngine& vkEngine = Global_vk_renderer.vk_engine;
	VkWriteDescriptorSet descriptor_writes{};
	VkDescriptorImageInfo img_infos[6]{};

	// diffuse
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[0].imageView = diffuse->view;
	img_infos[0].sampler = vkEngine.texture_sampler;

	// specular
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[1].imageView = specular->view;
	img_infos[1].sampler = vkEngine.texture_sampler;

	// ambient
	img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[2].imageView = ambient->view;
	img_infos[2].sampler = vkEngine.texture_sampler;

	// emissive
	img_infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[3].imageView = emissive->view;
	img_infos[3].sampler = vkEngine.texture_sampler;

	// normal
	img_infos[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[4].imageView = normal->view;
	img_infos[4].sampler = vkEngine.texture_sampler;

	// metallicRoughness
	img_infos[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[5].imageView = metallicRoughness->view;
	img_infos[5].sampler = vkEngine.texture_sampler;

	descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes.dstSet = descriptor_set;
	descriptor_writes.dstBinding = 0;
	descriptor_writes.dstArrayElement = 0;
	descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_writes.descriptorCount = 6;
	descriptor_writes.pImageInfo = img_infos;
	//descriptor_writes[0].pBufferInfo = &buf_info;
	//descriptor_writes[0].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(vkEngine.device, 1, &descriptor_writes, 0, nullptr);
}

void ModelMaterial::Destroy()
{
	//VulkanEngine& vkEngine;

	diffuse->ref_count -= 1;
	specular->ref_count -= 1;
	ambient->ref_count -= 1;
	emissive->ref_count -= 1;
	normal->ref_count -= 1;
	metallicRoughness->ref_count -= 1;
}

int ModelAnimation::Update(double delta_time)
{
	uint32_t update_count = 0;

	for (uint32_t i=0; i < update_queue.size(); i++)
	{
		if (update_queue[i])
		{
			update_count++;
			if (update_queue[i]->anim3D.Update(delta_time))
			{
				update_queue[i] = nullptr;
			}
		}
	}

	// 所有通道都更新完毕
	if (!update_count)
	{
		if (is_loop)
		{
			Begin();
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

void ModelAnimation::Begin()
{
	is_trigger = true;
	Reset();
	for (uint32_t i=0; i < channels.size(); i++)
	{
		update_queue[i] = &channels[i];
	}
}

void ModelAnimation::End()
{
	is_trigger = false;
}

void ModelAnimation::Reset()
{
	for (ModelAnimation::Channel& channel: channels)
	{
		channel.anim3D.Reset();
	}
}


int ModelBoneComponent::CreateDescriptSet()
{
	const VulkanEngine& vkEngine = Global_vk_renderer.vk_engine;

	if (bones.empty())
	{
		SDL_Log(LOG_ERROR("Bone matrix array is empty, Can't Create matrix buffer descriptor set!"));
		return 1;
	}

	for (int i = 0; i < 2; i++)
	{
		//// 创建骨骼矩阵数组缓冲区
		VkDeviceSize buf_size = sizeof(glm::mat4) * 100;

		if (vkEngine.CreateBuffer(buf_size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniform_buf[i], &uniform_buf_memory[i]))
		{
			SDL_Log(LOG_ERROR("Failed to create bones matrix uniform buffer!"));
			return 1;
		}
		vkMapMemory(vkEngine.device, uniform_buf_memory[i], 0, buf_size, 0, &uniform_map[i]);

		//// 创建描述符集
		vkEngine.CreateDescriptorSets(Global_vk_renderer.GetGeneral3DModelRenderSystem().descriptor_layout[1], vkEngine.descriptor_pool, &bones_matrix_set[i]);

		VkDescriptorBufferInfo buf_info{};
		buf_info.buffer = uniform_buf[i];
		buf_info.offset = 0;
		buf_info.range = buf_size;

		VkWriteDescriptorSet descriptor_writes{};
		descriptor_writes.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes.dstSet = bones_matrix_set[i];
		descriptor_writes.dstBinding = 0;
		descriptor_writes.dstArrayElement = 0;
		descriptor_writes.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptor_writes.descriptorCount = 1;
		descriptor_writes.pBufferInfo = &buf_info;
		//descriptor_writes.pImageInfo = nullptr;
		//descriptor_writes.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(vkEngine.device, 1, &descriptor_writes, 0, nullptr);
	}

	return 0;
}

void ModelBoneComponent::SyncBonesMatrixToUBO(uint8_t vkEngine_cur_frame)
{
	memcpy(uniform_map[vkEngine_cur_frame], final_matrix.data(), final_matrix.size() * sizeof(glm::mat4));
}

void ModelBoneComponent::DestroyGPUData()
{
	VulkanEngine& vkEngine = Global_vk_renderer.vk_engine;

	// 销毁缓冲区
	for (int i = 0; i < 2; i++)
	{
		vkUnmapMemory(vkEngine.device, uniform_buf_memory[i]);
		vkDestroyBuffer(vkEngine.device, uniform_buf[i], nullptr);
		vkFreeMemory(vkEngine.device, uniform_buf_memory[i], nullptr);
	}
}


Model::Model(const char* meshfile, const VkRenderer& vk_renderer, bool is_print_information, bool is_create_mesh)
{
	Global_resources_manager.LoadModel(meshfile, *this, vk_renderer, is_create_mesh, is_print_information);
}

void Model::Update(double delata_time)
{
	if (cur_animation && cur_animation->is_trigger)
	{
		if (cur_animation->Update(delata_time))
		{
			EndAnimation();
		}
	}

	if (!structTree.empty())
	{
		UpdateStructTreeNodeAndAnimMatrix(structTree[0]);
	}
}

uint32_t Model::FindStructTreeNode(uint32_t node_id)
{
	if (structTree.empty())
	{
		return -2;
	}

	for (uint32_t i=0; i < structTree.size(); i++)
	{
		if (structTree[i].nodeid == node_id)
		{
			return i;
		}
	}

	return -1;
}

uint32_t Model::FindBone(aiBone* bone)
{
	if (!bone_component)
	{
		return -2;
	}

	uint32_t bone_id = std::hash<aiString>()(bone->mName);
	for (uint32_t i = 0; i < bone_component->bones.size(); i++)
	{
		if (bone_component->bones[i].boneid == bone_id)
		{
			return i;
		}
	}

	uint32_t idx = (uint32_t)bone_component->bones.size();

	bone_component->bones.push_back({bone_id, aiToGlm(bone->mOffsetMatrix)});

	return idx;
}

void Model::UpdateStructTreeNodeAndAnimMatrix(Node& node)
{
	struct stack_node
	{
		Node* node;

		glm::mat4 mat;
		uint32_t idx;

		bool ret;

		stack_node(Node* nodeptr, const glm::mat4& mat) : node(nodeptr), mat(mat), idx(0), ret(false) {}
	};

	std::vector<stack_node> stack{};

	stack.emplace_back(&node, glm::mat4(1.f));

	while (!stack.empty())
	{
		stack_node& current = stack.back();

		// 若被动画标记，则应将自己的变换矩阵设为对应动画的对应轨道帧更新后的插值矩阵，否则就是用节点自身的变换矩阵
		glm::mat4 local_mat = (current.node->anim_idx != -1) ?
			animations[current.node->anim_idx].channels[current.node->channel_idx].anim3D.inter_mat :
			current.node->matrix;

		// 将父节点转下来的矩阵作用在该节点自己的矩阵
		glm::mat4 finally_mat = current.mat * local_mat;

		if (!current.ret)
		{
			// 若为骨骼模型则根据对应的骨骼索引更新矩阵
			if (current.node->bones_idx != -1)
			{
				bone_component->final_matrix[current.node->bones_idx] = finally_mat * bone_component->bones[current.node->bones_idx].offset_mat;
			}

			// 将叠加好的矩阵传给该节点的每个网格
			if (!current.node->meshs.empty())
			{
				for (uint32_t i: current.node->meshs)
				{
					vertex_meshs[i].node_and_anim_matrix = finally_mat;
				}
			}

			if (current.node->Childrens.empty())
			{
				stack.pop_back();
				if (!stack.empty())
				{
					stack.back().ret = true;
				}
				continue;
			}

			stack.emplace_back(&structTree[current.node->Childrens[current.idx]], finally_mat);
		}
		else
		{
			current.idx += 1;
			if (current.idx >= current.node->Childrens.size())
			{
				stack.pop_back();
				if (!stack.empty())
				{
					stack.back().ret = true;
				}
				continue;
			}

			stack.emplace_back(&structTree[current.node->Childrens[current.idx]], finally_mat);
		}
	}
}

void Model::TriggerAnimation()
{
	cur_animation->End();
	cur_animation->Begin();

	for (ModelAnimation::Channel& channel : cur_animation->channels)
	{
		structTree[channel.node_idx].anim_idx = channel.anim_idx;
		structTree[channel.node_idx].channel_idx = channel.channel_idx;
	}
}

void Model::EndAnimation()
{
	cur_animation->End();

	for (ModelAnimation::Channel& channel : cur_animation->channels)
	{
		structTree[channel.node_idx].anim_idx = -1;
		structTree[channel.node_idx].channel_idx = -1;
	}
}

void Model::DestroyGPUData()
{
	for (const ModelMesh& mesh : vertex_meshs)
	{
		mesh.DestroyGPUData();
	}
	for (ModelMaterial& material: materials)
	{
		material.Destroy();
	}
	for (ModelTexture& texture: textures_pool)
	{
		Global_vk_renderer.vk_engine.DestroyModelTexture(texture);
	}

	if (bone_component)
	{
		bone_component->DestroyGPUData();
		delete bone_component;
	}
}

