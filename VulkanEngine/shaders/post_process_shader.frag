#version 450

layout(set = 0, binding = 0) uniform sampler2D HDR_texture;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// 反色
vec3 InverseColor(vec3 color);
// Gaussian Blur 高斯模糊
vec3 GaussianBlur(vec2 uv, float width);
// 泛光
vec3 Bloom(vec2 uv, float width);

void main()
{
    vec3 HDRcolor = texture(HDR_texture, fragTexCoord).rgb;

    HDRcolor = GaussianBlur(fragTexCoord, 1280.0);

    // Reinhard 色调映射
    vec3 LDRcolor = HDRcolor / (HDRcolor + 1.0);

    // LDRcolor = InverseColor(LDRcolor);

    // Gamma 校正
    vec3 gamma = pow(LDRcolor, vec3(1.0/2.2));

	outColor = vec4(gamma, 1.0);
}

// 反色
vec3 InverseColor(vec3 color)
{
    return 1.0 - color;
}

// 泛光
vec3 Bloom(vec2 uv, float width)
{
    float dv = 1.0 / width;
    vec2 offset[9] = {
        {-dv,-dv}, {0,-dv}, {dv,-dv},
        {-dv,  0}, {0,  0}, {dv,  0},
        {-dv, dv}, {0, dv}, {dv, dv}
    };

    vec3 re_color = texture(HDR_texture, uv).rgb;

    for (int i=0; i < 9; i++)
    {
        vec3 frontiers_color = texture(HDR_texture, uv+offset[i]).rgb;
        if (frontiers_color.r > 1.0 && frontiers_color.g > 1.0 && frontiers_color.b > 1.0)
        {
            re_color += frontiers_color * 1.0/16.0;
        }
    }

    return re_color;
}

bool IsOutOfUVRange(vec2 uv)
{
    return uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0;
}

// Gaussian Blur 高斯模糊
vec3 GaussianBlur(vec2 uv, float width)
{
    // 3x3 卷积核
    // float kernel[9] = {
    //     1.0/16.0, 2.0/16.0, 1.0/16.0,
    //     2.0/16.0, 4.0/16.0, 2.0/16.0,
    //     1.0/16.0, 2.0/16.0, 1.0/16.0
    // };
    // 5x5 卷积核
    float kernel[25] = {
        1.0/100.0, 2.0/100.0,  4.0/100.0, 2.0/100.0, 1.0/100.0,
        2.0/100.0, 4.0/100.0,  8.0/100.0, 4.0/100.0, 2.0/100.0,
        4.0/100.0, 8.0/100.0, 16.0/100.0, 8.0/100.0, 4.0/100.0,
        2.0/100.0, 4.0/100.0,  8.0/100.0, 4.0/100.0, 2.0/100.0,
        1.0/100.0, 2.0/100.0,  4.0/100.0, 2.0/100.0, 1.0/100.0
    };

    float dv = 1.0 / width;
    // vec2 offset[9] = {
    //     {-dv,-dv}, {0,-dv}, {dv,-dv},
    //     {-dv,  0}, {0,  0}, {dv,  0},
    //     {-dv, dv}, {0, dv}, {dv, dv}
    // };
    vec2 offset[25] = {
        {-2*dv,-2*dv}, {-dv,-2*dv}, {0,-2*dv},{dv,-2*dv}, {2*dv,-2*dv},
        {-2*dv,  -dv}, {-dv,  -dv}, {0,  -dv},{dv,  -dv}, {2*dv,  -dv},
        {-2*dv,    0}, {-dv,    0}, {0,    0},{dv,    0}, {2*dv,    0},
        {-2*dv,   dv}, {-dv,   dv}, {0,   dv},{dv,   dv}, {2*dv,   dv},
        {-2*dv, 2*dv}, {-dv, 2*dv}, {0, 2*dv},{dv, 2*dv}, {2*dv, 2*dv}
    };

    vec3 re_color = vec3(0.0);

    for (int i=0; i < 25; i++)
    {
        if (IsOutOfUVRange(uv+offset[i])) { continue; }

        re_color += texture(HDR_texture, uv+offset[i]).rgb * kernel[i];
    }

    return re_color;
}
