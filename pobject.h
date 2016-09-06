#ifndef POBJECT_HH
#define POBJECT_HH

#include <glm/glm.hpp>

class PBodies {
public:
    PBodies(int size);
    ~PBodies();
	inline int size() { return count; }
	void applyGravity(float dt);
	void printBody(int index);

	glm::vec3 *pos, *vel, *acc, *color;
	float *mass, *radii;
	int count;
};

#endif
