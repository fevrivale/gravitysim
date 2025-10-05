#ifndef header_H
#define header_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <mutex>

class Object {
public:
    GLuint VAO, VBO;
    int vertexCount;
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 LastPos;
    float mass;
    float density;
    float radius;
    glm::vec4 color;
    bool glow;
    bool Initalizing;
    bool Launched;
    bool target;

    Object(glm::vec3 initPosition = glm::vec3(0.0f),
        glm::vec3 initVelocity = glm::vec3(0.0f),
        float mass = 1e22f,
        float density = 5515.0f,
        glm::vec4 color = glm::vec4(1.0f),
        bool Glow = false);

    std::vector<float> Draw();
    void UpdatePos();
    void UpdateVertices();
    glm::vec3 GetPos() const;
    void accelerate(float x, float y, float z);
    float CheckCollision(const Object& other);
};

// Global variables
extern bool running;
extern bool pause;
extern glm::vec3 cameraPos;
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;
extern float lastX, lastY;
extern float camYaw;
extern float camPitch;
extern float deltaTime;
extern float lastFrame;
extern bool usingMenu;
extern std::vector<Object> objs;
extern float sizeRatio;
extern std::mutex objsMutex;
extern const double G;
extern float initMass;

// Function declarations
GLFWwindow* StartGLU();
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount);
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
glm::vec3 sphericalToCartesian(float r, float theta, float phi);

#endif
