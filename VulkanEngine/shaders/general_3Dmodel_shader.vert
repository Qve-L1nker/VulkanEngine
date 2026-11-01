#version 450

layout(location = 0) in vec3 in_positions;     // 顶点位置
layout(location = 1) in vec3 in_normal;        // 法线
layout(location = 2) in vec2 in_texCoord;      // UV坐标
layout(location = 3) in vec3 in_tangent;       // 沿UV坐标的U方向的切线
layout(location = 4) in ivec4 in_bone_ids;     // 骨骼索引
layout(location = 5) in vec4 in_bone_weights;  // 骨骼权重

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragTangent;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 worldPosition;
layout(location = 4) out vec3 cameraViewPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(set = 2, binding = 0) readonly buffer BoneMatrixSSBO
{
	mat4 array[100];
} BoneMatrix;

layout(push_constant) uniform PushContants{
	layout(offset = 0) mat4 model_mat;
    layout(offset = 64) mat4 mesh_mat;
    layout(offset = 128) uint mesh_flags;
} obj;

const float PI = 3.14159265358979;

void main()
{
    mat4 final_mat;

    // 根据网格标志(mesh_flags)的bit位判断网格类型
    if (bool(bitfieldExtract(obj.mesh_flags, 0, 1)))
    {
        // 有骨骼的网格
        final_mat = BoneMatrix.array[in_bone_ids[0]] * in_bone_weights[0]
                  + BoneMatrix.array[in_bone_ids[1]] * in_bone_weights[1]
                  + BoneMatrix.array[in_bone_ids[2]] * in_bone_weights[2]
                  + BoneMatrix.array[in_bone_ids[3]] * in_bone_weights[3];
    }
    else
    {
        // 什么都没有的网格
        final_mat = obj.mesh_mat;
    }

    mat3 X_rot = mat3(
        1.0,        0.0,       0.0,
        0.0,  cos(PI/2),  sin(PI/2),
        0.0, -sin(PI/2),  cos(PI/2)
    );

    final_mat = mat4(X_rot) * final_mat;

	gl_Position = ubo.proj * ubo.view * obj.model_mat * final_mat * vec4(in_positions, 1.0);

	fragNormal = (obj.model_mat * final_mat * vec4(in_normal, 0.0)).xyz;
    if (!bool(isnan(in_tangent))) {fragTangent = (obj.model_mat * final_mat * vec4(in_tangent, 0.0)).xyz;}
	fragTexCoord = in_texCoord;
	worldPosition = (obj.model_mat * final_mat * vec4(in_positions, 1.0)).xyz;
	cameraViewPosition = (ubo.view * obj.model_mat * final_mat * vec4(in_positions, 1.0)).xyz;
}
