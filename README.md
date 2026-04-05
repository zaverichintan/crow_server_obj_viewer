# 3D Mesh Viewer

A web-based 3D mesh viewer built with C++ backend and Three.js frontend. Load and visualize OBJ files with interactive controls.

## Features

- 🎯 Interactive 3D mesh visualization using Three.js
- 📁 Load custom OBJ files from a `models/` directory
- 🎮 Full transform controls (rotation, translation, scale)
- 🔄 Hot-swap between different models via dropdown selector
- 💾 Built-in sample tetrahedron model
- 🖥️ Real-time rendering with OrbitControls camera

## Project Structure

```
test_crow/
├── src/
│   └── main.cpp              # C++ server (OBJ parser, REST API)
├── static/
│   ├── package.json          # npm configuration
│   ├── js/
│   │   └── app.js            # Three.js application
│   ├── dist/
│   │   └── app.js            # Bundled JavaScript (generated)
│   └── node_modules/         # npm dependencies
├── models/
│   └── tetrahedron.obj       # Sample OBJ file
├── build/                    # CMake build output
├── CMakeLists.txt            # Build configuration
└── README.md
```

## Prerequisites

- C++17 compatible compiler
- CMake 3.16+
- Node.js & npm (for frontend bundling)

## Build & Run

### Backend (C++ Server)

```bash
cd test_crow
mkdir -p build && cd build
cmake ..
make
./mesh_server
```

Server runs on `http://localhost:3000`

### Frontend (JavaScript)

```bash
cd test_crow/static
npm install
npm run build
```

## Usage

1. Start the server: `./build/mesh_server`
2. Open http://localhost:3000 in your browser
3. Use dropdown to select OBJ models
4. Use sliders to transform the mesh (rotation, position, scale)

## Adding Custom Models

1. Place `.obj` files in the `models/` directory
2. Rebuild: `cd build && make`
3. Restart the server

## API Endpoints

- `GET /` - HTML viewer
- `GET /api/models` - List available models
- `GET /api/triangles` - Get default cube (JSON)
- `GET /api/triangles?model=filename.obj` - Get specific model

## Technology Stack

- **Backend:** C++17, BSD Sockets
- **Frontend:** Three.js, OrbitControls, ES6 Modules
- **Build:** CMake, esbuild
- **Port:** 3000
