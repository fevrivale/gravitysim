#include "fuckass.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

using namespace glm;

// Shader sources
const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out float lightIntensity;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vec3 worldPos = (model * vec4(aPos, 1.0)).xyz;
    vec3 normal = normalize(aPos);
    vec3 dirToCenter = normalize(-worldPos);
    lightIntensity = max(dot(normal, dirToCenter), 0.15);})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in float lightIntensity;
out vec4 FragColor;
uniform vec4 objectColor;
uniform bool isGrid;
uniform bool GLOW;
void main() {
    if (isGrid) {
        FragColor = objectColor;
    } else if(GLOW){
        FragColor = vec4(objectColor.rgb * 100000, objectColor.a);
    }else {
        float fade = smoothstep(0.0, 10.0, lightIntensity*10);
        FragColor = vec4(objectColor.rgb * fade, objectColor.a);
    }})glsl";

// Define all global variables
bool running = true;
bool pause = true;
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float lastX = 400.0, lastY = 300.0;
static float camYaw = -90.0f;
static float camPitch = 0.0f;
float deltaTime = 0.0;
float lastFrame = 0.0;

// Menu system variables
bool usingMenu = false;
static bool mKeyPressed = false;
static bool pKeyPressed = false;
float newObjectRadius = 100.0f;
float newObjectVelocity = 500.0f;
float spawnDistance = 1000.0f;
bool newObjectGlow = false;
float newObjectColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

const double G = 6.6743e-11;
const float c = 299792458.0;
float initMass = 1e22f;
float sizeRatio = 30000.0f;

std::vector<Object> objs = {};
GLuint gridVAO, gridVBO;

// Object class implementation
Object::Object(glm::vec3 initPosition, glm::vec3 initVelocity, float mass, float density, glm::vec4 color, bool Glow) {
    this->position = initPosition;
    this->velocity = initVelocity;
    this->mass = mass;
    this->density = density;
    this->radius = pow(((3 * mass / density) / (4 * 3.14159265359f)), (1.0f / 3.0f)) / sizeRatio;
    this->color = color;
    this->glow = Glow;
    this->Initalizing = false;
    this->Launched = false;
    this->target = false;
    this->LastPos = position;

    std::vector<float> vertices = Draw();
    vertexCount = vertices.size();
    CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
}

std::vector<float> Object::Draw() {
    std::vector<float> vertices;
    int stacks = 10;
    int sectors = 10;

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

void Object::UpdatePos() {
    // Use proper time integration with deltaTime
    float timeStep = deltaTime * 60.0f; // Scale to match your simulation speed
    this->position[0] += this->velocity[0] * timeStep;
    this->position[1] += this->velocity[1] * timeStep;
    this->position[2] += this->velocity[2] * timeStep;

    // Update radius based on mass and density
    this->radius = pow(((3 * this->mass / this->density) / (4 * 3.14159265359f)), (1.0f / 3.0f)) / sizeRatio;
}

void Object::UpdateVertices() {
    std::vector<float> vertices = Draw();
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
}

glm::vec3 Object::GetPos() const {
    return this->position;
}

void Object::accelerate(float x, float y, float z) {
    // Use proper time integration
    float timeStep = deltaTime * 60.0f;
    this->velocity[0] += x * timeStep;
    this->velocity[1] += y * timeStep;
    this->velocity[2] += z * timeStep;
}

float Object::CheckCollision(const Object& other) {
    float dx = other.position[0] - this->position[0];
    float dy = other.position[1] - this->position[1];
    float dz = other.position[2] - this->position[2];
    float distance = sqrt(dx * dx + dy * dy + dz * dz);

    float minDistance = other.radius + this->radius;

    if (distance < minDistance) {
        // Handle collision - merge objects or bounce
        if (this->mass > other.mass * 2.0f) {
            // This object absorbs the smaller one
            return -1.0f; // Signal to remove the other object
        }
        else if (other.mass > this->mass * 2.0f) {
            // Other object absorbs this one
            return -2.0f; // Signal to remove this object
        }
        else {
            // Similar mass - bounce
            return 0.0f; // Stop movement
        }
    }
    return 1.0f; // No collision
}

// Grid functions (simplified implementations)
std::vector<float> CreateGridVertices(float size, int divisions, const std::vector<Object>& objs) {
    std::vector<float> vertices;
    float step = size / divisions;

    // Create a simple grid in XZ plane
    for (int i = -divisions; i <= divisions; ++i) {
        // Lines along Z axis
        vertices.insert(vertices.end(), { i * step, 0.0f, -size });
        vertices.insert(vertices.end(), { i * step, 0.0f, size });
        // Lines along X axis
        vertices.insert(vertices.end(), { -size, 0.0f, i * step });
        vertices.insert(vertices.end(), { size, 0.0f, i * step });
    }

    return vertices;
}

std::vector<float> UpdateGridVertices(std::vector<float>& gridVertices, const std::vector<Object>& objs) {
    // For now, return the same vertices
    // You could update this to make the grid follow objects
    return gridVertices;
}

void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, vertexCount / 3);
    glBindVertexArray(0);
}

int main() {
    GLFWwindow* window = StartGLU();
    GLuint shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    // ImGui initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = 1.0f;
    style.WindowRounding = 0.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend!" << std::endl;
        return -1;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend!" << std::endl;
        return -1;
    }

    io.DisplaySize = ImVec2(800, 600);
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUseProgram(shaderProgram);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 750000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    cameraPos = glm::vec3(0.0f, 10000.0f, 30000.0f);

    // Create stable initial solar system
    objs = {
        // Central star
        Object(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 1.989e30f, 1408, glm::vec4(1.0f, 0.9f, 0.1f, 1.0f), true),

        // Planet 1 - close orbit
        Object(glm::vec3(5000, 0, 0), glm::vec3(0, 0, 800), 5.972e24f, 5515, glm::vec4(0.0f, 0.4f, 1.0f, 1.0f)),

        // Planet 2 - farther orbit  
        Object(glm::vec3(0, 0, 8000), glm::vec3(600, 0, 0), 8.681e25f, 1270, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)),
    };

    std::vector<float> gridVertices = CreateGridVertices(20000.0f, 25, objs);
    CreateVBOVAO(gridVAO, gridVBO, gridVertices.data(), gridVertices.size());

    while (!glfwWindowShouldClose(window) && running == true) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        // Update ImGui
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800, 600);

        // Update mouse position
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        io.AddMousePosEvent((float)mouseX, (float)mouseY);
        io.AddMouseButtonEvent(0, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        io.AddMouseButtonEvent(1, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

        // Handle M key toggle for menu
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
            if (!mKeyPressed) {
                usingMenu = !usingMenu;
                if (usingMenu) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    std::cout << "Menu mode activated" << std::endl;
                }
                else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    std::cout << "Camera mode activated" << std::endl;
                }
                mKeyPressed = true;
            }
        }
        else {
            mKeyPressed = false;
        }

        // Handle P key toggle for pause
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pKeyPressed) {
                pause = !pause;
                std::cout << "Simulation " << (pause ? "Paused" : "Resumed") << std::endl;
                pKeyPressed = true;
            }
        }
        else {
            pKeyPressed = false;
        }

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // DISABLE DEPTH TEST for ImGui
        glDisable(GL_DEPTH_TEST);

        // Force ALL ImGui windows to be fully opaque
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        // 1. Debug Info - ALWAYS VISIBLE
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Objects: %zu", objs.size());
        ImGui::Text("Mode: %s", usingMenu ? "MENU" : "CAMERA");
        ImGui::Text("Pause: %s", pause ? "ON" : "OFF");
        ImGui::Text("Controls: M=Menu, P=Pause, Q=Quit");

        if (ImGui::Button("Print Object Info", ImVec2(150, 0))) {
            std::cout << "=== OBJECT INFO ===" << std::endl;
            for (int i = 0; i < objs.size(); i++) {
                std::cout << "Object " << i << ": Pos(" << objs[i].position.x << ","
                    << objs[i].position.y << "," << objs[i].position.z
                    << ") Vel(" << objs[i].velocity.x << ","
                    << objs[i].velocity.y << "," << objs[i].velocity.z
                    << ") Mass: " << objs[i].mass << std::endl;
            }
            std::cout << "===================" << std::endl;
        }
        ImGui::End();

        // 2. Object Creator - ONLY VISIBLE WHEN usingMenu IS TRUE
        if (usingMenu) {
            ImGui::Begin("Object Creator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Object Creator - ACTIVE");
            ImGui::Separator();

            // Menu controls
            ImGui::SliderFloat("Radius", &newObjectRadius, 10.0f, 1000.0f, "%.1f");
            ImGui::SliderFloat("Velocity", &newObjectVelocity, 0.0f, 5000.0f, "%.1f");
            ImGui::SliderFloat("Spawn Distance", &spawnDistance, 100.0f, 5000.0f, "%.1f");
            ImGui::Checkbox("Glow Effect", &newObjectGlow);
            ImGui::ColorEdit4("Color", newObjectColor);

            // Create button
            ImGui::Spacing();
            if (ImGui::Button("Create Object", ImVec2(-1, 0))) {
                float density = 5515.0f; // Earth-like density
                float volume = (4.0f / 3.0f) * 3.14159265359f * newObjectRadius * newObjectRadius * newObjectRadius;
                float mass = volume * density;

                // Spawn in front of camera with some initial velocity
                glm::vec3 spawnPosition = cameraPos + cameraFront * spawnDistance;

                // Give it some initial tangential velocity for orbital motion
                glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
                glm::vec3 initialVelocity = right * newObjectVelocity;

                objs.emplace_back(spawnPosition,
                    initialVelocity,
                    mass,
                    density,
                    glm::vec4(newObjectColor[0], newObjectColor[1], newObjectColor[2], newObjectColor[3]),
                    newObjectGlow);

                std::cout << "Object created! Mass: " << mass << " kg, Radius: " << newObjectRadius << std::endl;
            }

            // Preset buttons
            ImGui::Separator();
            ImGui::Text("Presets:");

            if (ImGui::Button("Small Moon", ImVec2(100, 0))) {
                newObjectRadius = 200.0f;
                newObjectVelocity = 800.0f;
                newObjectColor[0] = 0.8f; newObjectColor[1] = 0.8f; newObjectColor[2] = 1.0f; newObjectColor[3] = 1.0f;
                newObjectGlow = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Large Planet", ImVec2(100, 0))) {
                newObjectRadius = 500.0f;
                newObjectVelocity = 400.0f;
                newObjectColor[0] = 1.0f; newObjectColor[1] = 0.5f; newObjectColor[2] = 0.0f; newObjectColor[3] = 1.0f;
                newObjectGlow = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Bright Star", ImVec2(100, 0))) {
                newObjectRadius = 300.0f;
                newObjectVelocity = 0.0f;
                newObjectColor[0] = 1.0f; newObjectColor[1] = 1.0f; newObjectColor[2] = 0.0f; newObjectColor[3] = 1.0f;
                newObjectGlow = true;
            }

            // Solar System Controls
            ImGui::Separator();
            ImGui::Text("Solar System Controls:");

            if (ImGui::Button("Delete Central Star", ImVec2(150, 0))) {
                for (auto it = objs.begin(); it != objs.end(); ) {
                    if (it->glow) {
                        std::cout << "Central star deleted!" << std::endl;
                        it = objs.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete ALL Objects", ImVec2(150, 0))) {
                objs.clear();
                std::cout << "All objects deleted!" << std::endl;
            }

            ImGui::SameLine();
            if (ImGui::Button("Create Earth", ImVec2(150, 0))) {
                // Earth parameters
                float earthMass = 5.972e24f;
                float earthRadius = 6371.0f / 100.0f;
                glm::vec4 earthColor = glm::vec4(0.0f, 0.4f, 1.0f, 1.0f);

                // Find sun position
                glm::vec3 sunPosition = glm::vec3(0, 0, 0);
                for (const auto& obj : objs) {
                    if (obj.glow) {
                        sunPosition = obj.position;
                        break;
                    }
                }

                // Create Earth in orbit
                float earthOrbitDistance = 10000.0f;
                glm::vec3 earthPosition = sunPosition + glm::vec3(earthOrbitDistance, 0, 0);
                glm::vec3 earthVelocity = glm::vec3(0, 0, 500.0f);

                objs.emplace_back(earthPosition, earthVelocity, earthMass, 5515, earthColor, false);
                std::cout << "Earth created in orbit around the sun!" << std::endl;
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset Solar System", ImVec2(150, 0))) {
                objs.clear();
                objs = {
                    Object(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 1.989e30f, 1408, glm::vec4(1.0f, 0.9f, 0.1f, 1.0f), true),
                    Object(glm::vec3(5000, 0, 0), glm::vec3(0, 0, 800), 5.972e24f, 5515, glm::vec4(0.0f, 0.4f, 1.0f, 1.0f)),
                    Object(glm::vec3(0, 0, 8000), glm::vec3(600, 0, 0), 8.681e25f, 1270, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)),
                };
                std::cout << "Solar system reset!" << std::endl;
            }

            // All Objects List
            ImGui::Separator();
            ImGui::Text("All Objects (%zu total):", objs.size());
            if (ImGui::BeginChild("ObjectsList", ImVec2(0, 200), true)) {
                for (int i = 0; i < objs.size(); i++) {
                    ImGui::PushID(i);
                    ImGui::Text("%d: Pos(%.0f,%.0f,%.0f) Rad:%.0f %s",
                        i, objs[i].position.x, objs[i].position.y, objs[i].position.z,
                        objs[i].radius, objs[i].glow ? "GLOWING" : "");

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) {
                        objs.erase(objs.begin() + i);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::End();
        }

        ImGui::PopStyleColor(2);

        // RENDER ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // RE-ENABLE DEPTH TEST for 3D rendering
        glEnable(GL_DEPTH_TEST);

        // Set callbacks
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);

        UpdateCam(shaderProgram, cameraPos);

        // Handle object initialization (right-click mass increase)
        if (!objs.empty() && objs.back().Initalizing) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                objs.back().mass *= 1.0 + 1.0 * deltaTime;
                objs.back().radius = pow((3 * objs.back().mass / objs.back().density) / (4 * 3.14159265359f), 1.0f / 3.0f) / sizeRatio;
                objs.back().UpdateVertices();
            }
        }

        // Update physics (separate from rendering)
        if (!pause) {
            // First, calculate all forces
            std::vector<glm::vec3> accelerations(objs.size(), glm::vec3(0.0f));

            for (size_t i = 0; i < objs.size(); i++) {
                for (size_t j = i + 1; j < objs.size(); j++) {
                    if (!objs[i].Initalizing && !objs[j].Initalizing) {
                        glm::vec3 delta = objs[j].position - objs[i].position;
                        float distance = glm::length(delta);

                        if (distance > 0.1f) { // Avoid division by zero
                            // Calculate gravitational force (scaled for stability)
                            float force = (G * objs[i].mass * objs[j].mass) / (distance * distance);
                            // Scale force to make it workable in our coordinate system
                            force *= 1e-15f;

                            glm::vec3 forceDir = glm::normalize(delta);
                            glm::vec3 acceleration_i = forceDir * (force / objs[i].mass);
                            glm::vec3 acceleration_j = -forceDir * (force / objs[j].mass);

                            accelerations[i] += acceleration_i;
                            accelerations[j] += acceleration_j;
                        }
                    }
                }
            }

            // Then, update velocities and positions
            for (size_t i = 0; i < objs.size(); i++) {
                if (!objs[i].Initalizing) {
                    // Update velocity based on acceleration
                    objs[i].velocity += accelerations[i] * deltaTime * 60.0f;

                    // Update position
                    objs[i].UpdatePos();
                }
            }

            // Handle collisions after position updates
            for (size_t i = 0; i < objs.size(); i++) {
                for (size_t j = i + 1; j < objs.size(); j++) {
                    if (!objs[i].Initalizing && !objs[j].Initalizing) {
                        float collisionResult = objs[i].CheckCollision(objs[j]);

                        if (collisionResult < 0) {
                            // Handle object removal
                            if (collisionResult == -1.0f) {
                                // obj[i] absorbs obj[j]
                                objs[i].mass += objs[j].mass;
                                objs.erase(objs.begin() + j);
                                j--; // Adjust index after removal
                                std::cout << "Collision: Object " << i << " absorbed object " << j << std::endl;
                            }
                            else if (collisionResult == -2.0f) {
                                // obj[j] absorbs obj[i]
                                objs[j].mass += objs[i].mass;
                                objs.erase(objs.begin() + i);
                                i--; // Adjust index after removal
                                std::cout << "Collision: Object " << j << " absorbed object " << i << std::endl;
                                break; // Exit inner loop since i changed
                            }
                        }
                        else if (collisionResult == 0.0f) {
                            // Bounce - simple velocity reversal
                            std::swap(objs[i].velocity, objs[j].velocity);
                        }
                    }
                }
            }
        }

        // Draw the grid
        glUseProgram(shaderProgram);
        glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.25f);
        glUniform1i(glGetUniformLocation(shaderProgram, "isGrid"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), 0);
        gridVertices = UpdateGridVertices(gridVertices, objs);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
        DrawGrid(shaderProgram, gridVAO, gridVertices.size());

        // Draw objects
        for (auto& obj : objs) {
            glUniform4f(objectColorLoc, obj.color.r, obj.color.g, obj.color.b, obj.color.a);

            if (obj.Initalizing) {
                obj.radius = pow(((3 * obj.mass / obj.density) / (4 * 3.14159265359)), (1.0f / 3.0f)) / 1000000;
                obj.UpdateVertices();
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.position);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(glGetUniformLocation(shaderProgram, "isGrid"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "GLOW"), obj.glow ? 1 : 0);

            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount / 3);
        }

        glfwSwapBuffers(window);
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

GLFWwindow* StartGLU() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW, panic" << std::endl;
        return nullptr;
    }
    GLFWwindow* window = glfwCreateWindow(800, 600, "3D Solar System", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 800, 600);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return window;
}

GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Vertex shader
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

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Shader program
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

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    glUseProgram(shaderProgram);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Don't process camera movement if using menu
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;
    if (usingMenu) return;

    float cameraSpeed = 10000.0f * deltaTime;
    bool shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraFront;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraFront;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPos += cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraUp;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraUp;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        glfwTerminate();
        glfwWindowShouldClose(window);
        running = false;
    }

    // Arrow keys for object positioning during initialization
    if (!objs.empty() && objs[objs.size() - 1].Initalizing) {
        if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            if (!shiftPressed) {
                objs[objs.size() - 1].position[1] += objs[objs.size() - 1].radius * 0.2;
            }
        };
        if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            if (!shiftPressed) {
                objs[objs.size() - 1].position[1] -= objs[objs.size() - 1].radius * 0.2;
            }
        }
        if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size() - 1].position[0] += objs[objs.size() - 1].radius * 0.2;
        };
        if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            objs[objs.size() - 1].position[0] -= objs[objs.size() - 1].radius * 0.2;
        };
    };
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    // Don't process mouse look if using menu
    if (usingMenu) {
        lastX = xpos;
        lastY = ypos;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    camYaw += xoffset;
    camPitch += yoffset;

    if (camPitch > 89.0f) camPitch = 89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
    front.y = sin(glm::radians(camPitch));
    front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
    cameraFront = glm::normalize(front);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // Don't process mouse buttons if using menu
    if (usingMenu) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            objs.emplace_back(glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0f, 0.0f, 0.0f), initMass);
            objs[objs.size() - 1].Initalizing = true;
        };
        if (action == GLFW_RELEASE) {
            objs[objs.size() - 1].Initalizing = false;
            objs[objs.size() - 1].Launched = true;
        };
    };
    if (!objs.empty() && button == GLFW_MOUSE_BUTTON_RIGHT && objs[objs.size() - 1].Initalizing) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            objs[objs.size() - 1].mass *= 1.2;
        }
        std::cout << "MASS: " << objs[objs.size() - 1].mass << std::endl;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Don't process scroll if using menu
    if (usingMenu) return;

    float cameraSpeed = 250000.0f * deltaTime;
    if (yoffset > 0) {
        cameraPos += cameraSpeed * cameraFront;
    }
    else if (yoffset < 0) {
        cameraPos -= cameraSpeed * cameraFront;
    }
}

glm::vec3 sphericalToCartesian(float r, float theta, float phi) {
    float x = r * sin(theta) * cos(phi);
    float y = r * cos(theta);
    float z = r * sin(theta) * sin(phi);
    return glm::vec3(x, y, z);
}
std::mutex objsMutex;
