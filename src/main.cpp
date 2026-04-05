#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmath>
#include <dirent.h>
#include <algorithm>

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

std::string getIndexHTML() {
    return R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mesh Viewer</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f0f1e; color: #fff; }

        #container {
            display: flex;
            height: 100vh;
        }

        #viewport {
            flex: 1;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            position: relative;
        }

        canvas { display: block; width: 100%; height: 100%; }

        #controls {
            width: 300px;
            padding: 20px;
            background: #111;
            border-left: 1px solid #333;
            overflow-y: auto;
        }

        .control-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            font-weight: 500;
            margin-bottom: 5px;
            font-size: 14px;
        }

        input[type="range"] {
            width: 100%;
            height: 6px;
            cursor: pointer;
        }

        input[type="number"] {
            width: 100%;
            padding: 8px;
            background: #222;
            border: 1px solid #444;
            color: #fff;
            border-radius: 4px;
            font-size: 13px;
        }

        input[type="number"]:focus {
            outline: none;
            border-color: #0f6;
        }

        .control-value {
            font-size: 12px;
            color: #888;
            margin-top: 4px;
        }

        h2 {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 15px;
            color: #0f6;
        }

        #stats {
            margin-top: 30px;
            padding: 15px;
            background: #1a1a2e;
            border-radius: 4px;
            font-size: 13px;
            line-height: 1.6;
        }

        #stats p {
            margin: 5px 0;
        }

        .spinner {
            display: inline-block;
            width: 14px;
            height: 14px;
            border: 2px solid #444;
            border-top-color: #0f6;
            border-radius: 50%;
            animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div id="container">
        <div id="viewport"></div>
        <div id="controls">
            <h2>Model</h2>
            <div class="control-group">
                <label>Select OBJ</label>
                <select id="model-select" style="width: 100%; padding: 8px; background: #222; border: 1px solid #444; color: #fff; border-radius: 4px; font-size: 13px; cursor: pointer;">
                    <option value="">Default Cube</option>
                </select>
            </div>

            <h2>Transform Controls</h2>

            <div class="control-group">
                <label>Rotate X (°)</label>
                <input type="range" id="rotateX" min="-180" max="180" value="0" step="1">
                <div class="control-value"><span id="rotateX-val">0</span>°</div>
            </div>

            <div class="control-group">
                <label>Rotate Y (°)</label>
                <input type="range" id="rotateY" min="-180" max="180" value="0" step="1">
                <div class="control-value"><span id="rotateY-val">0</span>°</div>
            </div>

            <div class="control-group">
                <label>Rotate Z (°)</label>
                <input type="range" id="rotateZ" min="-180" max="180" value="0" step="1">
                <div class="control-value"><span id="rotateZ-val">0</span>°</div>
            </div>

            <h2>Position Controls</h2>

            <div class="control-group">
                <label>Translate X</label>
                <input type="number" id="translateX" value="0" step="0.1">
            </div>

            <div class="control-group">
                <label>Translate Y</label>
                <input type="number" id="translateY" value="0" step="0.1">
            </div>

            <div class="control-group">
                <label>Translate Z</label>
                <input type="number" id="translateZ" value="0" step="0.1">
            </div>

            <h2>Scale Control</h2>

            <div class="control-group">
                <label>Scale</label>
                <input type="range" id="scale" min="0.1" max="5" value="1" step="0.05">
                <div class="control-value"><span id="scale-val">1.0</span>x</div>
            </div>

            <div id="stats">
                <p><span class="spinner"></span> Loading mesh...</p>
            </div>
        </div>
    </div>

    <script src="/static/dist/app.js"></script>
</body>
</html>
    )";
}

// Parse query string from path; returns query map and updates path
std::string parseQueryString(std::string& path, std::string& model) {
    model = "";
    size_t qpos = path.find('?');
    if (qpos == std::string::npos) {
        return "";
    }

    std::string query = path.substr(qpos + 1);
    path = path.substr(0, qpos);

    // Parse key=value pairs
    size_t pos = 0;
    while (pos < query.length()) {
        size_t eq = query.find('=', pos);
        if (eq == std::string::npos) break;

        std::string key = query.substr(pos, eq - pos);
        size_t amp = query.find('&', eq);
        std::string value = (amp != std::string::npos)
            ? query.substr(eq + 1, amp - eq - 1)
            : query.substr(eq + 1);

        if (key == "model") {
            // Simple URL decode: convert '+' to space
            model = value;
            for (auto& c : model) {
                if (c == '+') c = ' ';
            }
        }

        pos = (amp != std::string::npos) ? amp + 1 : query.length();
    }

    return query;
}

void handle_client(int client_socket) {
    char buffer[4096];
    int n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (n > 0) {
        buffer[n] = '\0';
        std::string request(buffer);

        // Parse request line
        std::string method, path, version;
        std::istringstream iss(request);
        iss >> method >> path >> version;

        std::string response;
        std::string model;
        parseQueryString(path, model);

        if (path == "/" || path == "/index.html") {
            std::string html = getIndexHTML();
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/html; charset=utf-8\r\n";
            response += "Content-Length: " + std::to_string(html.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Connection: close\r\n\r\n";
            response += html;
        }
        else if (path == "/api/models") {
            // List available OBJ models
            std::vector<std::string> models = listModels();
            std::stringstream ss;
            ss << "{\"models\":[";
            for (size_t i = 0; i < models.size(); ++i) {
                if (i > 0) ss << ",";
                ss << "\"" << models[i] << "\"";
            }
            ss << "]}";

            std::string json = ss.str();
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Connection: close\r\n\r\n";
            response += json;
        }
        else if (path == "/api/triangles") {
            // Return triangle data (optionally for a specific model)
            std::string json = getTrianglesJSON(model);
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Connection: close\r\n\r\n";
            response += json;
        }
        else if (path.substr(0, 8) == "/static/") {
            // Generic static file handler
            std::string filepath = path.substr(1); // Remove leading slash

            // Security: prevent directory traversal
            if (filepath.find("..") != std::string::npos) {
                response = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
            } else {
                std::ifstream file(filepath);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    response = "HTTP/1.1 200 OK\r\n";

                    // Determine content type
                    std::string content_type = "application/octet-stream";
                    if (filepath.find(".js") != std::string::npos) {
                        content_type = "application/javascript";
                    } else if (filepath.find(".css") != std::string::npos) {
                        content_type = "text/css";
                    } else if (filepath.find(".html") != std::string::npos) {
                        content_type = "text/html";
                    }

                    response += "Content-Type: " + content_type + "\r\n";
                    response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
                    response += "Access-Control-Allow-Origin: *\r\n";
                    response += "Connection: close\r\n\r\n";
                    response += content;
                } else {
                    response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                }
            }
        }
        else if (method == "OPTIONS") {
            response = "HTTP/1.1 200 OK\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Connection: close\r\n\r\n";
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
        }

        send(client_socket, response.c_str(), response.length(), 0);
    }

    close(client_socket);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(3000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket to port 3000 (port may be in use)\n";
        perror("bind");
        return 1;
    }

    listen(server_socket, 5);
    std::cout << "Server listening on http://localhost:3000\n";

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            std::cerr << "Error accepting connection\n";
            continue;
        }

        std::thread(&handle_client, client_socket).detach();
    }

    close(server_socket);
    return 0;
}
