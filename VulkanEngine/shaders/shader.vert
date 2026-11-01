#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(push_constant) uniform PushContants{
	layout(offset = 0) mat4 model_mat;
	layout(offset = 64) mat4 mesh_mat;
} obj;

layout(location = 0) in vec3 in_positions;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 worldPosition;
layout(location = 3) out vec3 cameraPosition;

void main()
{
	gl_Position = ubo.proj * ubo.view * obj.model_mat * obj.mesh_mat * vec4(in_positions, 1.0);

	fragNormal = (obj.model_mat * obj.mesh_mat * vec4(in_normal, 0.0)).xyz;
	fragTexCoord = in_texCoord;
	worldPosition = (obj.model_mat * vec4(in_positions, 1.0)).xyz;
	cameraPosition = (ubo.view * obj.model_mat * vec4(in_positions, 1.0)).xyz;
}