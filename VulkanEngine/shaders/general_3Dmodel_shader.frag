#version 450

//// 计算定向光源(如：太阳光这样的平行光)
struct DirLight
{
	vec3 direction;
	vec3 color;
};
vec3 CalDirLight(DirLight light, vec3 diffuse, vec3 specular, vec3 normal);
////

//// 计算点光源
struct PointLight
{
	vec3 position;
	vec3 color;
};
vec3 CalPointLight(PointLight light, vec3 diffuse, vec3 specular, vec3 normal);
////

#define POINT_LIGHT_FLAG 0
#define DIR_LIGHT_FLAG 1
#define CONE_LIGHT_FLAG 2

vec3 PBR(vec3 albedo, vec3 normal, float metallic, float roughness);

layout(set = 1, binding = 0) uniform sampler2D textures[6];
// layout(binding = 1) uniform sampler2D diffuse_tex;
// layout(binding = 2) uniform sampler2D specular_tex;
// layout(binding = 3) uniform sampler2D ambient_tex;
// layout(binding = 4) uniform sampler2D emissive_tex;
// layout(binding = 5) uniform sampler2D normal_tex;
// layout(binding = 6) uniform sampler2D metallic_roughness_tex;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragTangent;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 worldPosition;
layout(location = 4) in vec3 cameraViewPosition;

layout(push_constant) uniform PushContants{
    layout(offset = 132) uint texture_flags;
} obj;

layout(location = 0) out vec4 outColor;

struct Light
{
	uint flags;
	vec3 L;
	vec3 color;
} light_array[3] = {  // 灯光数组
	{DIR_LIGHT_FLAG, vec3(1.0, -1.0, -1.0), vec3(1.0, 1.0, 1.0)},
	{DIR_LIGHT_FLAG, vec3(-1.0, 0.0, 1.0), vec3(1.0, 1.0, 1.0)},
	{DIR_LIGHT_FLAG, vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 1.0)}
};

uint light_array_count = 3;

const float PI = 3.14159265358979;

void main()
{
	vec3 normal = normalize(fragNormal);

	// UV坐标下的纹素
	vec4 diffuse = texture(textures[0], fragTexCoord);
	vec4 specular = texture(textures[1], fragTexCoord);
	float ambient = texture(textures[2], fragTexCoord).a;
    vec4 emissive = texture(textures[3], fragTexCoord);
	vec3 mr = texture(textures[5], fragTexCoord).rgb;

	// 如果有法线贴图则使用法线贴图里的值代替法线
	if (bool(bitfieldExtract(obj.texture_flags, 0, 1)))
	{
		// 计算转换到切线空间的矩阵
		vec3 T = normalize(fragTangent);
		mat3 TBN = mat3(T, cross(normal, T), normal);
		// 获取法线
		normal = normalize(texture(textures[4], fragTexCoord).xyz*2.0 - 1.0);
		normal = normalize(TBN * normal);
	}

	vec3 result = PBR(diffuse.xyz, normal, mr.b, clamp(mr.g, 0.045, 1.0));

    result += emissive.xyz;

	result = result / (result + vec3(1.0));
	result = pow(result, vec3(1.0/2.2));
	outColor = vec4(result, diffuse.a);
}

// 法线分布函数
float DistributionGGX(vec3 n, vec3 h, float roughness)
{
	float NdotH = max(dot(n,h), 0.0);

	////////////////
	// GGX分布
	// r = roughness^2
	// D_h = r^2 / PI * ((n·h)^2 * (r^2-1) + 1)^2
	float r4  = pow(roughness, 4);
	float d  = (NdotH * r4 - NdotH) * NdotH + 1.0;
	return r4 / PI * pow(d,2);
	////////////////

	////////////////
	// Beckmann 分布
	// r = roughness
	// D_h = (1/PI*r^2*n·h^4) * exp((n·h^2 - 1) / (r^2 * n·h^2))
	////////////////

	////////////////
	// 正态分布
	// r = roughness
	// sigma = (-1/4*r^2+1/4)
	// D_h = (1/sigma*sprt(2*PI)) * exp(-1/2((n·h^2-1)/sigma)^2)
	////////////////
}

// 环境光遮蔽函数
float GeometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
	// G_nvl = 0.5 / 2*nl*(1-r)*nv + r*(nl + nv)

	float NdotV = max(dot(n,v), 0.0);
	float NdotL = max(dot(n,l), 0.0);

	// float r = roughness;
	// return 0.5 / 2 * NdotL * NdotV * (1-r) + r * (NdotV + NdotL) + 0.000001;

	float k = (roughness + 1.0);
	k = (k * k) / 8.0;                                 // k = (r+1)²/8
	float gV = NdotV / (NdotV * (1.0 - k) + k);        // G1(v)
	float gL = NdotL / (NdotL * (1.0 - k) + k);        // G1(l)
	return gV * gL;                                    // G = G1(l)·G1(v)
}

// 菲涅尔方程
vec3 FresnelSchlick(vec3 h, vec3 v, vec3 F0)
{
	float HdotV = max(dot(h,v), 0.0);
	return F0 + (1 - F0) * pow(1 - HdotV, 5);
}

vec3 PBR(vec3 albedo, vec3 normal, float metallic, float roughness)
{
	/* N - 法线
	 * H - 能把入射光线反射向摄像机的角平分线向量
	 * V - 指向摄像机的方向
	 * L - 入射光线的反方向
	 * F0 - 反射光的颜色，金属度越高
	 * albedo - 物体基础色
	 * metallic - 金属度
	 * roughness - 粗糙度
	**/
	vec3 N = normalize(normal);
	vec3 V = normalize(-cameraViewPosition);
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3 color = vec3(0.0);
	for (int i=0; i < light_array_count; i++)
	{
		Light light = light_array[i];
		vec3 L;
		float distan;
		switch (light.flags)
		{
		case POINT_LIGHT_FLAG:
			L = normalize(light.L - worldPosition);
			distan = length(light.L - worldPosition) + 0.000001;
			break;
		case DIR_LIGHT_FLAG:
			L = normalize(-light.L);
			distan = 1;
			break;
		}
		vec3 H = normalize(V + L);

		float D = DistributionGGX(N, H, roughness);
		vec3 F = FresnelSchlick(H, V, F0);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 spec = D*F*G / (4 * max(dot(N,V), 0.0) * max(dot(N,L), 0.0) + 0.0001);

		vec3 kd = (1.0 - F) * (1.0 - metallic);
		// float ks = metallic;
		color += (kd * albedo / PI + spec) * light.color * max(dot(N,L), 0.0) / pow(distan, 2);
	}

	return color;
}
