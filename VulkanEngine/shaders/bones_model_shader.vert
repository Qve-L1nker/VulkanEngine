#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(set = 2, binding = 0) uniform BoneMatrixUBO
{
	mat4 array[100];
} BoneMatrix;

layout(push_constant) uniform PushContants{
	layout(offset = 0) mat4 model_mat;
} obj;

layout(location = 0) in vec3 in_positions;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texCoord;
layout(location = 3) in ivec4 in_bone_ids;
layout(location = 4) in vec4 in_bone_weights;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 worldPosition;
layout(location = 3) out vec3 cameraPosition;

void main()
{
    mat4 final_bone_mat = BoneMatrix.array[in_bone_ids[0]] * in_bone_weights[0]
                        + BoneMatrix.array[in_bone_ids[1]] * in_bone_weights[1]
                        + BoneMatrix.array[in_bone_ids[2]] * in_bone_weights[2]
                        + BoneMatrix.array[in_bone_ids[3]] * in_bone_weights[3];


	gl_Position = ubo.proj * ubo.view * obj.model_mat * final_bone_mat * vec4(in_positions, 1.0);

	fragNormal = (obj.model_mat * final_bone_mat * vec4(in_normal, 0.0)).xyz;
	fragTexCoord = in_texCoord;
	worldPosition = (obj.model_mat * final_bone_mat * vec4(in_positions, 1.0)).xyz;
	cameraPosition = (ubo.view * obj.model_mat * final_bone_mat * vec4(in_positions, 1.0)).xyz;
}