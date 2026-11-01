#include <SDL3/SDL.h>

#include "Animation.h"

int Animation3D::Update(double delta_time)
{
	cur_time += delta_time;

	if (cur_time >= all_time)
	{
		return 1;
	}

	Animation3D::KeyFrame* prev;
	Animation3D::KeyFrame* last;
	double a;
	glm::vec3 translate = t_key_frames[t_key_frame_idx - 1].transform.translate;
	glm::quat qrotation = r_key_frames[r_key_frame_idx - 1].transform.rotation;
	glm::vec3 scaling = s_key_frames[s_key_frame_idx - 1].transform.scaling;

	if (s_key_frame_idx < s_key_frames.size())
	{
		if (cur_time > s_key_frames[s_key_frame_idx].time)
		{
			s_key_frame_idx += 1;
		}
		prev = &s_key_frames[s_key_frame_idx - 1];
		last = &s_key_frames[s_key_frame_idx];
		a = (cur_time - prev->time) / (last->time - prev->time);
		scaling = glm::mix(prev->transform.scaling, last->transform.scaling, a);
	}

	if (r_key_frame_idx < r_key_frames.size())
	{
		if (cur_time > r_key_frames[r_key_frame_idx].time)
		{
			r_key_frame_idx += 1;
		}
		prev = &r_key_frames[r_key_frame_idx - 1];
		last = &r_key_frames[r_key_frame_idx];
		a = (cur_time - prev->time) / (last->time - prev->time);
		qrotation = glm::slerp(prev->transform.rotation, last->transform.rotation, (float)a);
	}

	if (t_key_frame_idx < t_key_frames.size())
	{
		if (cur_time > t_key_frames[t_key_frame_idx].time)
		{
			t_key_frame_idx += 1;
		}
		prev = &t_key_frames[t_key_frame_idx - 1];
		last = &t_key_frames[t_key_frame_idx];
		a = (cur_time - prev->time) / (last->time - prev->time);
		translate = glm::mix(prev->transform.translate, last->transform.translate, a);
	}


	glm::mat4 T = glm::translate(glm::mat4(1.f), translate);
	glm::mat4 R = glm::mat4_cast(qrotation);
	glm::mat4 S = glm::scale(glm::mat4(1.f), scaling);

	inter_mat = T * R * S;

	return 0;
}

void Animation3D::Reset()
{
	cur_time = 0.0;
	t_key_frame_idx = 1;
	r_key_frame_idx = 1;
	s_key_frame_idx = 1;
	inter_mat = glm::mat4(1.f);
}

Animation3D::KeyFrame& Animation3D::FindKeyFrame(double key_time, uint8_t flag)
{
	std::vector<Animation3D::KeyFrame>* key_frame_array;

	switch (flag)
	{
	case 0:
		key_frame_array = &t_key_frames;
		break;
	case 1:
		key_frame_array = &r_key_frames;
		break;
	case 2:
		key_frame_array = &s_key_frames;
		break;
	default:
		key_frame_array = &t_key_frames;
		break;
	}

	for (Animation3D::KeyFrame& key: *key_frame_array)
	{
		if (key.time == key_time)
		{
			return key;
		}
	}

	Animation3D::KeyFrame new_key{flag};
	switch (flag)
	{
	case 0:
		new_key.time = key_time;
		key_frame_array->push_back(new_key);
		break;
	case 1:
		new_key.time = key_time;
		key_frame_array->push_back(new_key);
		break;
	case 2:
		new_key.time = key_time;
		key_frame_array->push_back(new_key);
		break;
	default:
		new_key.time = key_time;
		key_frame_array->push_back(new_key);
		break;
	}


	return key_frame_array->back();
}
