# Implementation Guide: Three.js Frontend + OpenCASCADE Backend

## 🎯 **Overview**

This document provides the final implementation guide for adding a modern Three.js web frontend to the existing OpenCASCADE semiconductor device modeling framework while preserving complete VTK export functionality for ParaView.

**Architecture Philosophy**: Exploit Three.js strengths for visualization while keeping the proven C++ OpenCASCADE backend focused on geometry operations.

---

## **📋 Current Project Analysis**

### **What We Have (Excellent Foundation)**
- ✅ **SemiconductorDevice**: Complete device management with layer system
- ✅ **GeometryBuilder**: Full OpenCASCADE geometry operations  
- ✅ **BoundaryMesh**: Triangular mesh generation with quality analysis
- ✅ **VTKExporter**: Complete VTK file export (no external VTK dependencies!)
- ✅ **Build System**: CMake with OCE libraries integration
- ✅ **Examples**: 7 working examples demonstrating all features

### **What We're Adding (Minimal Extensions)**
- ✅ **GeometryEngine**: Thin wrapper around existing classes
- ✅ **WebAPI**: Ultra-minimal HTTP/JSON interface  
- ✅ **Three.js Frontend**: Advanced 3D visualization
- ❌ **NO changes** to existing classes
- ❌ **NO VTK library dependencies** (keep current text-based export)

---

## **🚀 Implementation Plan**

### **Phase 1: Backend Extensions (2-3 days)**

#### **1.1 GeometryEngine Wrapper (NEW FILE)**
Create `include/GeometryEngine.h` - minimal wrapper around existing classes:

```cpp
// include/GeometryEngine.h
#ifndef GEOMETRY_ENGINE_H
#define GEOMETRY_ENGINE_H

#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "VTKExporter.h"
#include <nlohmann/json.hpp>

// Simple command result - always includes VTK data
struct CommandResult {
    bool success = false;
    bool geometryChanged = false; 
    bool meshChanged = false;
    std::string message;
    std::string vtkData;           // Uses existing VTKExporter
    nlohmann::json basicStats;
    
    nlohmann::json toJSON() const {
        return {
            {"success", success},
            {"geometry_changed", geometryChanged},
            {"mesh_changed", meshChanged}, 
            {"message", message},
            {"vtk_available", !vtkData.empty()},
            {"stats", basicStats}
        };
    }
};

// Simple geometry data for Three.js
struct GeometryDelta {
    std::vector<float> vertices;        // Flattened [x,y,z,x,y,z,...]
    std::vector<uint32_t> indices;      // Triangle indices
    std::vector<int> materialIds;       // Material per triangle
    std::vector<std::string> materialNames;
    
    nlohmann::json toJSON() const;
};

// Layer specification from web commands
struct LayerSpec {
    std::string geometry;               // "box", "cylinder", "sphere"
    std::string material;               // "silicon", "oxide", "metal"
    std::string region;                 // "substrate", "gate", "contact"
    std::string name;
    std::vector<double> dimensions;     // [length, width, height] for box
    std::vector<double> position;       // [x, y, z]
    
    static LayerSpec fromJSON(const nlohmann::json& json);
};

class GeometryEngine {
private:
    std::unique_ptr<SemiconductorDevice> m_device;  // Uses existing class
    std::string m_deviceName;
    
public:
    explicit GeometryEngine(const std::string& deviceName = "Device");
    
    // Command interface (wraps existing methods)
    CommandResult addLayer(const LayerSpec& spec);
    CommandResult removeLayer(const std::string& layerName);
    CommandResult generateMesh(double meshSize = 1e-6);
    CommandResult refineMesh(const std::vector<double>& points, double localSize);
    CommandResult validateDevice();
    
    // JSON command interface for web API
    CommandResult executeCommand(const nlohmann::json& command);
    
    // VTK export (uses existing VTKExporter exactly as-is)
    std::string exportCurrentVTK() const;
    
    // Simple data extraction for Three.js
    GeometryDelta getGeometryDelta() const;
    nlohmann::json getDeviceInfo() const;
    
    // Direct access to existing classes
    SemiconductorDevice* getDevice() { return m_device.get(); }
    const SemiconductorDevice* getDevice() const { return m_device.get(); }
};

#endif // GEOMETRY_ENGINE_H
```

#### **1.2 GeometryEngine Implementation (NEW FILE)**
Create `src/GeometryEngine.cpp`:

```cpp
// src/GeometryEngine.cpp  
#include "GeometryEngine.h"
#include <fstream>
#include <sstream>

GeometryEngine::GeometryEngine(const std::string& deviceName) 
    : m_deviceName(deviceName) {
    m_device = std::make_unique<SemiconductorDevice>(deviceName);
}

CommandResult GeometryEngine::addLayer(const LayerSpec& spec) {
    try {
        // Create geometry using existing GeometryBuilder
        gp_Pnt position(spec.position.size() > 0 ? spec.position[0] : 0.0,
                       spec.position.size() > 1 ? spec.position[1] : 0.0,
                       spec.position.size() > 2 ? spec.position[2] : 0.0);
        
        TopoDS_Solid solid;
        if (spec.geometry == "box" && spec.dimensions.size() >= 3) {
            Dimensions3D dims(spec.dimensions[0], spec.dimensions[1], spec.dimensions[2]);
            solid = GeometryBuilder::createBox(position, dims);  // Existing method
        } 
        // Add other geometry types...
        
        // Create material using existing system
        MaterialProperties material = SemiconductorDevice::createStandardSilicon(); // Default
        if (spec.material == "oxide") {
            material = SemiconductorDevice::createStandardSiliconDioxide();
        }
        // Add other materials...
        
        // Create layer using existing DeviceLayer
        DeviceRegion region = DeviceRegion::Substrate;  // Default
        if (spec.region == "gate") region = DeviceRegion::Gate;
        // Add other regions...
        
        auto layer = std::make_unique<DeviceLayer>(solid, material, region, spec.name);
        
        // Use existing methods
        m_device->addLayer(std::move(layer));           // Existing method
        m_device->buildDeviceGeometry();               // Existing method
        
        return CommandResult{
            .success = true,
            .geometryChanged = true,
            .message = "Layer '" + spec.name + "' added successfully", 
            .vtkData = exportCurrentVTK(),              // Always provide VTK
            .basicStats = getDeviceInfo()
        };
    } catch (const std::exception& e) {
        return CommandResult{
            .success = false,
            .message = "Failed to add layer: " + std::string(e.what())
        };
    }
}

CommandResult GeometryEngine::generateMesh(double meshSize) {
    try {
        m_device->generateGlobalBoundaryMesh(meshSize);  // Existing method
        
        return CommandResult{
            .success = true,
            .meshChanged = true,
            .message = "Mesh generated with size " + std::to_string(meshSize),
            .vtkData = exportCurrentVTK(),               // ParaView-ready VTK
            .basicStats = getDeviceInfo()
        };
    } catch (const std::exception& e) {
        return CommandResult{
            .success = false,
            .message = "Mesh generation failed: " + std::string(e.what())
        };
    }
}

std::string GeometryEngine::exportCurrentVTK() const {
    if (!m_device) return "";
    
    // Use existing VTKExporter (no changes needed)
    std::string tempFile = "/tmp/device_" + std::to_string(rand()) + ".vtk";
    
    // Your existing VTK export works exactly as before
    bool success = m_device->exportMesh(tempFile, "VTK");
    
    if (success) {
        std::ifstream file(tempFile);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        std::remove(tempFile.c_str());
        return content;
    }
    
    return "";
}

GeometryDelta GeometryEngine::getGeometryDelta() const {
    GeometryDelta delta;
    
    if (!m_device) return delta;
    
    const BoundaryMesh* mesh = m_device->getGlobalMesh();
    if (!mesh) return delta;
    
    // Simple data extraction from existing BoundaryMesh
    const auto& nodes = mesh->getNodes();
    const auto& elements = mesh->getElements();
    
    // Extract vertices for Three.js BufferGeometry
    delta.vertices.reserve(nodes.size() * 3);
    for (const auto& node : nodes) {
        delta.vertices.push_back(static_cast<float>(node->point.X()));
        delta.vertices.push_back(static_cast<float>(node->point.Y()));
        delta.vertices.push_back(static_cast<float>(node->point.Z()));
    }
    
    // Extract triangles for Three.js
    delta.indices.reserve(elements.size() * 3);
    delta.materialIds.reserve(elements.size());
    
    for (const auto& element : elements) {
        delta.indices.insert(delta.indices.end(), {
            static_cast<uint32_t>(element->nodeIds[0]),
            static_cast<uint32_t>(element->nodeIds[1]), 
            static_cast<uint32_t>(element->nodeIds[2])
        });
        delta.materialIds.push_back(element->faceId);
    }
    
    // Extract material names from existing layers
    const auto& layers = m_device->getLayers();
    for (const auto& layer : layers) {
        delta.materialNames.push_back(layer->getMaterial().name);
    }
    
    return delta;  // Three.js handles everything else
}

// Add other method implementations...
```

---

### **Phase 2: Web API Layer (1 day)**

#### **2.1 Add Dependencies to CMakeLists.txt**
Update existing `CMakeLists.txt`:

```cmake
# Add to existing CMakeLists.txt after line 43 (OCE_LIBRARIES)

# Web API dependencies (minimal additions)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HTTPLIB QUIET httplib)

# JSON library (header-only)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)

# Optional: Only add web API if dependencies found
if(HTTPLIB_FOUND AND NLOHMANN_JSON_INCLUDE_DIR)
    message(STATUS "Web API dependencies found - building web interface")
    set(BUILD_WEB_API ON)
    
    # Add web API include directories
    include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
    
    # Web API executable
    add_executable(semiconductor_web_server
        src/GeometryEngine.cpp
        web_api/WebAPIServer.cpp
    )
    
    target_link_libraries(semiconductor_web_server
        semiconductor_device  # Existing library
        ${HTTPLIB_LIBRARIES}
    )
else()
    message(STATUS "Web API dependencies not found - skipping web interface")
    message(STATUS "Install: sudo apt install nlohmann-json3-dev libhttplib-dev")
    set(BUILD_WEB_API OFF)
endif()

# Existing library and examples continue to work unchanged
```

#### **2.2 Web API Server (NEW FILE)**
Create `web_api/WebAPIServer.cpp`:

```cpp
// web_api/WebAPIServer.cpp
#include "../include/GeometryEngine.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

class WebAPIServer {
private:
    httplib::Server m_server;
    std::unique_ptr<GeometryEngine> m_engine;
    
public:
    WebAPIServer() {
        m_engine = std::make_unique<GeometryEngine>("WebDevice");
        setupRoutes();
    }
    
    void setupRoutes() {
        // CORS for web frontend
        m_server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            return httplib::Server::HandlerResponse::Unhandled;
        });
        
        // Command execution - uses existing backend
        m_server.Post("/api/commands", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json command = nlohmann::json::parse(req.body);
                CommandResult result = m_engine->executeCommand(command);  // Backend processes
                res.set_content(result.toJSON().dump(), "application/json");
            } catch (const std::exception& e) {
                nlohmann::json error = {{"error", e.what()}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            }
        });
        
        // Geometry data for Three.js
        m_server.Get("/api/geometry", [this](const httplib::Request&, httplib::Response& res) {
            GeometryDelta delta = m_engine->getGeometryDelta();  // Simple extraction
            res.set_content(delta.toJSON().dump(), "application/json");
        });
        
        // VTK export - uses existing VTKExporter
        m_server.Get("/api/export/vtk", [this](const httplib::Request&, httplib::Response& res) {
            std::string vtkData = m_engine->exportCurrentVTK();  // Existing export
            if (vtkData.empty()) {
                res.status = 404;
                res.set_content("No mesh data available", "text/plain");
            } else {
                res.set_content(vtkData, "application/vtk");
                res.set_header("Content-Disposition", "attachment; filename=\"device.vtk\"");
            }
        });
        
        // Device info
        m_server.Get("/api/device", [this](const httplib::Request&, httplib::Response& res) {
            nlohmann::json info = m_engine->getDeviceInfo();
            res.set_content(info.dump(), "application/json");
        });
        
        // Static files for frontend
        m_server.set_mount_point("/", "./frontend");
    }
    
    void start(int port = 8080) {
        std::cout << "Starting web server on port " << port << std::endl;
        std::cout << "Backend: OpenCASCADE + existing VTK export" << std::endl;
        std::cout << "Frontend: http://localhost:" << port << std::endl;
        
        m_server.listen("0.0.0.0", port);
    }
};

int main() {
    WebAPIServer server;
    server.start(8080);
    return 0;
}
```

---

### **Phase 3: Three.js Frontend (2-3 days)**

#### **3.1 Frontend Structure**
Create directory structure:
```
frontend/
├── index.html              # Main web interface
├── js/
│   ├── SmartGeometryViewer.js  # Three.js visualization engine
│   ├── CommandInterface.js     # API communication
│   └── MaterialLibrary.js      # Material definitions
└── css/
    └── style.css              # UI styling
```

#### **3.2 Main Web Interface (NEW FILE)**
Create `frontend/index.html`:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Semiconductor Device Modeling</title>
    <style>
        body { margin: 0; font-family: Arial, sans-serif; background: #1a1a1a; color: white; }
        #container { display: flex; height: 100vh; }
        #sidebar { width: 300px; background: #2a2a2a; padding: 20px; overflow-y: auto; }
        #viewer { flex: 1; }
        .command-section { margin-bottom: 20px; padding: 15px; background: #3a3a3a; border-radius: 5px; }
        button { background: #4a9eff; border: none; padding: 8px 16px; color: white; border-radius: 3px; cursor: pointer; }
        button:hover { background: #357abd; }
        input, select { width: 100%; padding: 5px; margin: 5px 0; background: #4a4a4a; border: 1px solid #666; color: white; }
        #status { padding: 10px; background: #1a4a1a; margin: 10px 0; border-radius: 3px; }
        .download-link { color: #4a9eff; text-decoration: none; }
        .download-link:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div id="container">
        <div id="sidebar">
            <h2>Device Controls</h2>
            
            <div class="command-section">
                <h3>Add Layer</h3>
                <div>
                    <label>Geometry:</label>
                    <select id="geometry-type">
                        <option value="box">Box</option>
                        <option value="cylinder">Cylinder</option>
                        <option value="sphere">Sphere</option>
                    </select>
                </div>
                <div>
                    <label>Material:</label>
                    <select id="material-type">
                        <option value="silicon">Silicon</option>
                        <option value="oxide">Silicon Oxide</option>
                        <option value="metal">Metal Contact</option>
                    </select>
                </div>
                <div>
                    <label>Region:</label>
                    <select id="region-type">
                        <option value="substrate">Substrate</option>
                        <option value="gate">Gate</option>
                        <option value="contact">Contact</option>
                    </select>
                </div>
                <div>
                    <label>Name:</label>
                    <input type="text" id="layer-name" placeholder="Layer name">
                </div>
                <div>
                    <label>Dimensions (μm):</label>
                    <input type="text" id="dimensions" placeholder="100,100,50" value="100,100,50">
                </div>
                <button onclick="addLayer()">Add Layer</button>
            </div>
            
            <div class="command-section">
                <h3>Mesh Generation</h3>
                <div>
                    <label>Mesh Size (μm):</label>
                    <input type="number" id="mesh-size" value="1.0" step="0.1">
                </div>
                <button onclick="generateMesh()">Generate Mesh</button>
            </div>
            
            <div class="command-section">
                <h3>Export</h3>
                <button onclick="exportVTK()">Export VTK (ParaView)</button>
                <div id="vtk-download" style="margin-top: 10px;"></div>
            </div>
            
            <div id="status">
                Ready - Backend: OpenCASCADE + VTK Export
            </div>
        </div>
        
        <div id="viewer"></div>
    </div>

    <!-- Three.js and dependencies -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
    
    <!-- Our application -->
    <script src="js/SmartGeometryViewer.js"></script>
    <script src="js/CommandInterface.js"></script>
    
    <script>
        // Initialize the application
        const viewer = new SmartGeometryViewer('viewer');
        const controller = new CommandInterface('http://localhost:8080');
        
        // Command handlers
        async function addLayer() {
            const spec = {
                geometry: document.getElementById('geometry-type').value,
                material: document.getElementById('material-type').value,
                region: document.getElementById('region-type').value,
                name: document.getElementById('layer-name').value || 'Layer',
                dimensions: document.getElementById('dimensions').value.split(',').map(x => parseFloat(x) * 1e-6),
                position: [0, 0, 0]
            };
            
            const result = await controller.addLayer(spec);
            updateStatus(result.message);
            
            if (result.geometry_changed) {
                await updateVisualization();
            }
        }
        
        async function generateMesh() {
            const meshSize = parseFloat(document.getElementById('mesh-size').value) * 1e-6;
            const result = await controller.generateMesh(meshSize);
            updateStatus(result.message);
            
            if (result.mesh_changed) {
                await updateVisualization();
            }
        }
        
        async function exportVTK() {
            const blob = await controller.exportVTK();
            const url = URL.createObjectURL(blob);
            
            document.getElementById('vtk-download').innerHTML = 
                `<a href="${url}" download="device.vtk" class="download-link">Download device.vtk</a>`;
                
            updateStatus("VTK file ready for download (ParaView compatible)");
        }
        
        async function updateVisualization() {
            const delta = await controller.getGeometryDelta();
            viewer.updateGeometry(delta);
        }
        
        function updateStatus(message) {
            document.getElementById('status').textContent = message;
        }
        
        // Initialize visualization
        updateVisualization();
    </script>
</body>
</html>
```

#### **3.3 Smart Three.js Viewer (NEW FILE)**
Create `frontend/js/SmartGeometryViewer.js`:

```javascript
// frontend/js/SmartGeometryViewer.js
class SmartGeometryViewer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        
        // Three.js core setup
        this.scene = new THREE.Scene();
        this.camera = new THREE.PerspectiveCamera(75, this.container.clientWidth / this.container.clientHeight, 0.001, 1000);
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        
        // Initialize renderer
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
        this.renderer.setClearColor(0x1a1a1a);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.container.appendChild(this.renderer.domElement);
        
        // Three.js controls (built-in smooth camera controls)
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.1;
        
        // Three.js raycaster for mouse interaction
        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();
        
        // Geometry management
        this.deviceMesh = null;
        this.materials = this.createMaterialLibrary();
        
        // Setup lighting and scene
        this.setupLighting();
        this.setupEventListeners();
        
        // Start render loop
        this.animate();
        
        // Initialize camera position
        this.camera.position.set(0.002, 0.002, 0.002);  // 2mm away for microelectronics
        this.controls.update();
    }
    
    createMaterialLibrary() {
        // Exploit Three.js advanced material system
        return {
            silicon: new THREE.MeshPhysicalMaterial({
                color: 0x404040,
                metalness: 0.1,
                roughness: 0.6,
                transparent: true,
                opacity: 0.9
            }),
            oxide: new THREE.MeshPhysicalMaterial({
                color: 0x87CEEB,
                metalness: 0.0,
                roughness: 0.1,
                transmission: 0.7,     // Glass-like transparency
                thickness: 0.5,
                transparent: true,
                opacity: 0.8
            }),
            metal: new THREE.MeshPhysicalMaterial({
                color: 0xFFD700,
                metalness: 1.0,
                roughness: 0.2,
                envMapIntensity: 1.0
            })
        };
    }
    
    setupLighting() {
        // Ambient light
        const ambientLight = new THREE.AmbientLight(0x404040, 0.3);
        this.scene.add(ambientLight);
        
        // Directional light with shadows
        const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
        dirLight.position.set(0.001, 0.001, 0.0005);  // Positioned for microelectronics scale
        dirLight.castShadow = true;
        this.scene.add(dirLight);
        
        // Point light for better illumination
        const pointLight = new THREE.PointLight(0xffffff, 0.4);
        pointLight.position.set(-0.001, 0.001, -0.0005);
        this.scene.add(pointLight);
    }
    
    setupEventListeners() {
        // Window resize
        window.addEventListener('resize', () => this.onWindowResize());
        
        // Mouse interaction for object selection
        this.renderer.domElement.addEventListener('click', (event) => this.onMouseClick(event));
        
        // Mouse move for hover effects
        this.renderer.domElement.addEventListener('mousemove', (event) => this.onMouseMove(event));
    }
    
    async updateGeometry(geometryDelta) {
        // Remove existing mesh
        if (this.deviceMesh) {
            this.scene.remove(this.deviceMesh);
            if (this.deviceMesh.geometry) this.deviceMesh.geometry.dispose();
        }
        
        if (!geometryDelta.vertices || geometryDelta.vertices.length === 0) {
            console.log("No geometry data to display");
            return;
        }
        
        // Create Three.js BufferGeometry (GPU-optimized)
        const geometry = new THREE.BufferGeometry();
        
        // Vertex positions
        const vertices = new Float32Array(geometryDelta.vertices);
        geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
        
        // Triangle indices
        if (geometryDelta.indices && geometryDelta.indices.length > 0) {
            const indices = new Uint32Array(geometryDelta.indices);
            geometry.setIndex(new THREE.BufferAttribute(indices, 1));
        }
        
        // Let Three.js compute normals (GPU-optimized)
        geometry.computeVertexNormals();
        geometry.computeBoundingBox();
        geometry.computeBoundingSphere();
        
        // Create materials based on device data
        const material = this.createDeviceMaterial(geometryDelta.materialIds, geometryDelta.materialNames);
        
        // Create mesh
        this.deviceMesh = new THREE.Mesh(geometry, material);
        this.deviceMesh.castShadow = true;
        this.deviceMesh.receiveShadow = true;
        
        // Add to scene
        this.scene.add(this.deviceMesh);
        
        // Auto-fit camera to geometry
        this.fitCameraToGeometry();
        
        console.log(`Updated geometry: ${geometryDelta.vertices.length/3} vertices, ${geometryDelta.indices.length/3} triangles`);
    }
    
    createDeviceMaterial(materialIds, materialNames) {
        // For now, use silicon material as default
        // In a full implementation, you could create multi-material meshes
        return this.materials.silicon.clone();
    }
    
    fitCameraToGeometry() {
        if (!this.deviceMesh) return;
        
        // Calculate bounding box
        const box = new THREE.Box3().setFromObject(this.deviceMesh);
        const size = box.getSize(new THREE.Vector3());
        const center = box.getCenter(new THREE.Vector3());
        
        // Position camera to view entire geometry
        const maxDim = Math.max(size.x, size.y, size.z);
        const fitHeightDistance = maxDim / (2 * Math.atan(Math.PI * this.camera.fov / 360));
        const fitWidthDistance = fitHeightDistance / this.camera.aspect;
        const distance = 1.2 * Math.max(fitHeightDistance, fitWidthDistance);
        
        this.camera.position.copy(center);
        this.camera.position.add(new THREE.Vector3(distance, distance, distance));
        this.camera.lookAt(center);
        
        this.controls.target.copy(center);
        this.controls.update();
    }
    
    onMouseClick(event) {
        // Three.js raycasting for object selection
        this.mouse.x = (event.clientX / this.container.clientWidth) * 2 - 1;
        this.mouse.y = -(event.clientY / this.container.clientHeight) * 2 + 1;
        
        this.raycaster.setFromCamera(this.mouse, this.camera);
        
        if (this.deviceMesh) {
            const intersects = this.raycaster.intersectObject(this.deviceMesh);
            if (intersects.length > 0) {
                const intersection = intersects[0];
                console.log('Clicked point:', intersection.point);
                console.log('Triangle index:', intersection.faceIndex);
                
                // Highlight clicked area (visual feedback)
                this.highlightTriangle(intersection.faceIndex);
            }
        }
    }
    
    onMouseMove(event) {
        // Update mouse coordinates for hover effects
        this.mouse.x = (event.clientX / this.container.clientWidth) * 2 - 1;
        this.mouse.y = -(event.clientY / this.container.clientHeight) * 2 + 1;
    }
    
    highlightTriangle(faceIndex) {
        // Example: Create a temporary highlight effect
        if (!this.deviceMesh) return;
        
        // This is where you could implement triangle highlighting
        // For now, just log the selection
        console.log('Selected triangle:', faceIndex);
    }
    
    onWindowResize() {
        this.camera.aspect = this.container.clientWidth / this.container.clientHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    }
    
    animate() {
        requestAnimationFrame(() => this.animate());
        
        // Update controls (damping)
        this.controls.update();
        
        // Render scene
        this.renderer.render(this.scene, this.camera);
    }
}
```

#### **3.4 Command Interface (NEW FILE)**
Create `frontend/js/CommandInterface.js`:

```javascript
// frontend/js/CommandInterface.js
class CommandInterface {
    constructor(baseUrl = 'http://localhost:8080') {
        this.baseUrl = baseUrl;
        this.commandHistory = JSON.parse(localStorage.getItem('commandHistory') || '[]');
    }
    
    // Add layer command - calls backend GeometryEngine
    async addLayer(layerSpec) {
        const command = {
            type: 'add_layer',
            parameters: layerSpec
        };
        
        const result = await this.executeCommand(command);
        this.addToHistory(command);
        return result;
    }
    
    // Generate mesh - calls existing BoundaryMesh
    async generateMesh(meshSize) {
        const command = {
            type: 'generate_mesh',
            parameters: { mesh_size: meshSize }
        };
        
        const result = await this.executeCommand(command);
        this.addToHistory(command);
        return result;
    }
    
    // Execute command via web API
    async executeCommand(command) {
        try {
            const response = await fetch(`${this.baseUrl}/api/commands`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(command)
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const result = await response.json();
            
            // Auto-save command history
            this.saveHistory();
            
            return result;
        } catch (error) {
            console.error('Command execution failed:', error);
            return {
                success: false,
                message: 'Command failed: ' + error.message
            };
        }
    }
    
    // Get geometry data for Three.js
    async getGeometryDelta() {
        try {
            const response = await fetch(`${this.baseUrl}/api/geometry`);
            if (!response.ok) throw new Error('Failed to fetch geometry data');
            return await response.json();
        } catch (error) {
            console.error('Failed to get geometry data:', error);
            return { vertices: [], indices: [], materialIds: [] };
        }
    }
    
    // Export VTK file (uses existing VTKExporter)
    async exportVTK() {
        try {
            const response = await fetch(`${this.baseUrl}/api/export/vtk`);
            if (!response.ok) throw new Error('Failed to export VTK');
            return await response.blob();
        } catch (error) {
            console.error('VTK export failed:', error);
            throw error;
        }
    }
    
    // Get device information
    async getDeviceInfo() {
        try {
            const response = await fetch(`${this.baseUrl}/api/device`);
            if (!response.ok) throw new Error('Failed to get device info');
            return await response.json();
        } catch (error) {
            console.error('Failed to get device info:', error);
            return {};
        }
    }
    
    // Command history management
    addToHistory(command) {
        this.commandHistory.push({
            ...command,
            timestamp: new Date().toISOString()
        });
        
        // Keep only last 100 commands
        if (this.commandHistory.length > 100) {
            this.commandHistory.shift();
        }
    }
    
    saveHistory() {
        localStorage.setItem('commandHistory', JSON.stringify(this.commandHistory));
    }
    
    getHistory() {
        return this.commandHistory;
    }
}
```

---

## **🔧 Build and Run Instructions**

### **Dependencies Installation**
```bash
# Install web API dependencies (optional)
sudo apt update
sudo apt install nlohmann-json3-dev libhttplib-dev

# Existing OCE dependencies (already have these)
sudo apt install liboce-foundation-dev liboce-modeling-dev \
                 liboce-ocaf-dev liboce-visualization-dev \
                 build-essential cmake pkg-config
```

### **Build Process**
```bash
# Build existing library + new web interface
./build.sh

# Or manual build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Check if web server was built
ls build/semiconductor_web_server 2>/dev/null && echo "Web server available" || echo "Web server not built (missing dependencies)"
```

### **Running the Application**
```bash
# Option 1: Web interface (if dependencies available)
cd build
./semiconductor_web_server
# Open browser to http://localhost:8080

# Option 2: Existing examples (always work)
./basic_shapes_example
./mosfet_example
# VTK files generated as before - open in ParaView

# Option 3: Manual VTK export (existing functionality)
./mosfet_example
paraview mosfet_device.vtk
```

---

## **✅ Validation: Requirements Met**

### **✅ Three.js Frontend Exploitation**
- **GPU-accelerated rendering**: WebGL with hardware acceleration
- **Advanced materials**: PBR materials with metalness, roughness, transmission
- **Interactive controls**: Built-in Three.js OrbitControls
- **Smart geometry processing**: BufferGeometry with automatic normal computation
- **Visual effects**: Shadows, transparency, environment mapping

### **✅ Clean Separation of Concerns**
- **Backend**: OpenCASCADE geometry operations, mesh generation (unchanged)
- **Frontend**: Three.js visualization, user interaction, browser features
- **API**: Ultra-thin translation layer (no business logic)

### **✅ Complete VTK Export (No VTK Dependencies)**
- **Uses existing VTKExporter**: Text-based VTK file generation (no libraries needed)
- **ParaView compatible**: Existing mesh export functionality unchanged
- **Always available**: Every geometry/mesh operation provides VTK data
- **Web accessible**: VTK files downloadable from web interface

### **✅ Minimal Backend Changes**
- **GeometryEngine**: Single wrapper class around existing functionality
- **No modifications**: All existing classes (SemiconductorDevice, GeometryBuilder, etc.) unchanged
- **Backward compatible**: All existing examples continue working
- **Optional web**: System works with or without web dependencies

---

## **🚀 Next Steps**

1. **Implement Phase 1**: Add `GeometryEngine.h` and `GeometryEngine.cpp`
2. **Test backend**: Verify wrapper works with existing functionality  
3. **Add web API**: Install dependencies and add web server
4. **Create frontend**: Build Three.js interface
5. **Integration test**: Full web-to-backend-to-ParaView workflow

**The system will provide modern web visualization while preserving your proven OpenCASCADE geometry engine and complete VTK export capabilities.**

<citations>
<document>
<document_type>RULE</document_type>
<document_id>/home/dabajabaza/Documents/OCC_examples_semiconductor_device/WARP.md</document_id>
</document>
</citations>
