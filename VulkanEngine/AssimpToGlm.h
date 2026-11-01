//#pragma once
#ifndef _ASSIMP_TO_GLM_H
#define _ASSIMP_TO_GLM_H

#include <string.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/scene.h>

inline glm::mat4 aiToGlm(aiMatrix4x4& aimat)
{
	glm::mat4 glmmat;
	/*(
		aimat.a1, aimat.b1, aimat.c1, aimat.d1,
		aimat.a2, aimat.b2, aimat.c2, aimat.d2,
		aimat.a3, aimat.b3, aimat.c3, aimat.d3,
		aimat.a4, aimat.b4, aimat.c4, aimat.d4
	)*/

	memcpy_s(&glmmat, 64, &aimat, 64);

	return glm::transpose(glmmat);
}

inline glm::quat aiToGlm(aiQuaternion& aiquat)
{
	return glm::quat(aiquat.w, aiquat.x, aiquat.y, aiquat.z);
}

inline glm::vec3 aiToGlm(aiVector3D& aivec3)
{
	return glm::vec3(aivec3.x, aivec3.y, aivec3.z);
}

#endif // !_ASSIMP_TO_GLM_H

