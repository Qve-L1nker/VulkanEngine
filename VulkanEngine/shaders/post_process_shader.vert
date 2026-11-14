#version 450

layout(location = 0) in vec2 in_positions;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 fragTexCoord;

void main()
{
	gl_Position = vec4(in_positions, 0.0, 1.0);

	fragTexCoord = in_uv;
}
