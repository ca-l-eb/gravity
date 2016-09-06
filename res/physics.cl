__kernel void apply_gravity(__global float* pos,
                            __global float* vel,
                            __global float* acc,
                            __global float* mass,
                            __global int* size) {
    int id = get_global_id(0);
    int loc = id * sizeof(float) * 3;

    float px = pos[loc];
    float py = pos[loc + 1];
    float pz = pos[loc + 2];
    int n = size[0];

    float EPS = 1e-6f;
    for (int j = 0; j < n; j++) {
        int loc_j = j * sizeof(float) * 3;
        float dx = pos[loc_j]     - px;
        float dy = pos[loc_j + 1] - py;
        float dz = pos[loc_j + 2] - pz;

        float mag_sq = dx * dx + dy * dy + dz * dz + EPS;
        float mag_sixth = mag_sq * mag_sq * mag_sq;

        float inv_mag_cubed = rsqrt(mag_sixth);
        float f_gravity_j = (mass[j] * inv_mag_cubed); // Partial force due to jth body on ith body

        // Accumulate forces of all other particles on work-item particle given by id
        acc[loc]     += dx * f_gravity_j;
        acc[loc + 1] += dy * f_gravity_j;
        acc[loc + 2] += dz * f_gravity_j;
    }
}

// Call after apply_gravity kernel is completed
__kernel void update_positions(__global float* pos,
                               __global float* vel,
                               __global float* acc,
                               __global float* dt) {

    float G_CONSTANT = 6.67408E-11f;
    int id = get_global_id(0);
    float t = dt[0];

    // We need to know how far apart each component is... data is glm::vec3 format on the GPU
    int loc = id * sizeof(float) * 3;
    float gaxdt = G_CONSTANT * acc[loc]     * t;
    float gaydt = G_CONSTANT * acc[loc + 1] * t;
    float gazdt = G_CONSTANT * acc[loc + 2] * t;

    pos[loc]     += vel[loc]     * t + (gaxdt * t * 0.5f);
    pos[loc + 1] += vel[loc + 1] * t + (gaydt * t * 0.5f);
    pos[loc + 2] += vel[loc + 2] * t + (gazdt * t * 0.5f);

    // Update velocities for next tick
    vel[loc]     += gaxdt;
    vel[loc + 1] += gaydt;
    vel[loc + 2] += gazdt;

    // Clear the acceleration for next tick
    acc[loc]     = 0.0f;
    acc[loc + 1] = 0.0f;
    acc[loc + 2] = 0.0f;
}
