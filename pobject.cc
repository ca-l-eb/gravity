#include <glm/glm.hpp>

#include <cmath>
#include <iostream>
#include <vector>

#include "pobject.h"

PBodies::PBodies(int size)
{
    count = size;
    pos.resize(size);
    vel.resize(size);
    acc.resize(size);
    color.resize(size);
    mass.resize(size);
}

void PBodies::applyGravity(float dt)
{
    static const float G_CONSTANT = 6.67408E-11f;
    static const float EPS = 1e-6f;

    int n = this->count;
    glm::vec3 *pos = this->pos.data();
    glm::vec3 *vel = this->vel.data();
    glm::vec3 *acc = this->acc.data();
    float *mass = this->mass.data();

#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            //			if (j == i) continue;
            // Direction x,y,z vectors
            float dx = pos[j].x - pos[i].x;
            float dy = pos[j].y - pos[i].y;
            float dz = pos[j].z - pos[i].z;

            float mag_sq = dx * dx + dy * dy + dz * dz + EPS;
            float mag_sixth = mag_sq * mag_sq * mag_sq;

            // Inverse cube = 1/r^2 (Newton's equation) * 1/r (normalize the direction vectors
            float inv_mag_cubed = 1.0f / std::sqrt(mag_sixth);

            // We dont need to multiply by i's mass because we will eventually be dividing
            // it away when calculating the acceleration due to gravity (F=ma -> a=F/m)

            float f_gravity_j =
                (mass[j] * inv_mag_cubed);  // Partial force due to jth body on ith body

            // Accumulate forces for this tick
            acc[i].x += dx * f_gravity_j;
            acc[i].y += dy * f_gravity_j;
            acc[i].z += dz * f_gravity_j;
        }
    }

#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        // Update positions for next tick (x(t) = x0 + v0*t + 1/2 at^2)
        pos[i].x += vel[i].x * dt + (G_CONSTANT * acc[i].x * dt * dt * 0.5f);
        pos[i].y += vel[i].y * dt + (G_CONSTANT * acc[i].y * dt * dt * 0.5f);
        pos[i].z += vel[i].z * dt + (G_CONSTANT * acc[i].z * dt * dt * 0.5f);

        // Update velocities for next tick
        vel[i].x += G_CONSTANT * acc[i].x * dt;
        vel[i].y += G_CONSTANT * acc[i].y * dt;
        vel[i].z += G_CONSTANT * acc[i].z * dt;

        // Clear the acceleration for next tick
        acc[i].x = 0;
        acc[i].y = 0;
        acc[i].z = 0;
    }
}

void PBodies::printBody(int i)
{
    std::cout << "(" << pos[i].x << ", " << pos[i].y << ", " << pos[i].z << "), "
              << "(" << vel[i].x << ", " << vel[i].y << ", " << vel[i].z << "), "
              << "(" << acc[i].x << ", " << acc[i].y << ", " << acc[i].z << ")";
}
