#include <glm/glm.hpp>

#include <iostream>
#include <cmath>
#include <vector>

#include "pobject.h"

PBodies::PBodies(int size) {
    count = size;
    pos = new glm::vec3[size];
    vel = new glm::vec3[size];
    acc = new glm::vec3[size];
    color = new glm::vec3[size];
    mass = new float[size];
    radii = new float[size];
}

PBodies::~PBodies() {
    delete[] pos;
    delete[] vel;
    delete[] acc;
    delete[] color;
    delete[] mass;
    delete[] radii;
}

void PBodies::applyGravity(float dt) {
    int n = this->count;
    float t = dt * dt / 2.0f;
#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        float partial = 6.67408E-11f * mass[i];
        if (partial == 0) continue;
        for (int j = i + 1; j < n; j++) {
            // Direction x,y,z vectors
            float dx = pos[j].x - pos[i].x;
            float dy = pos[j].y - pos[i].y;
            float dz = pos[j].z - pos[i].z;

            float mag_sq = dx * dx + dy * dy + dz * dz;
            float mag = std::sqrt(mag_sq);

            // normalize the direction vector
            dx /= mag;
            dy /= mag;
            dz /= mag;

			float clamp = (radii[i] + radii[j]) / 2;
            if (mag < clamp) {
                float momentumX = mass[i]* vel[i].x + mass[j]* vel[j].x;
                float momentumY = mass[i]* vel[i].y + mass[j]* vel[j].y;
                float momentumZ = mass[i]* vel[i].z + mass[j]* vel[j].z;

                float massSum = mass[i] + mass[j];
                float groupVx = (momentumX / massSum);
                float groupVy = (momentumY / massSum);
                float groupVz = (momentumZ / massSum);

                vel[i].x = groupVx;
                vel[i].y = groupVy;
                vel[i].z = groupVz;

                vel[j].x = groupVx;
                vel[j].y = groupVy;
                vel[j].z = groupVz;
                continue;
            }
            // m^3 / (kg * s^2)
            float f_gravity = partial * (mass[j] / mag_sq);
            float fx = f_gravity * dx;
            float fy = f_gravity * dy;
            float fz = f_gravity * dz;

            // Calculate acceleration for this tick on both bodies
            acc[i].x += fx / mass[i];
            acc[i].y += fy / mass[i];
            acc[i].z += fz / mass[i];

            acc[j].x -= fx / mass[j];
            acc[j].y -= fy / mass[j];
            acc[j].z -= fz / mass[j];
        }

        // Update positions for next tick
        pos[i].x += vel[i].x * dt + (acc[i].x * t);
        pos[i].y += vel[i].y * dt + (acc[i].y * t);
        pos[i].z += vel[i].z * dt + (acc[i].z * t);

        // Update velocities for next tick
        vel[i].x += acc[i].x * dt;
        vel[i].y += acc[i].y * dt;
        vel[i].z += acc[i].z * dt;

        // Clear the acceleration for next tick
        acc[i].x = 0;
        acc[i].y = 0;
        acc[i].z = 0;
    }

}

void PBodies::applyGravity2(float dt) {
	int n = this->count;
	static const float G_CONSTANT = 6.67408E-11f;
    static const float EPS = 1e-6f;

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

			float f_gravity_j = (mass[j] * inv_mag_cubed); // Partial force due to jth body on ith body

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

void PBodies::printBody(int i) {
    std::cout << "(" << pos[i].x << ", " << pos[i].y << ", " << pos[i].z << "), "
              << "(" << vel[i].x << ", " << vel[i].y << ", " << vel[i].z << "), "
              << "(" << acc[i].x << ", " << acc[i].y << ", " << acc[i].z << ")";
}
