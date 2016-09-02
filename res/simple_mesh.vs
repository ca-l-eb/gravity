#version 330

in vec3 vertices;
in vec3 position;
in vec3 inColor;
in float scale;

uniform mat4 view, projection;

out vec3 fColor;

void main() {
    gl_Position = projection * view * vec4(vertices*scale + position, 1.0);
    fColor = inColor;
}