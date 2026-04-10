#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cmath>
#include <dirent.h>
#include <algorithm>
#include "crow.h"

struct Vec3 {
    float x, y, z;
};

struct Triangle {
    Vec3 a, b, c;
};

// Hardcoded cube mesh: 12 triangles (2 per face, CCW winding from outside)
static const std::vector<Triangle> CUBE_TRIANGLES = {
    // Front face (z = 1)
    {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}},
    {{-1, -1, 1}, {1, 1, 1}, {-1, 1, 1}},

    // Back face (z = -1)
    {{1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}},
    {{1, -1, -1}, {-1, 1, -1}, {1, 1, -1}},

    // Right face (x = 1)
    {{1, -1, -1}, {1, -1, 1}, {1, 1, 1}},
    {{1, -1, -1}, {1, 1, 1}, {1, 1, -1}},

    // Left face (x = -1)
    {{-1, -1, 1}, {-1, -1, -1}, {-1, 1, -1}},
    {{-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1}},

    // Top face (y = 1)
    {{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}},
    {{-1, 1, -1}, {1, 1, 1}, {1, 1, -1}},

    // Bottom face (y = -1)
    {{1, -1, -1}, {1, -1, 1}, {-1, -1, 1}},
    {{1, -1, -1}, {-1, -1, 1}, {-1, -1, -1}},
};

std::vector<std::string> listModels() {
    std::vector<std::string> models;
    DIR* dir = opendir("models");
    if (!dir) {
        return models;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename(entry->d_name);
        if (filename.length() > 4) {
            std::string ext = filename.substr(filename.length() - 4);
            if (ext == ".obj" || ext == ".OBJ") {
                models.push_back(filename);
            }
        }
    }

    closedir(dir);
    std::sort(models.begin(), models.end());
    return models;
}

std::vector<Triangle> parseOBJ(const std::string& filepath) {
    std::vector<Triangle> triangles;
    std::vector<Vec3> verts;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        return triangles;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            verts.push_back({x, y, z});
        } else if (token == "f") {
            std::vector<int> indices;
            std::string vertexStr;
            while (iss >> vertexStr) {
                // Parse vertex reference (format: v, v/vt, v/vt/vn, or v//vn)
                int vertexIndex = 0;
                size_t slashPos = vertexStr.find('/');
                std::string indexStr = (slashPos != std::string::npos)
                    ? vertexStr.substr(0, slashPos)
                    : vertexStr;

                vertexIndex = std::stoi(indexStr);

                // Handle negative indices (relative to end)
                if (vertexIndex < 0) {
                    vertexIndex = verts.size() + vertexIndex + 1;
                }

                // Convert to 0-based
                vertexIndex--;

                if (vertexIndex >= 0 && vertexIndex < (int)verts.size()) {
                    indices.push_back(vertexIndex);
                }
            }

            // Fan triangulation for faces with 4+ vertices
            if (indices.size() >= 3) {
                for (size_t i = 1; i < indices.size() - 1; ++i) {
                    Triangle tri;
                    tri.a = verts[indices[0]];
                    tri.b = verts[indices[i]];
                    tri.c = verts[indices[i + 1]];
                    triangles.push_back(tri);
                }
            }
        }
    }

    file.close();
    return triangles;
}

std::string getTrianglesJSON(const std::string& model = "") {
    // Determine which triangles to use
    std::vector<Triangle> triangles;

    if (model.empty()) {
        triangles = CUBE_TRIANGLES;
    } else {
        // Security: reject model names with path components
        if (model.find('/') != std::string::npos ||
            model.find('\\') != std::string::npos ||
            model.find("..") != std::string::npos) {
            return "{\"error\":\"Invalid model name\"}";
        }

        // Parse OBJ file
        triangles = parseOBJ("models/" + model);
        if (triangles.empty()) {
            return "{\"error\":\"Failed to parse model or model not found\"}";
        }
    }

    // Serialize to JSON
    std::stringstream ss;
    ss << "{\"triangle_count\":" << triangles.size() << ",\"triangles\":[";

    for (size_t i = 0; i < triangles.size(); ++i) {
        const auto& tri = triangles[i];
        if (i > 0) ss << ",";
        ss << "{\"a\":{\"x\":" << tri.a.x << ",\"y\":" << tri.a.y << ",\"z\":" << tri.a.z << "},"
           << "\"b\":{\"x\":" << tri.b.x << ",\"y\":" << tri.b.y << ",\"z\":" << tri.b.z << "},"
           << "\"c\":{\"x\":" << tri.c.x << ",\"y\":" << tri.c.y << ",\"z\":" << tri.c.z << "}}";
    }

    ss << "]}";
    return ss.str();
}


int main() {
    crow::SimpleApp app;

    // Serve index.html at root
    CROW_ROUTE(app, "/")
    ([]() {
        std::ifstream file("public/index.html");
        if (file) {
            return crow::response(200, std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
        }
        return crow::response(404, "Not Found");
    });

    CROW_ROUTE(app, "/index.html")
    ([]() {
        std::ifstream file("public/index.html");
        if (file) {
            return crow::response(200, std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));
        }
        return crow::response(404, "Not Found");
    });

    // API route to list available models
    CROW_ROUTE(app, "/api/models")
    ([]() {
        auto models = listModels();
        std::stringstream ss;
        ss << "{\"models\":[";
        for (size_t i = 0; i < models.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << models[i] << "\"";
        }
        ss << "]}";
        auto resp = crow::response(200, ss.str());
        resp.set_header("Content-Type", "application/json");
        resp.set_header("Access-Control-Allow-Origin", "*");
        return resp;
    });

    // API route to get triangle data
    CROW_ROUTE(app, "/api/triangles")
    ([](const crow::request& req) {
        const char* model_param = req.url_params.get("model");
        std::string model = model_param ? std::string(model_param) : "";
        auto json = getTrianglesJSON(model);
        auto resp = crow::response(200, json);
        resp.set_header("Content-Type", "application/json");
        resp.set_header("Access-Control-Allow-Origin", "*");
        return resp;
    });

    // Static file serving
    CROW_ROUTE(app, "/static/<string>")
    ([](std::string path) {
        // Security: prevent directory traversal
        if (path.find("..") != std::string::npos) {
            return crow::response(400, "Bad Request");
        }

        std::ifstream file("static/" + path);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            std::string content_type = "application/octet-stream";

            if (path.find(".js") != std::string::npos) {
                content_type = "application/javascript";
            } else if (path.find(".css") != std::string::npos) {
                content_type = "text/css";
            } else if (path.find(".html") != std::string::npos) {
                content_type = "text/html";
            }

            auto resp = crow::response(200, content);
            resp.set_header("Content-Type", content_type);
            resp.set_header("Access-Control-Allow-Origin", "*");
            return resp;
        }
        return crow::response(404, "Not Found");
    });

    app.port(3000).multithreaded().run();
    return 0;
}
