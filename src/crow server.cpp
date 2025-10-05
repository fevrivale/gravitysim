//#include <GL/glew.h>
//#include <GLFW/glfw3.h>
//#include "header.h"
//#include <crow.h>
//#include <thread>
//#include <iostream>
//#include <sstream>
//
//void StartCrowServer() {
//    crow::SimpleApp app;
//
//    // Add new object endpoint
//    CROW_ROUTE(app, "/add_object").methods("POST"_method)
//        ([](const crow::request& req) {
//        try {
//            auto json = crow::json::load(req.body);
//            if (!json) {
//                return crow::response(400, "Invalid JSON");
//            }
//
//            // Extract parameters from JSON
//            float px = static_cast<float>(json["px"].d());
//            float py = static_cast<float>(json["py"].d());
//            float pz = static_cast<float>(json["pz"].d());
//            float vx = static_cast<float>(json["vx"].d());
//            float vy = static_cast<float>(json["vy"].d());
//            float vz = static_cast<float>(json["vz"].d());
//            float mass = static_cast<float>(json["mass"].d());
//
//            // Optional parameters with defaults
//            float density = 5515.0f;
//            glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);
//            bool glow = false;
//
//            if (json.has("density")) {
//                density = static_cast<float>(json["density"].d());
//            }
//            if (json.has("color")) {
//                auto color_json = json["color"];
//                color.r = static_cast<float>(color_json[0].d());
//                color.g = static_cast<float>(color_json[1].d());
//                color.b = static_cast<float>(color_json[2].d());
//                if (color_json.size() > 3) {
//                    color.a = static_cast<float>(color_json[3].d());
//                }
//            }
//            if (json.has("glow")) {
//                glow = json["glow"].b();
//            }
//
//            // Calculate radius based on mass and density
//            float radius = pow(((3 * mass / density) / (4 * 3.14159265359f)), (1.0f / 3.0f)) / sizeRatio;
//
//            // Create new object
//            glm::vec3 position = glm::vec3(px, py, pz);
//            glm::vec3 velocity = glm::vec3(vx, vy, vz);
//
//            {
//                std::lock_guard<std::mutex> lock(objsMutex);
//                objs.emplace_back(position, velocity, mass, density, color, glow);
//            }
//
//            std::cout << "Added new object via Crow: mass=" << mass
//                << ", position=(" << px << "," << py << "," << pz << ")"
//                << ", velocity=(" << vx << "," << vy << "," << vz << ")"
//                << ", radius=" << radius << std::endl;
//
//            crow::json::wvalue response;
//            response["status"] = "success";
//            response["message"] = "Object added successfully";
//            response["radius"] = radius;
//            return crow::response(200, response);
//        }
//        catch (const std::exception& e) {
//            crow::json::wvalue response;
//            response["status"] = "error";
//            response["message"] = std::string("Error: ") + e.what();
//            return crow::response(500, response);
//        }
//            });
//
//    // Get objects info endpoint - FIXED: Always return crow::response
//    CROW_ROUTE(app, "/objects")
//        ([]() {
//        crow::json::wvalue response;
//        std::lock_guard<std::mutex> lock(objsMutex);
//
//        crow::json::wvalue objects_array;
//        int i = 0;
//        for (const auto& obj : objs) {
//            crow::json::wvalue obj_json;
//            auto pos = obj.GetPos();
//
//            // Create position array
//            crow::json::wvalue pos_array;
//            pos_array[0] = pos.x;
//            pos_array[1] = pos.y;
//            pos_array[2] = pos.z;
//            obj_json["position"] = std::move(pos_array);
//
//            // Create velocity array
//            crow::json::wvalue vel_array;
//            vel_array[0] = obj.velocity.x;
//            vel_array[1] = obj.velocity.y;
//            vel_array[2] = obj.velocity.z;
//            obj_json["velocity"] = std::move(vel_array);
//
//            // Create color array
//            crow::json::wvalue color_array;
//            color_array[0] = obj.color.r;
//            color_array[1] = obj.color.g;
//            color_array[2] = obj.color.b;
//            color_array[3] = obj.color.a;
//            obj_json["color"] = std::move(color_array);
//
//            obj_json["mass"] = obj.mass;
//            obj_json["density"] = obj.density;
//            obj_json["radius"] = obj.radius;
//            obj_json["glow"] = obj.glow;
//            obj_json["initializing"] = obj.Initalizing;
//            obj_json["launched"] = obj.Launched;
//
//            objects_array[i++] = std::move(obj_json);
//        }
//        response["objects"] = std::move(objects_array);
//        response["count"] = i;
//        response["paused"] = pause;
//        response["using_menu"] = usingMenu;
//
//        return crow::response(200, response); // FIXED: Wrap in crow::response
//            });
//
//    // Get detailed object info by index - FIXED: Always return crow::response
//    CROW_ROUTE(app, "/objects/<int>")
//        ([](int index) {
//        std::lock_guard<std::mutex> lock(objsMutex);
//
//        if (index < 0 || index >= objs.size()) {
//            crow::json::wvalue response;
//            response["status"] = "error";
//            response["message"] = "Object not found";
//            return crow::response(404, response);
//        }
//
//        const auto& obj = objs[index];
//        auto pos = obj.GetPos();
//
//        crow::json::wvalue response;
//
//        // Create position array
//        crow::json::wvalue pos_array;
//        pos_array[0] = pos.x;
//        pos_array[1] = pos.y;
//        pos_array[2] = pos.z;
//        response["position"] = std::move(pos_array);
//
//        // Create velocity array
//        crow::json::wvalue vel_array;
//        vel_array[0] = obj.velocity.x;
//        vel_array[1] = obj.velocity.y;
//        vel_array[2] = obj.velocity.z;
//        response["velocity"] = std::move(vel_array);
//
//        // Create color array
//        crow::json::wvalue color_array;
//        color_array[0] = obj.color.r;
//        color_array[1] = obj.color.g;
//        color_array[2] = obj.color.b;
//        color_array[3] = obj.color.a;
//        response["color"] = std::move(color_array);
//
//        response["mass"] = obj.mass;
//        response["density"] = obj.density;
//        response["radius"] = obj.radius;
//        response["glow"] = obj.glow;
//        response["initializing"] = obj.Initalizing;
//        response["launched"] = obj.Launched;
//
//        return crow::response(200, response); // FIXED: Wrap in crow::response
//            });
//
//    // Remove object endpoint
//    CROW_ROUTE(app, "/remove_object/<int>").methods("DELETE"_method)
//        ([](int index) {
//        std::lock_guard<std::mutex> lock(objsMutex);
//
//        if (index < 0 || index >= objs.size()) {
//            crow::json::wvalue response;
//            response["status"] = "error";
//            response["message"] = "Invalid object index";
//            return crow::response(400, response);
//        }
//
//        std::string name = "Object " + std::to_string(index);
//        if (objs[index].glow) {
//            name = "Central Star";
//        }
//
//        objs.erase(objs.begin() + index);
//
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Removed " + name;
//        return crow::response(200, response);
//            });
//
//    // Clear all objects endpoint
//    CROW_ROUTE(app, "/clear_objects").methods("DELETE"_method)
//        ([]() {
//        std::lock_guard<std::mutex> lock(objsMutex);
//        int count = objs.size();
//        objs.clear();
//
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Cleared " + std::to_string(count) + " objects";
//        return crow::response(200, response);
//            });
//
//    // Simulation control endpoints
//    CROW_ROUTE(app, "/pause").methods("POST"_method)
//        ([]() {
//        pause = true;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Simulation paused";
//        return crow::response(200, response);
//            });
//
//    CROW_ROUTE(app, "/resume").methods("POST"_method)
//        ([]() {
//        pause = false;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Simulation resumed";
//        return crow::response(200, response);
//            });
//
//    CROW_ROUTE(app, "/toggle_pause").methods("POST"_method)
//        ([]() {
//        pause = !pause;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = pause ? "Simulation paused" : "Simulation resumed";
//        response["paused"] = pause;
//        return crow::response(200, response);
//            });
//
//    CROW_ROUTE(app, "/reset").methods("POST"_method)
//        ([]() {
//        std::lock_guard<std::mutex> lock(objsMutex);
//        objs.clear();
//
//        // Create default solar system
//        objs = {
//            Object(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 1.989e30f, 1408, glm::vec4(1.0f, 0.9f, 0.1f, 1.0f), true),
//            Object(glm::vec3(5000, 0, 0), glm::vec3(0, 0, 800), 5.972e24f, 5515, glm::vec4(0.0f, 0.4f, 1.0f, 1.0f)),
//            Object(glm::vec3(0, 0, 8000), glm::vec3(600, 0, 0), 8.681e25f, 1270, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)),
//        };
//
//        pause = true;
//
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Simulation reset to default solar system";
//        return crow::response(200, response);
//            });
//
//    // Camera control endpoint
//    CROW_ROUTE(app, "/camera").methods("POST"_method)
//        ([](const crow::request& req) {
//        try {
//            auto json = crow::json::load(req.body);
//            if (!json) {
//                crow::json::wvalue response;
//                response["status"] = "error";
//                response["message"] = "Invalid JSON";
//                return crow::response(400, response);
//            }
//
//            if (json.has("position")) {
//                auto pos = json["position"];
//                cameraPos.x = static_cast<float>(pos[0].d());
//                cameraPos.y = static_cast<float>(pos[1].d());
//                cameraPos.z = static_cast<float>(pos[2].d());
//            }
//
//            if (json.has("target")) {
//                auto target = json["target"];
//                glm::vec3 targetPos = glm::vec3(
//                    static_cast<float>(target[0].d()),
//                    static_cast<float>(target[1].d()),
//                    static_cast<float>(target[2].d())
//                );
//                cameraFront = glm::normalize(targetPos - cameraPos);
//
//                // Update camera orientation
//                camYaw = glm::degrees(atan2(cameraFront.z, cameraFront.x));
//                camPitch = glm::degrees(asin(cameraFront.y));
//            }
//
//            if (json.has("look_at")) {
//                auto target = json["look_at"];
//                glm::vec3 targetPos = glm::vec3(
//                    static_cast<float>(target[0].d()),
//                    static_cast<float>(target[1].d()),
//                    static_cast<float>(target[2].d())
//                );
//                cameraFront = glm::normalize(targetPos - cameraPos);
//            }
//
//            crow::json::wvalue response;
//            response["status"] = "success";
//            response["message"] = "Camera updated";
//            return crow::response(200, response);
//        }
//        catch (const std::exception& e) {
//            crow::json::wvalue response;
//            response["status"] = "error";
//            response["message"] = std::string("Error: ") + e.what();
//            return crow::response(500, response);
//        }
//            });
//
//    // Get camera info - FIXED: Always return crow::response
//    CROW_ROUTE(app, "/camera")
//        ([]() {
//        crow::json::wvalue response;
//
//        crow::json::wvalue pos_array;
//        pos_array[0] = cameraPos.x;
//        pos_array[1] = cameraPos.y;
//        pos_array[2] = cameraPos.z;
//        response["position"] = std::move(pos_array);
//
//        crow::json::wvalue front_array;
//        front_array[0] = cameraFront.x;
//        front_array[1] = cameraFront.y;
//        front_array[2] = cameraFront.z;
//        response["front"] = std::move(front_array);
//
//        response["yaw"] = camYaw;
//        response["pitch"] = camPitch;
//
//        return crow::response(200, response); // FIXED: Wrap in crow::response
//            });
//
//    // Menu control endpoints
//    CROW_ROUTE(app, "/menu/enable").methods("POST"_method)
//        ([]() {
//        usingMenu = true;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Menu enabled";
//        return crow::response(200, response);
//            });
//
//    CROW_ROUTE(app, "/menu/disable").methods("POST"_method)
//        ([]() {
//        usingMenu = false;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = "Menu disabled";
//        return crow::response(200, response);
//            });
//
//    CROW_ROUTE(app, "/menu/toggle").methods("POST"_method)
//        ([]() {
//        usingMenu = !usingMenu;
//        crow::json::wvalue response;
//        response["status"] = "success";
//        response["message"] = usingMenu ? "Menu enabled" : "Menu disabled";
//        response["menu_enabled"] = usingMenu;
//        return crow::response(200, response);
//            });
//
//    // Health check endpoint with simulation status - FIXED: Always return crow::response
//    CROW_ROUTE(app, "/health")
//        ([]() {
//        crow::json::wvalue response;
//        response["status"] = "healthy";
//        response["objects_count"] = objs.size();
//        response["paused"] = pause;
//        response["menu_mode"] = usingMenu;
//        response["fps"] = (deltaTime > 0) ? (1.0f / deltaTime) : 0.0f;
//
//        return crow::response(200, response); // FIXED: Wrap in crow::response
//            });
//
//    // Simulation statistics - FIXED: Always return crow::response
//    CROW_ROUTE(app, "/stats")
//        ([]() {
//        crow::json::wvalue response;
//
//        std::lock_guard<std::mutex> lock(objsMutex);
//
//        float total_mass = 0.0f;
//        int glowing_objects = 0;
//        float max_velocity = 0.0f;
//
//        for (const auto& obj : objs) {
//            total_mass += obj.mass;
//            if (obj.glow) glowing_objects++;
//            float velocity = glm::length(obj.velocity);
//            if (velocity > max_velocity) max_velocity = velocity;
//        }
//
//        response["total_objects"] = objs.size();
//        response["total_mass"] = total_mass;
//        response["glowing_objects"] = glowing_objects;
//        response["max_velocity"] = max_velocity;
//        response["paused"] = pause;
//        response["delta_time"] = deltaTime;
//
//        return crow::response(200, response); // FIXED: Wrap in crow::response
//            });
//
//    // Root endpoint
//    CROW_ROUTE(app, "/")
//        ([]() {
//        crow::json::wvalue response;
//        response["message"] = "Solar System Simulation Server is running!";
//        response["endpoints"] = "Use /health for status check, /objects to see objects";
//        return crow::response(200, response);
//            });
//
//    std::cout << "Starting Crow server on http://localhost:18080" << std::endl;
//    std::cout << "Available endpoints:" << std::endl;
//    std::cout << "  POST   /add_object              - Add new celestial object" << std::endl;
//    std::cout << "  GET    /objects                 - Get all objects info" << std::endl;
//    std::cout << "  GET    /objects/<index>         - Get specific object info" << std::endl;
//    std::cout << "  DELETE /remove_object/<index>   - Remove object by index" << std::endl;
//    std::cout << "  DELETE /clear_objects           - Remove all objects" << std::endl;
//    std::cout << "  POST   /pause                   - Pause simulation" << std::endl;
//    std::cout << "  POST   /resume                  - Resume simulation" << std::endl;
//    std::cout << "  POST   /toggle_pause            - Toggle pause state" << std::endl;
//    std::cout << "  POST   /reset                   - Reset to default solar system" << std::endl;
//    std::cout << "  GET/POST /camera                - Get/Set camera position" << std::endl;
//    std::cout << "  POST   /menu/[enable|disable|toggle] - Control menu mode" << std::endl;
//    std::cout << "  GET    /health                  - Health check with status" << std::endl;
//    std::cout << "  GET    /stats                   - Simulation statistics" << std::endl;
//    std::cout << std::endl;
//
//    app.port(18080).multithreaded().run();

//}
