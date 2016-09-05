#include <GL/glew.h>

#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <random>
#include <thread>
#include <mutex>
#include <fstream>
#include "display.h"
#include "pobject.h"
#include "shader.h"

static glm::vec3* makeCircleMesh(float r, int segments);
static void save_PBodies(PBodies& b, std::string filename);
static void handle_args(int argc, char* argv[], int& count, float& dt, float& step);
static void init_PBodies(PBodies& b);
static std::mutex mu;

float sign(float f) {
    if (f < 0) return -1;
    return 1;
}

static void save_PBodies(PBodies& b, std::string filename) {
	std::ofstream out(filename);
	for (int i = 0; i < b.size(); i++) {
		out << b.pos[i].x << "," << b.pos[i].y << "," << b.pos[i].z << "\n";
	}
	out.close();
}

static void doPhysics(PBodies* b, float dt, bool* updated, bool* running) {
    while (true) {
        b->applyGravity(dt);
        std::lock_guard<std::mutex> guard(mu);
		*updated = true; // Instance data needs updating... (in main thread)
        if (! *running) {
            break;
        }
    }
	std::cout << "Done with physics thread" << std::endl;
}

int main(int argc, char* argv[]) {
    int width = 1600;
    int height = 900;
    GLDisplay disp(width, height, "Hello, world!");
    if (disp.wasError()) {
        return disp.wasError();
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    float dt = 0.00005f; // Time step in seconds
    int count = 1 << 10; // Number of particles
    float step = float(M_PI / 300.0f); // Camera rotation in radians
    handle_args(argc, argv, count, dt, step);

    // 1000 ms / 100 fps = 10 ms / frame
	const int desired_fps_sleep = static_cast<int>(1000.0f / 100.0f);

    int segments = 10;
    //glm::vec3* circleMesh = makeCircleMesh(4, segments);
    //int circleMeshSize = segments * 3 * sizeof(glm::vec3);
    glm::vec3 particle[] = {{-0.5f, -0.5f, 0.0f},
                            {0.0f, 0.5f, 0.0f},
                            {0.5f, -0.5f, 0.0f} };

    PBodies b(count);
    init_PBodies(b); // Generate random locations for the particles

    Shader simpleShader("res/simple_mesh.vs", "res/simple_mesh.fs");
    simpleShader.use();

    // Time to link vertex data with the corresponding vertex attribute in the vertex shader
    GLint vertAttrib = simpleShader.getAttribLocation("vertices");
    GLint colorAttrib = simpleShader.getAttribLocation("inColor");
    GLint scaleAttrib = simpleShader.getAttribLocation("scale");
    GLint positionAttrib = simpleShader.getAttribLocation("position");

    GLint viewUniform = simpleShader.getUniformLocation("view");
    GLint projectionUniform = simpleShader.getUniformLocation("projection");


    GLuint vboVertices, vboPositions, vboColors, vboScale, vao;
    glGenVertexArrays(1, &vao); // Generate vao
    glGenBuffers(1, &vboVertices);
    glGenBuffers(1, &vboPositions);
    glGenBuffers(1, &vboColors);
    glGenBuffers(1, &vboScale);
    glBindVertexArray(vao);

    // Set up vertices / circle mesh
//    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
//    glBufferData(GL_ARRAY_BUFFER, circleMeshSize, circleMesh, GL_STATIC_DRAW);
//    glVertexAttribPointer(vertAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
//    glEnableVertexAttribArray(vertAttrib);

	// Single triangle as particle instead of circle as above
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle), particle, GL_STATIC_DRAW);
    glVertexAttribPointer(vertAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(vertAttrib);


    // Set up scale of circles
    glBindBuffer(GL_ARRAY_BUFFER, vboScale);
    glBufferData(GL_ARRAY_BUFFER, b.size() * sizeof(GLfloat), b.radii, GL_STATIC_DRAW);
    glVertexAttribPointer(scaleAttrib, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(scaleAttrib);

    // Set up color of circles
    glBindBuffer(GL_ARRAY_BUFFER, vboColors);
    glBufferData(GL_ARRAY_BUFFER, b.size() * sizeof(glm::vec3), b.color, GL_STATIC_DRAW);
    glVertexAttribPointer(colorAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(colorAttrib);

    // Set up offsets (positions of circles), needs to be updated every iteration
    glBindBuffer(GL_ARRAY_BUFFER, vboPositions);
    glBufferData(GL_ARRAY_BUFFER, b.size() * sizeof(glm::vec3), b.pos, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(positionAttrib);


    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Tell OpenGL about the instanced data (color, scale, and position)
    glVertexAttribDivisor(colorAttrib, 1);
    glVertexAttribDivisor(scaleAttrib, 1);
    glVertexAttribDivisor(positionAttrib, 1);


    // Wireframe rendering
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_DEPTH_TEST);

    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view;

    glm::mat4 perspectiveMatrix = glm::perspective(80.0f, (float) width / (float) height, 0.1f, 100.0f);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(perspectiveMatrix));

    bool updatedPosition = false;
    bool running = true;

    std::thread physicsThread(doPhysics, &b, dt, &updatedPosition, &running);

    float counter = 0;

	int frames = 1;
	auto now = std::chrono::high_resolution_clock::now();
    while (!disp.isClosed()) {
        disp.clear(0.0f, 0.0f, 0.0f, 1.0f);
        if (disp.wasResized()) {
            perspectiveMatrix = glm::perspective(80.0f, (float) disp.getWidth() / (float) disp.getHeight(), 0.1f, 100.0f);
            glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(perspectiveMatrix));
            glViewport(0, 0, disp.getWidth(), disp.getHeight());
        }
        // Update transformation camera
		view = glm::lookAt(glm::vec3(2 * sin(counter), 1.1f*sin(1.3*counter)*cos(.33f*counter), 2 * cos(counter)), cameraTarget, up);
		//view = glm::lookAt(glm::vec3(2.5 * sin(counter), 0.5f , 2.5 * cos(counter)), cameraTarget, up);
		glUniformMatrix4fv(viewUniform, 1, GL_FALSE, glm::value_ptr(view));

        simpleShader.use(); // Bind the simple shader

        glBindVertexArray(vao);

        {
            // We don't want the other thread messing with data when we're moving it to the GPU
            std::lock_guard<std::mutex> guard(mu);
            if (updatedPosition) {
                glBindBuffer(GL_ARRAY_BUFFER, vboPositions);
                glBufferSubData(GL_ARRAY_BUFFER, 0, b.size() * sizeof(glm::vec3), (GLvoid *) b.pos);
                updatedPosition = false;
            }
        } // Release the guard


        // Draw the instanced particle data
        //glDrawArraysInstanced(GL_TRIANGLES, 0, circleMeshSize/sizeof(glm::vec3), count);
		glDrawArraysInstanced(GL_TRIANGLES, 0, sizeof(particle), count);

        disp.update();
		frames++;
		if (frames >= 60) {
			auto prev = now;
			now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> duration = (now - prev);
			std::cout << 60.0 / duration.count() << " fps" << std::endl;
			frames = 0;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(desired_fps_sleep));
        counter += step;
    }

    running = false;

    physicsThread.join();

    return 0;
}

// Circle centered at (0, 0, 0) with radius r
static glm::vec3* makeCircleMesh(float r, int segments) {
    glm::vec3* vertices = new glm::vec3[segments * 3];
    float theta = 2.0f * M_PI / float(segments);
    float angle = 0;
    for (int i = 0; i < 3 * segments; i += 3) {
        vertices[i] = {0.0f, 0.0f, 0.0f};
        float x = r*cos(angle);
        float y = r*sin(angle);
        vertices[i+1] = {x, y, 0.0f};
        angle += theta;
        x = r*cos(angle);
        y = r*sin(angle);
        vertices[i+2] = {x, y, 0.0f};
    }
    return vertices;
}

static void handle_args(int argc, char* argv[], int& count, float& dt, float& step) {
    if (argc == 2) {
        std::string fs(argv[1]);
        step = std::stof(fs);
    }
    if (argc == 3) {
        std::string is(argv[1]);
        count = std::stoi(is);
        std::string fs(argv[2]);
        dt = std::stof(fs);
    }
    if (argc == 4) {
        std::string is(argv[1]);
        count = std::stoi(is);
        std::string fs(argv[2]);
        dt = std::stof(fs);
        fs = std::string(argv[3]);
        step = std::stof(fs) / 300.0f;
    }
}

static void init_PBodies(PBodies& b) {
    int count = b.size();
    float range = 0.2f;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(-range, range);
    for (int i = 0; i < count-1; i++) {
        float randX = dist(gen);
        float randY = dist(gen);
        float randZ = dist(gen);
        if (i < count/2) { // block 1
            b.pos[i] = { randX - 1.3f, randY, randZ };
            b.vel[i] = { 0.0f, 110.0f, 0.0f }; // Good looping
//			b.vel[i] = { 0.0f, 160.0f, 0.0f }; // Good mixing
            b.color[i] = { 0.0f, 1.0f, 0.0f };
        }
        else if (i < count) { // block 2
            b.pos[i] = { randX + 1.3f, randY, randZ };
            b.vel[i] = { 0.0f, -110.0f, 0.0f };
//			b.vel[i] = { 0.0f, -160.0f, 0.0f };
            b.color[i] = { 1.0f, 0.0f, 1.0f };
        }
        float mass = fabs(dist(gen) * 9.5e8f);
        b.mass[i] = mass;
        b.acc[i] = {0, 0, 0};
        b.radii[i] = pow(mass, 0.333) * 6.9e-6f;
        //b.color[i] = { fabs(dist(gen)), fabs(dist(gen)), fabs(dist(gen)) };
    }
    b.pos[count-1] = {0.0f, 0.0f, 0.0f};
    b.mass[count-1] = 5e14f;
    b.color[count-1] = {1.0f, 1.0f, 1.0f};
    b.radii[count-1] = pow(1e14f, .333) * 2.9e-6f;
}