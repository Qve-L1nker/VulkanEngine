#version 450

// layout(push_constant) uniform PushContants
// {
// 	layout(offset = 0) uvec2 txt;
// } FPS_txt;

layout(location = 0) in vec2 in_positions;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_tex_idx;

layout(location = 0) out vec2 fragTexCoord;
// layout(location = 1) out uint fragTexIdx;

void main()
{
	gl_Position = vec4(in_positions, 0.0, 1.0);

	fragTexCoord = in_uv;
}
