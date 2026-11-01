//#pragma once
#ifndef _ANIMATION_H
#define _ANIMATION_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// 动画类
class Animation3D
{
public:
	struct KeyFrame
	{
		double time = 0.0;
		union
		{
			glm::vec3 translate;
			glm::quat rotation;
			glm::vec3 scaling;
		} transform;

		KeyFrame() = delete;
		KeyFrame(uint8_t flag)
		{
			switch (flag)
			{
			case 0:
				transform.translate = glm::vec3(0.f);
				break;
			case 1:
				transform.rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
				break;
			case 2:
				transform.scaling = glm::vec3(1.f);
				break;
			default:
				transform.rotation = glm::quat(0.f, 0.f, 0.f, 0.f);
			}
		}
	};

public:
	Animation3D() = default;
	~Animation3D() = default;

	Animation3D(const Animation3D& obj) = default;
	Animation3D& operator=(const Animation3D& obj) = default;

	/**
	 * @brief 更新动画时间
	 * @param delta_time - 时间差
	 * @return 若动画播放结束且不会循环，则返回1，否则返回0
	 */
	int Update(double delta_time);

	// 重置动画
	void Reset();

	/**
	 * @brief 获取某个时间点的关键帧
	 * @param key_time - 关键帧的时间点
	 * @param flag - 标志要获取的关键帧，0-位置、1-旋转、2-缩放
	 * @return 返回关键帧数组中符合的引用，若不存在这个时间点的关键帧，会在数组中构建一个
	 */
	KeyFrame& FindKeyFrame(double key_time, uint8_t flag);

	inline void SetAllTime(double time)
	{
		all_time = time;
	}
	// @param flag - 标志要获取的关键帧，0 - 位置、1 - 旋转、2 - 缩放
	inline std::vector<KeyFrame>& GetKeyFrames(uint8_t flag)
	{
		switch (flag)
		{
		case 0:
			return t_key_frames;
		case 1:
			return r_key_frames;
		case 2:
			return s_key_frames;
		default:
			return t_key_frames;
		}
	}

	// 用于存储动画更新后插值计算出的矩阵
	glm::mat4 inter_mat{1.f};

protected:
	// 动画的当前时间点
	double cur_time = 0.0;
	// 动画总时长
	double all_time = 0.0;
	// 关键帧索引
	uint32_t t_key_frame_idx = 1;
	uint32_t r_key_frame_idx = 1;
	uint32_t s_key_frame_idx = 1;
	// 动画关键帧数组
	std::vector<KeyFrame> t_key_frames{};
	std::vector<KeyFrame> r_key_frames{};
	std::vector<KeyFrame> s_key_frames{};

};

#endif // !_ANIMATION_H