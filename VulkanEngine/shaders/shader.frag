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


layout(set = 1, binding = 0) uniform sampler2D textures[3];
// layout(binding = 1) uniform sampler2D diffuse_tex;
// layout(binding = 2) uniform sampler2D specular_tex;
// layout(binding = 3) uniform sampler2D ambient_tex;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 worldPosition;
layout(location = 3) in vec3 cameraPosition;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 normal = normalize(fragNormal);
	// 直射太阳光
	DirLight sun1 = {
		vec3(1.0, 0.0, -1.0), // 直射方向
		vec3(1.0, 1.0, 1.0)   // 颜色
	};
	DirLight sun2 = {
		vec3(0.0, 1.0, 0.0), // 直射方向
		vec3(1.0, 1.0, 1.0)   // 颜色
	};
	// 点光源
	// PointLight light = {
	// 	vec3(2.0, 0.0, 0.0),  // 灯光位置
	// 	vec3(1.0, 1.0, 1.0)   // 灯光颜色
	// };

	// UV坐标下的纹素
	vec4 diffuse = texture(textures[0], fragTexCoord);
	vec4 specular = texture(textures[1], fragTexCoord);
	float ambient = texture(textures[2], fragTexCoord).a;

	// 环境光(ambient)
	vec3 result = 0.1 * diffuse.xyz * ambient;
	// 太阳光(direction light)
	result += CalDirLight(sun1, diffuse.xyz, specular.xyz, normal);
	result += CalDirLight(sun2, diffuse.xyz, specular.xyz, normal);
	// 灯光(point light)
	// result += CalPointLight(light, diffuse.xyz, specular.xyz, normal);

	outColor = vec4(result, diffuse.a);
}

vec3 CalDirLight(DirLight light, vec3 diffuse, vec3 specular, vec3 normal)
{
	vec3 light_dir = normalize(-light.direction);

	vec3 E = normalize(vec3(0.0, 0.0, 0.0) - cameraPosition);
	vec3 R = reflect(-light_dir, normal);

	// 法线和光线的点积
	float cosTheta = max(dot(normal, light_dir), 0.0);
	// 漫反射
	vec3 result = diffuse * light.color * cosTheta;

	// 光线反射与视线的方向重合度
	float cosAlpha = max(dot(E, R), 0.0);
	// 镜面反射
	result += specular * light.color * pow(cosAlpha, 5);

	return result;
}

vec3 CalPointLight(PointLight light, vec3 diffuse, vec3 specular, vec3 normal)
{
	vec3 light_dir = light.position - worldPosition;
	float distan = length(light_dir);
	light_dir = normalize(light_dir);

	vec3 E = normalize(vec3(0.0, 0.0, 0.0) - cameraPosition);
	vec3 R = reflect(-light_dir, normal);

	// 法线和光线的点积
	float cosTheta = max(dot(normal, light_dir), 0.0);
	// 漫反射
	vec3 result = diffuse * light.color * cosTheta / pow(distan, 2);

	// 光线反射与视线的方向重合度
	float cosAlpha = max(dot(E, R), 0.0);
	// 镜面反射
	result += specular * light.color * pow(cosAlpha, 5) / pow(distan, 2);

	return result;
}