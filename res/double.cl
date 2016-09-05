__kernel void vec_add(__global float* x, float f) {
    int id = get_global_id(0);
    x[id] = x[id] + f;
}
