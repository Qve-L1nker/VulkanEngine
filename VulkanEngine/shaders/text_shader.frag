#version 450

layout(set = 0, binding = 0) uniform sampler2D text_texture;

layout(location = 0) in vec2 fragTexCoord;
// layout(location = 1) flat in uint fragTexIdx;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(text_texture, fragTexCoord);
}
