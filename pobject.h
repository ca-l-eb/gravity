#ifndef POBJECT_HH
#define POBJECT_HH

#include <glm/glm.hpp>
#include <vector>

class PBodies
{
public:
    PBodies(int size);
    inline int size()
    {
        return count;
    }
    void applyGravity(float dt);
    void printBody(int index);

    std::vector<glm::vec3> pos, vel, acc, color;
    std::vector<float> mass;
    int count;
};

#endif
