#version 450

// 计算该顶点对应的uv
vec2 CalNumUV();

vec2 Ascii_UV_Map[10] = {
	vec2(0.0, 0.5),
	vec2(0.2, 0.5),
	vec2(0.4, 0.5),
	vec2(0.6, 0.5),
	vec2(0.8, 0.5),
	vec2(0.0, 1.0),
	vec2(0.2, 1.0),
	vec2(0.4, 1.0),
	vec2(0.6, 1.0),
	vec2(0.8, 1.0)
};

layout(push_constant) uniform PushContants
{
	layout(offset = 0) uvec2 txt;
} FPS_txt;

layout(location = 0) in vec2 in_positions;
layout(location = 1) in uint in_tex_idx;
layout(location = 2) in uint in_vertex_idx;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out uint fragTexIdx;

void main()
{
	gl_Position = vec4(in_positions, 0.0, 1.0);

	fragTexCoord = CalNumUV();
	fragTexIdx = in_tex_idx;
}

vec2 CalNumUV()
{
	if (in_vertex_idx < 4)
	{
		switch (in_vertex_idx)
		{
		case 0:
			return vec2(0.0, 1.0);
		case 1:
			return vec2(1.0, 1.0);
		case 2:
			return vec2(1.0, 0.0);
		case 3:
			return vec2(0.0, 0.0);
		}
	}

	uint idx1 = int(floor(max(in_vertex_idx - 4, 0) / 4));
	uint idx2 = int(floor(idx1 / 4));
	
	uvec2 ascii = bitfieldExtract(FPS_txt.txt, int(idx1*8), 8);
	// uint ascii = FPS_txt.txt[idx2] & (0xff << ((idx1 - 4*idx2) * 8));
	if (ascii[0] == 0)
	{
		return vec2(0.0, 0.0);
	}

	vec2 result = Ascii_UV_Map[ascii[0] - 0x30];

	uint offset = (in_vertex_idx - 4) - idx1 * 4;

	switch (offset)
	{
	case 1:
		result += vec2(0.2, 0.0);
		break;
	case 2:
		result += vec2(0.2, -0.5);
		break;
	case 3:
		result += vec2(0.0, -0.5);
		break;
	}

	return result;
}
