#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>
#include <limits>
#include <cmath>

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Earth structure
struct Continent {
    std::string name;
    float population;
    glm::vec3 color;
    std::vector<glm::vec2> coordinates;
};

struct EarthInfo {
    std::vector<Continent> continents;
    float totalPopulation;

    EarthInfo() {
        totalPopulation = 0.0f;
        continents = {
            {"Asia", 4700.0f, glm::vec3(0.2f, 0.8f, 0.2f), {{10, 55}, {60, 150}}},
            {"Africa", 1400.0f, glm::vec3(0.9f, 0.7f, 0.1f), {{-35, 37}, {-18, 52}}},
            {"Europe", 750.0f, glm::vec3(0.3f, 0.3f, 1.0f), {{36, -10}, {71, 60}}},
            {"North America", 600.0f, glm::vec3(0.8f, 0.2f, 0.2f), {{7, -168}, {84, -52}}},
            {"South America", 430.0f, glm::vec3(1.0f, 0.5f, 0.0f), {{-56, -82}, {13, -35}}},
            {"Australia", 26.0f, glm::vec3(1.0f, 0.0f, 1.0f), {{-44, 113}, {-10, 154}}}
        };
        for (const auto& continent : continents) {
            totalPopulation += continent.population;
        }
    }
};

// Collision effects
struct CollisionEffect {
    glm::vec3 position;
    float radius;
    float intensity;
    float lifetime;
    glm::vec4 color;
};

struct ImpactZone {
    glm::vec3 position;
    float radius;
    float intensity;
    float lifetime;
    glm::vec4 color;
};

const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec3 Normal;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(aPos);
})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;
uniform vec4 objectColor;
uniform bool isGrid;
uniform bool GLOW;

void main() {
    if (isGrid) {
        FragColor = objectColor;
    } else if(GLOW){
        FragColor = vec4(objectColor.rgb * 5.0, objectColor.a);
    } else {
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
        float minBrightness = 0.6;
        vec3 baseColor = mix(objectColor.rgb, vec3(1.0), 1.0 - minBrightness);
        float diff = max(dot(Normal, lightDir), 0.0);
        diff = mix(minBrightness, 1.0, diff);
        vec3 finalColor = baseColor * diff;
        FragColor = vec4(finalColor, objectColor.a);
    }
})glsl";

// Global variables
bool running = true;
bool pause = false;
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5000.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
double lastX = 400.0, lastY = 300.0;
float yaw = -90.0f;
float pitch = 0.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool firstMouse = true;
float timeScale = 1.0f;


const double G = 6.6743e-11;
const float c = 299792458.0f;
float initMass = float(powf(10, 22));
float sizeRatio = 30000.0f;

// Menu state variables
bool showMenu = false;
bool isFullscreen = true;
float asteroidVelocity = 15000.0f;
float asteroidRadius = 5.0f;
float asteroidMass = 1e15f;
glm::vec3 earthPosition = glm::vec3(0, 500.0f, 0);
glm::vec3 asteroidColor = glm::vec3(0.5f, 0.5f, 0.5f);

// Earth properties
float earthMass = 5.972e24f;
glm::vec3 originalEarthPosition = glm::vec3(0, 2000.0f, 0);
glm::vec3 originalEarthVelocity = glm::vec3(0, 0, 0);

// Global instances
EarthInfo earth;
std::vector<CollisionEffect> collisionEffects;
std::vector<ImpactZone> impactZones;
std::vector<class Object> objs = {};

// Key state tracking
bool keys[1024] = { false };

// Window management
GLFWwindow* mainWindow = nullptr;
int windowWidth = 1920;
int windowHeight = 1080;

// FUNCTION IMPLEMENTATIONS
glm::vec3 sphericalToCartesian(float r, float theta, float phi) {
    float x = r * sin(theta) * cos(phi);
    float y = r * cos(theta);
    float z = r * sin(theta) * sin(phi);
    return glm::vec3(x, y, z);
}

void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

class Object {
public:
    GLuint VAO, VBO;
    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::vec3 velocity = glm::vec3(0, 0, 0);
    size_t vertexCount;
    glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    bool Initalizing = false;
    bool Launched = false;
    bool target = false;
    bool userDefinedRadius = false;
    bool shouldExplode = false;
    float explosionTimer = 0.0f;
    float rotationAngle = 0.0f;
    float rotationSpeed = 0.5f;

    float mass;
    float density;
    float radius;

    glm::vec3 LastPos = position;
    bool glow;
    bool isEarth = false;

    Object(glm::vec3 initPosition, glm::vec3 initVelocity, float mass, float density = 3344, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), bool Glow = false, bool earth = false) {
        this->position = initPosition;
        this->velocity = initVelocity;
        this->mass = mass;
        this->density = density;
        this->radius = powf(((3.0f * this->mass / this->density) / (4.0f * 3.14159265359f)), (1.0f / 3.0f)) / sizeRatio;
        this->color = color;
        this->glow = Glow;
        this->isEarth = earth;

        std::vector<float> vertices = Draw();
        vertexCount = vertices.size();
        CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
    }

    std::vector<float> Draw() {
        std::vector<float> vertices;
        int stacks = 20;
        int sectors = 20;

        for (float i = 0.0f; i <= stacks; ++i) {
            float theta1 = (i / stacks) * glm::pi<float>();
            float theta2 = (i + 1) / stacks * glm::pi<float>();
            for (float j = 0.0f; j < sectors; ++j) {
                float phi1 = j / sectors * 2 * glm::pi<float>();
                float phi2 = (j + 1) / sectors * 2 * glm::pi<float>();
                glm::vec3 v1 = sphericalToCartesian(this->radius, theta1, phi1);
                glm::vec3 v2 = sphericalToCartesian(this->radius, theta1, phi2);
                glm::vec3 v3 = sphericalToCartesian(this->radius, theta2, phi1);
                glm::vec3 v4 = sphericalToCartesian(this->radius, theta2, phi2);

                vertices.insert(vertices.end(), { v1.x, v1.y, v1.z });
                vertices.insert(vertices.end(), { v2.x, v2.y, v2.z });
                vertices.insert(vertices.end(), { v3.x, v3.y, v3.z });

                vertices.insert(vertices.end(), { v2.x, v2.y, v2.z });
                vertices.insert(vertices.end(), { v4.x, v4.y, v4.z });
                vertices.insert(vertices.end(), { v3.x, v3.y, v3.z });
            }
        }
        return vertices;
    }

    void UpdatePos() {
        this->position[0] += this->velocity[0] * deltaTime * timeScale;
        this->position[1] += this->velocity[1] * deltaTime * timeScale;
        this->position[2] += this->velocity[2] * deltaTime * timeScale;

        if (!userDefinedRadius) {
            this->radius = powf(((3.0f * this->mass / this->density) / (4.0f * 3.14159265359f)), (1.0f / 3.0f)) / sizeRatio;
        }
        if (isEarth) {
            rotationAngle += rotationSpeed * deltaTime * timeScale;
        }
    }

    void UpdateVertices() {
        std::vector<float> vertices = Draw();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    }

    glm::vec3 GetPos() const {
        return this->position;
    }

    bool CheckCollision(Object& other) {
        float dx = other.position[0] - this->position[0];
        float dy = other.position[1] - this->position[1];
        float dz = other.position[2] - this->position[2];
        float distance = sqrt(dx * dx + dy * dy + dz * dz);
        float minDistance = other.radius + this->radius;

        if (distance < minDistance) {
            glm::vec3 collisionPoint = this->position + (other.position - this->position) * 0.5f;

            glm::vec3 relativeVel = this->velocity - other.velocity;
            float impactSpeed = glm::length(relativeVel);
            float kineticEnergy = 0.5f * this->mass * impactSpeed * impactSpeed;

            CollisionEffect effect;
            effect.position = collisionPoint;
            effect.radius = minDistance * 2.0f;
            effect.intensity = kineticEnergy / 1e25f;
            effect.lifetime = 1.0f;
            effect.color = glm::vec4(1.0f, 1.0f, 0.0f, 0.8f);
            collisionEffects.push_back(effect);

            float massRatio = this->mass / other.mass;
            bool thisExplodes = (massRatio < 0.5f) || (impactSpeed > 1000.0f && this->mass < other.mass);
            bool otherExplodes = (massRatio > 2.0f) || (impactSpeed > 1000.0f && other.mass < this->mass);

            if (thisExplodes) {
                this->shouldExplode = true;
                this->explosionTimer = 0.5f;
            }
            if (otherExplodes) {
                other.shouldExplode = true;
                other.explosionTimer = 0.5f;
            }

            ImpactZone zone;
            zone.position = collisionPoint;
            zone.radius = minDistance * 3.0f;
            zone.intensity = effect.intensity;
            zone.lifetime = 5.0f;

            if (zone.intensity < 0.3f) {
                zone.color = glm::vec4(0.0f, 1.0f, 0.0f, 0.3f);
            }
            else if (zone.intensity < 0.7f) {
                zone.color = glm::vec4(1.0f, 0.5f, 0.0f, 0.4f);
            }
            else {
                zone.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
            }
            impactZones.push_back(zone);

            std::cout << "COLLISION! Impact Speed: " << impactSpeed
                << ", Energy: " << kineticEnergy
                << ", Destruction Level: " << zone.intensity << std::endl;

            return true;
        }
        return false;
    }
};

GLuint gridVAO, gridVBO;

void ResetEarth() {
    for (auto it = objs.begin(); it != objs.end(); ) {
        if (it->isEarth) {
            it = objs.erase(it);
        }
        else {
            ++it;
        }
    }

    Object earthObj(originalEarthPosition, originalEarthVelocity, earthMass, 5515, glm::vec4(0.1f, 0.3f, 0.8f, 1.0f), false, true);
    earthObj.radius = 27.0f;
    earthObj.userDefinedRadius = true;
    earthObj.UpdateVertices();
    objs.push_back(earthObj);

    std::cout << "EARTH RESET! Mass: " << earthMass << ", Position: (0, 500, 0)" << std::endl;
}

void ToggleFullscreen(GLFWwindow* window) {
    isFullscreen = !isFullscreen;

    if (isFullscreen) {
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
        glfwSetWindowMonitor(window, primaryMonitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
        windowWidth = videoMode->width;
        windowHeight = videoMode->height;
    }
    else {
        // Windowed mode with some padding
        int monitorX, monitorY;
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
        glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);

        int newWidth = videoMode->width * 3 / 4;
        int newHeight = videoMode->height * 3 / 4;
        int newX = monitorX + (videoMode->width - newWidth) / 2;
        int newY = monitorY + (videoMode->height - newHeight) / 2;

        glfwSetWindowMonitor(window, nullptr, newX, newY, newWidth, newHeight, videoMode->refreshRate);
        windowWidth = newWidth;
        windowHeight = newHeight;
    }

    // Update viewport
    glViewport(0, 0, windowWidth, windowHeight);

    // Update projection matrix
    GLuint shaderProgram = *static_cast<GLuint*>(glfwGetWindowUserPointer(window));
    glUseProgram(shaderProgram);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)windowWidth / (float)windowHeight, 0.1f, 100000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void RenderMenu() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Asteroid Creator", &showMenu, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("ASTEROID PARAMETERS");
        ImGui::Separator();

        ImGui::Text("Mass: %.2e kg", asteroidMass);
        ImGui::SliderFloat("##Mass", &asteroidMass, 1e12f, 1e20f, "%.2e");
        ImGui::SameLine(); ImGui::Text("(R/F)");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Range: 1 billion to 100 quintillion kg");

        ImGui::Text("Radius: %.1f m", asteroidRadius);
        ImGui::SliderFloat("##Radius", &asteroidRadius, 1.0f, 50.0f, "%.1f");
        ImGui::SameLine(); ImGui::Text("(LEFT/RIGHT)");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Range: 1m to 50m (small asteroids)");

        ImGui::Text("Velocity: %.0f m/s", asteroidVelocity);
        ImGui::SliderFloat("##Velocity", &asteroidVelocity, 1000.0f, 50000.0f, "%.0f");
        ImGui::SameLine(); ImGui::Text("(UP/DOWN)");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Range: 1 km/s to 50 km/s");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("ASTEROID COLOR");
        ImGui::Separator();

        ImVec4 currentColor = ImVec4(asteroidColor.r, asteroidColor.g, asteroidColor.b, 1.0f);
        ImGui::ColorButton("Current Color", currentColor, ImGuiColorEditFlags_NoPicker, ImVec2(100, 30));
        ImGui::SameLine();
        ImGui::Text("Preview");

        ImGui::Text("Adjust RGB Values:");
        ImGui::SliderFloat("Red##Color", &asteroidColor.r, 0.0f, 1.0f, "R: %.2f");
        ImGui::SliderFloat("Green##Color", &asteroidColor.g, 0.0f, 1.0f, "G: %.2f");
        ImGui::SliderFloat("Blue##Color", &asteroidColor.b, 0.0f, 1.0f, "B: %.2f");

        ImGui::Text("Asteroid Type Presets:");
        ImVec2 buttonSize = ImVec2(60, 30);

        if (ImGui::Button("Rocky##Preset", buttonSize)) asteroidColor = glm::vec3(0.5f, 0.5f, 0.5f);
        ImGui::SameLine();
        if (ImGui::Button("Iron##Preset", buttonSize)) asteroidColor = glm::vec3(0.6f, 0.4f, 0.3f);
        ImGui::SameLine();
        if (ImGui::Button("Ice##Preset", buttonSize)) asteroidColor = glm::vec3(0.8f, 0.9f, 1.0f);

        if (ImGui::Button("Carbon##Preset", buttonSize)) asteroidColor = glm::vec3(0.2f, 0.2f, 0.2f);
        ImGui::SameLine();
        if (ImGui::Button("Custom##Preset", buttonSize)) asteroidColor = glm::vec3(0.7f, 0.3f, 0.1f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("QUICK PRESETS");
        ImGui::Separator();

        if (ImGui::Button("Small Pebble (1e12 kg)", ImVec2(200, 25))) {
            asteroidMass = 1e12f;
            asteroidRadius = 1.0f;
            asteroidVelocity = 20000.0f;
        }

        if (ImGui::Button("Medium Asteroid (1e15 kg)", ImVec2(200, 25))) {
            asteroidMass = 1e15f;
            asteroidRadius = 5.0f;
            asteroidVelocity = 15000.0f;
        }

        if (ImGui::Button("Large Asteroid (1e18 kg)", ImVec2(200, 25))) {
            asteroidMass = 1e18f;
            asteroidRadius = 20.0f;
            asteroidVelocity = 10000.0f;
        }

        if (ImGui::Button("Extinction Event (1e20 kg)", ImVec2(200, 25))) {
            asteroidMass = 1e20f;
            asteroidRadius = 50.0f;
            asteroidVelocity = 30000.0f;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("DISPLAY SETTINGS");
        ImGui::Separator();

        if (ImGui::Button(isFullscreen ? "Switch to Windowed (F11)" : "Switch to Fullscreen (F11)", ImVec2(250, 30))) {
            ToggleFullscreen(mainWindow);
        }


        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("SIMULATION SPEED");
        ImGui::Separator();

        ImGui::Text("Time Scale: %.2f", timeScale);
        ImGui::SliderFloat("##TimeScale", &timeScale, 0.01f, 2.0f, "%.2f");
        ImGui::SameLine(); ImGui::Text("(T/Y)");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "0.01x to 2x speed");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("EARTH CONTROLS");
        ImGui::Separator();

        ImGui::Text("Earth Mass: %.2e kg", earthMass);
        if (ImGui::Button("Reset Earth (E)")) {
            ResetEarth();
        }
        ImGui::SameLine();
        ImGui::Text("Resets Earth to original position");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("CONTROLS");
        ImGui::Separator();

        ImGui::BulletText("M - Toggle this menu");
        ImGui::BulletText("F11 - Toggle Fullscreen");
        ImGui::BulletText("LEFT/RIGHT - Adjust Radius");
        ImGui::BulletText("UP/DOWN - Adjust Velocity");
        ImGui::BulletText("R/F - Adjust Mass");
        ImGui::BulletText("E - Reset Earth");
        ImGui::BulletText("Left Click - Create asteroid");
        ImGui::BulletText("WASD + Space/Shift - Move camera");
        ImGui::BulletText("Mouse - Look around");
        ImGui::BulletText("Q - Quit");
    }

    ImGui::End();
}

GLFWwindow* StartGLU() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW, panic" << std::endl;
        return nullptr;
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

    // Start in fullscreen mode
    mainWindow = glfwCreateWindow(videoMode->width, videoMode->height, "3D Gravity Simulation - Asteroid Impact", primaryMonitor, NULL);
    if (!mainWindow) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(mainWindow);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Initialize ImGui for GLFW
    ImGui_ImplGlfw_InitForOpenGL(mainWindow, false);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, videoMode->width, videoMode->height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return mainWindow;
}

GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    glUseProgram(shaderProgram);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void processCameraMovement() {
    float cameraSpeed = 5000.0f * deltaTime;

    if (keys[GLFW_KEY_W]) cameraPos += cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_S]) cameraPos -= cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_A]) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys[GLFW_KEY_D]) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys[GLFW_KEY_SPACE]) cameraPos += cameraUp * cameraSpeed;
    if (keys[GLFW_KEY_LEFT_SHIFT]) cameraPos -= cameraUp * cameraSpeed;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

    // Let ImGui handle the key first
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return;
    }

    if (action == GLFW_PRESS) {
        keys[key] = true;
    }
    else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        showMenu = !showMenu;
        firstMouse = true;
    }

    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        ToggleFullscreen(window);
    }

    if (key == GLFW_KEY_K && action == GLFW_PRESS) {
        pause = !pause;
        std::cout << "Simulation " << (pause ? "PAUSED" : "RUNNING") << std::endl;
    }

    if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
        running = false;
    }

    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        ResetEarth();
    }

    // Only process menu controls if menu is open
    if (showMenu) {
        if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
            asteroidVelocity += 1000.0f;
            if (asteroidVelocity > 50000.0f) asteroidVelocity = 50000.0f;
        }
        if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
            asteroidVelocity -= 1000.0f;
            if (asteroidVelocity < 1000.0f) asteroidVelocity = 1000.0f;
        }
        if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
            asteroidRadius += 2.0f;
            if (asteroidRadius > 50.0f) asteroidRadius = 50.0f;
        }
        if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
            asteroidRadius -= 2.0f;
            if (asteroidRadius < 1.0f) asteroidRadius = 1.0f;
        }
        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            asteroidMass *= 10.0f;
            if (asteroidMass > 1e20f) asteroidMass = 1e20f;
        }
        if (key == GLFW_KEY_F && action == GLFW_PRESS) {
            asteroidMass /= 10.0f;
            if (asteroidMass < 1e12f) asteroidMass = 1e12f;
        }

        if (key == GLFW_KEY_T && action == GLFW_PRESS) {
            timeScale = std::max(0.01f, timeScale * 0.5f);
        }
        if (key == GLFW_KEY_Y && action == GLFW_PRESS) {
            timeScale = std::min(2.0f, timeScale * 2.0f);
        }

    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {

    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

    // Let ImGui handle the mouse first
    if (ImGui::GetIO().WantCaptureMouse) {
        firstMouse = true;
        return;
    }

    // Don't move camera if menu is open
    if (showMenu) {
        firstMouse = true;
        return;
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = float(xpos - lastX);
    float yoffset = float(lastY - ypos);
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    // Let ImGui handle mouse clicks first
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Create object at fixed distance in front of camera
        glm::vec3 spawnPos = cameraPos + cameraFront * 1500.0f;

        // Give it initial velocity toward where we're looking
        glm::vec3 initialVelocity = cameraFront * asteroidVelocity;

        // Create object with all menu settings
        glm::vec4 objColor = glm::vec4(asteroidColor.r, asteroidColor.g, asteroidColor.b, 1.0f);
        Object newObj(spawnPos, initialVelocity, asteroidMass, 3344, objColor);

        // Use proper radius scaling
        newObj.radius = asteroidRadius;
        newObj.userDefinedRadius = true;
        newObj.UpdateVertices();

        objs.push_back(newObj);

        std::cout << "Asteroid created! Mass: " << asteroidMass << " kg, Radius: " << newObj.radius << " m, Velocity: " << asteroidVelocity << " m/s" << std::endl;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {

    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

    // Let ImGui handle scroll first
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    // Don't scroll camera if menu is open
    if (showMenu) {
        return;
    }

    float cameraSpeed = 1000.0f;
    if (yoffset > 0) {
        cameraPos += cameraFront * cameraSpeed;
    }
    else if (yoffset < 0) {
        cameraPos -= cameraFront * cameraSpeed;
    }
}

void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
    glUseProgram(shaderProgram);
    glm::mat4 model = glm::mat4(1.0f);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, vertexCount / 3);
    glBindVertexArray(0);
}

std::vector<float> CreateGridVertices(float size, int divisions) {
    std::vector<float> vertices;
    float step = size / divisions;
    float halfSize = size / 2.0f;

    // Create grid lines
    for (int i = 0; i <= divisions; ++i) {
        float x = -halfSize + i * step;
        // X lines (parallel to Z)
        vertices.push_back(x); vertices.push_back(0); vertices.push_back(-halfSize);
        vertices.push_back(x); vertices.push_back(0); vertices.push_back(halfSize);

        float z = -halfSize + i * step;
        // Z lines (parallel to X)
        vertices.push_back(-halfSize); vertices.push_back(0); vertices.push_back(z);
        vertices.push_back(halfSize); vertices.push_back(0); vertices.push_back(z);
    }

    return vertices;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Handle window minimization (width or height == 0)
    if (width == 0 || height == 0) {
        return;
    }

    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);

    // Update projection matrix
    GLuint shaderProgram = *static_cast<GLuint*>(glfwGetWindowUserPointer(window));
    glUseProgram(shaderProgram);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void applyGravityForces() {
    for (auto& obj : objs) {
        if (pause || obj.shouldExplode) continue;

        for (auto& obj2 : objs) {
            if (&obj2 == &obj || obj2.shouldExplode) continue;

            float dx = obj2.GetPos()[0] - obj.GetPos()[0];
            float dy = obj2.GetPos()[1] - obj.GetPos()[1];
            float dz = obj2.GetPos()[2] - obj.GetPos()[2];
            float distance = sqrt(dx * dx + dy * dy + dz * dz);

            if (distance > 0.1f) {
                glm::vec3 direction = glm::normalize(obj2.position - obj.position);
                double Gforce = (G * obj.mass * obj2.mass) / (distance * distance);

                float acceleration = Gforce / obj.mass;
                obj.velocity.x += direction.x * acceleration * deltaTime * timeScale;
                obj.velocity.y += direction.y * acceleration * deltaTime * timeScale;
                obj.velocity.z += direction.z * acceleration * deltaTime * timeScale;
            }
        }
    }
}

void checkAllCollisions() {
    for (size_t i = 0; i < objs.size(); ++i) {
        if (objs[i].shouldExplode) continue;

        for (size_t j = i + 1; j < objs.size(); ++j) {
            if (objs[j].shouldExplode) continue;

            objs[i].CheckCollision(objs[j]);
        }
    }
}


int main() {
    GLFWwindow* window = StartGLU();
    if (!window) {
        return -1;
    }

    GLuint shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);
    glfwSetWindowUserPointer(window, &shaderProgram);


    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glUseProgram(shaderProgram);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUseProgram(shaderProgram);

    // Set up callbacks
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Start with normal cursor (menu will be open by default)
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Create Earth
    Object earthObj(originalEarthPosition, originalEarthVelocity, earthMass, 5515, glm::vec4(0.1f, 0.3f, 0.8f, 1.0f), false, true);
    earthObj.radius = 27.0f;
    earthObj.userDefinedRadius = true;
    earthObj.UpdateVertices();
    objs.push_back(earthObj);

    std::cout << "EARTH created! Mass: " << earthMass << " kg, Position: (0, 500, 0), Radius: " << earthObj.radius << " m" << std::endl;

    // Create grid
    std::vector<float> gridVertices = CreateGridVertices(10000.0f, 50);
    CreateVBOVAO(gridVAO, gridVBO, gridVertices.data(), gridVertices.size());

    std::cout << "=== ASTEROID IMPACT SIMULATION ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "M - Toggle menu" << std::endl;
    std::cout << "F11 - Toggle Fullscreen" << std::endl;
    std::cout << "E - Reset Earth to original position" << std::endl;
    std::cout << "Left Click - Create asteroid" << std::endl;
    std::cout << "WASD + Space/Shift - Move camera" << std::endl;
    std::cout << "Mouse - Look around" << std::endl;
    std::cout << "Q - Quit" << std::endl;
    std::cout << "Earth Mass: " << earthMass << " kg" << std::endl;

    while (!glfwWindowShouldClose(window) && running) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start ImGui frame every frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (showMenu || ImGui::GetIO().WantCaptureMouse) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        // Update cursor mode based on current state
        //UpdateCursorMode(window);

        // Process camera movement - check ImGui state
        ImGuiIO& io = ImGui::GetIO();
        if (!showMenu && !io.WantCaptureKeyboard) {
            processCameraMovement();
        }
        UpdateCam(shaderProgram, cameraPos);

        // Draw grid
        glUseProgram(shaderProgram);
        glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.3f);
        glUniform1i(glGetUniformLocation(shaderProgram, "isGrid"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), 0);
        DrawGrid(shaderProgram, gridVAO, gridVertices.size());

        // Handle explosions and remove destroyed objects
        for (auto it = objs.begin(); it != objs.end(); ) {
            if (it->shouldExplode) {
                it->explosionTimer -= deltaTime;
                if (it->explosionTimer <= 0.0f) {
                    std::cout << "OBJECT EXPLODED!" << std::endl;
                    it = objs.erase(it);
                    continue;
                }
            }
            ++it;
        }

        // Apply physics
        if (!pause) {
            applyGravityForces();
            checkAllCollisions();
        }

        // Update and draw objects
        for (auto& obj : objs) {
            // Handle explosion effect
            glm::vec4 drawColor = obj.color;
            if (obj.shouldExplode) {
                float flash = (sin(currentFrame * 20.0f) + 1.0f) * 0.5f;
                drawColor = glm::mix(obj.color, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), flash);
            }

            glUniform4f(objectColorLoc, drawColor.r, drawColor.g, drawColor.b, drawColor.a);

            if (!pause && !obj.shouldExplode) {
                obj.UpdatePos();
            }

            // Draw object (only if not exploded)
            if (!obj.shouldExplode || obj.explosionTimer > 0.0f) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, obj.position);
                model = glm::scale(model, glm::vec3(obj.radius));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(glGetUniformLocation(shaderProgram, "isGrid"), 0);
                glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), obj.glow ? 1 : 0);

                glBindVertexArray(obj.VAO);
                glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount / 3);
            }
        }

        // Draw collision effects
        for (auto it = collisionEffects.begin(); it != collisionEffects.end(); ) {
            it->lifetime -= deltaTime;
            if (it->lifetime > 0) {
                float pulse = (sin(currentFrame * 15.0f) + 1.0f) * 0.3f + 0.7f;
                glm::vec4 effectColor = it->color;
                effectColor.a *= (it->lifetime / 1.0f) * pulse;

                glUniform4f(objectColorLoc, effectColor.r, effectColor.g, effectColor.b, effectColor.a);
                glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), 1);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, it->position);
                model = glm::scale(model, glm::vec3(it->radius * (1.0f + pulse * 0.5f)));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                if (!objs.empty()) {
                    glBindVertexArray(objs[0].VAO);
                    glDrawArrays(GL_TRIANGLES, 0, objs[0].vertexCount / 3);
                }

                ++it;
            }
            else {
                it = collisionEffects.erase(it);
            }
        }

        // Draw impact zones
        for (auto it = impactZones.begin(); it != impactZones.end(); ) {
            it->lifetime -= deltaTime;
            if (it->lifetime > 0) {
                float pulse = (sin(currentFrame * 8.0f) + 1.0f) * 0.2f + 0.8f;
                glm::vec4 zoneColor = it->color;
                zoneColor.a *= (it->lifetime / 5.0f) * 0.6f * pulse;

                glUniform4f(objectColorLoc, zoneColor.r, zoneColor.g, zoneColor.b, zoneColor.a);
                glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), 1);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, it->position);
                model = glm::scale(model, glm::vec3(it->radius * pulse));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

                if (!objs.empty()) {
                    glBindVertexArray(objs[0].VAO);
                    glDrawArrays(GL_TRIANGLES, 0, objs[0].vertexCount / 3);
                }

                ++it;
            }
            else {
                it = impactZones.erase(it);
            }
        }

        glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), 0);

        // Render menu - ImGui frames are already started above
        if (showMenu) {
            RenderMenu();
        }

        // Always render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    for (auto& obj : objs) {
        glDeleteVertexArrays(1, &obj.VAO);
        glDeleteBuffers(1, &obj.VBO);
    }

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}