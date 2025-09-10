# Frontend Online Geometry Update Integration Plan

## Overview
This document outlines how to add frontend online geometry update capabilities while preserving the existing C++ backend implementation.

## Architecture Strategy: **Wrapper + Web API Approach**

### Core Principle
- **Keep**: All existing SemiconductorDevice, GeometryBuilder, BoundaryMesh, VTKExporter classes
- **Add**: Thin web API layer that wraps the existing C++ classes
- **Benefit**: Zero changes to proven backend, maximum reuse of existing functionality

---

## 1. Web API Layer Implementation

### 1.1 C++ Web Server (Recommended: cpp-httplib or Crow)

```cpp
// WebAPIServer.h
#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include <httplib.h>  // or crow.h
#include <nlohmann/json.hpp>

class WebAPIServer {
private:
    std::map<std::string, std::unique_ptr<SemiconductorDevice>> m_devices;
    httplib::Server m_server;
    
public:
    void startServer(int port = 8080);
    void setupEndpoints();
    
    // Device management endpoints
    void handleCreateDevice(const httplib::Request& req, httplib::Response& res);
    void handleGetDevice(const httplib::Request& req, httplib::Response& res);
    void handleUpdateDevice(const httplib::Request& req, httplib::Response& res);
    
    // Geometry operation endpoints
    void handleAddLayer(const httplib::Request& req, httplib::Response& res);
    void handleUpdateLayer(const httplib::Request& req, httplib::Response& res);
    void handleBooleanOperation(const httplib::Request& req, httplib::Response& res);
    
    // Mesh and visualization endpoints
    void handleGenerateMesh(const httplib::Request& req, httplib::Response& res);
    void handleExportVTK(const httplib::Request& req, httplib::Response& res);
    void handleExportWebGL(const httplib::Request& req, httplib::Response& res);
};
```

### 1.2 JSON Serialization for Current Classes

```cpp
// DeviceSerializer.h
class DeviceSerializer {
public:
    // Convert current classes to JSON (no changes to existing classes)
    static nlohmann::json deviceToJson(const SemiconductorDevice& device);
    static nlohmann::json layerToJson(const DeviceLayer& layer);
    static nlohmann::json meshToJson(const BoundaryMesh& mesh);
    
    // Create devices from JSON commands
    static std::unique_ptr<SemiconductorDevice> jsonToDevice(const nlohmann::json& json);
    static std::unique_ptr<DeviceLayer> jsonToLayer(const nlohmann::json& json);
    
    // Export mesh data in web-friendly formats
    static nlohmann::json meshToThreeJS(const BoundaryMesh& mesh);
    static std::string meshToSTL(const BoundaryMesh& mesh);
};
```

---

## 2. REST API Endpoints (Wrapping Your Current Backend)

### 2.1 Device Management
```http
POST /api/devices                    # Create new device
GET  /api/devices/{id}              # Get device info
PUT  /api/devices/{id}              # Update device properties
DELETE /api/devices/{id}            # Delete device
```

### 2.2 Geometry Operations (Using GeometryBuilder)
```http
POST /api/devices/{id}/layers                        # Add layer (calls addLayer())
PUT  /api/devices/{id}/layers/{layerId}             # Update layer
POST /api/devices/{id}/operations/boolean           # Boolean ops (calls GeometryBuilder::*)
POST /api/devices/{id}/operations/transform         # Transform ops (calls GeometryBuilder::*)
```

### 2.3 Mesh Operations (Using BoundaryMesh/VTKExporter)
```http
POST /api/devices/{id}/mesh/generate                # Generate mesh (calls generateGlobalBoundaryMesh())
POST /api/devices/{id}/mesh/refine                  # Refine mesh (calls refineGlobalMesh())
GET  /api/devices/{id}/mesh/export/{format}         # Export mesh (calls exportMesh())
```

### 2.4 Real-time Visualization
```http
GET  /api/devices/{id}/visualization/threejs        # Get Three.js-compatible mesh data
GET  /api/devices/{id}/visualization/vtk           # Get VTK data (uses existing VTKExporter)
WebSocket /api/devices/{id}/updates                 # Real-time updates
```

---

## 3. Frontend Web Interface

### 3.1 Technology Stack
- **3D Visualization**: Three.js (WebGL-based)
- **UI Framework**: React or Vue.js
- **Real-time Communication**: WebSockets
- **HTTP Client**: Fetch API or Axios

### 3.2 Frontend Components

```javascript
// GeometryViewer.js - Uses your VTK export data
class GeometryViewer {
    constructor(containerId) {
        this.scene = new THREE.Scene();
        this.renderer = new THREE.WebGLRenderer();
        this.camera = new THREE.PerspectiveCamera();
        
        // WebSocket connection for real-time updates
        this.socket = new WebSocket('ws://localhost:8080/api/devices/{id}/updates');
        this.setupWebSocket();
    }
    
    // Load mesh from your VTKExporter output
    async loadMeshFromVTK(deviceId) {
        const response = await fetch(`/api/devices/${deviceId}/visualization/threejs`);
        const meshData = await response.json();
        this.updateMesh(meshData);
    }
    
    // Handle real-time geometry updates
    setupWebSocket() {
        this.socket.onmessage = (event) => {
            const update = JSON.parse(event.data);
            this.handleGeometryUpdate(update);
        };
    }
}

// CommandInterface.js - Sends commands to your backend
class CommandInterface {
    async addLayer(deviceId, layerConfig) {
        const response = await fetch(`/api/devices/${deviceId}/layers`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(layerConfig)
        });
        return response.json();
    }
    
    async performBooleanOperation(deviceId, operation) {
        const response = await fetch(`/api/devices/${deviceId}/operations/boolean`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(operation)
        });
        return response.json();
    }
}
```

---

## 4. Incremental Update Strategy (Leveraging Current Face IDs)

### 4.1 Optimization Using Current BoundaryMesh Face IDs

```cpp
// IncrementalUpdater.h - New class that uses your existing face ID system
class IncrementalUpdater {
private:
    SemiconductorDevice* m_device;  // Uses your existing device
    std::map<int, uint64_t> m_faceHashes;  // Track face changes
    
public:
    struct GeometryDelta {
        std::vector<int> addedFaceIds;
        std::vector<int> removedFaceIds;
        std::vector<int> modifiedFaceIds;
    };
    
    // Calculate what changed (uses your existing BoundaryMesh face IDs)
    GeometryDelta calculateDelta(const SemiconductorDevice& oldDevice, 
                                const SemiconductorDevice& newDevice);
    
    // Export only changed parts (uses your existing VTKExporter)
    nlohmann::json exportDeltaMesh(const GeometryDelta& delta);
    
    // Update frontend incrementally
    void sendIncrementalUpdate(const GeometryDelta& delta, WebSocket& socket);
};
```

### 4.2 Leveraging Your Current Face ID System

Your current `BoundaryMesh` already has face IDs (line 33 in BoundaryMesh.h):
```cpp
struct MeshElement {
    std::array<int, 3> nodeIds;
    int id;
    int faceId;  // ← Already exists! Can be used for incremental updates
    gp_Pnt centroid;
    double area;
};
```

**Strategy**: Use this existing `faceId` to track which parts of geometry changed, then only update those faces in the frontend.

---

## 5. WebSocket Real-time Updates

### 5.1 C++ WebSocket Server (Using existing architecture)

```cpp
// RealtimeUpdateServer.h
class RealtimeUpdateServer {
private:
    SemiconductorDevice* m_device;  // Your existing device
    std::vector<WebSocket*> m_clients;
    
public:
    // When geometry changes (using your existing methods)
    void onGeometryChanged(const std::string& deviceId) {
        // Use your existing mesh generation
        m_device->buildDeviceGeometry();
        m_device->generateGlobalBoundaryMesh();
        
        // Export updated mesh (using your existing VTKExporter)
        auto meshData = DeviceSerializer::meshToThreeJS(*m_device->getGlobalMesh());
        
        // Send to all connected clients
        broadcastUpdate(meshData);
    }
    
    void broadcastUpdate(const nlohmann::json& update) {
        for (auto* client : m_clients) {
            client->send(update.dump());
        }
    }
};
```

---

## 6. Implementation Steps (Preserving Current Backend)

### Phase 1: Minimal Web API (1-2 weeks)
1. **Add HTTP server** (cpp-httplib or similar)
2. **Create DeviceSerializer** (wraps your existing classes)
3. **Implement basic endpoints** (create device, add layer, export mesh)
4. **Test with simple frontend** (HTML + Three.js)

### Phase 2: Real-time Updates (1-2 weeks)
1. **Add WebSocket support**
2. **Implement IncrementalUpdater** (uses your existing face IDs)
3. **Create frontend GeometryViewer** (consumes your VTK data)
4. **Test incremental updates**

### Phase 3: Advanced UI (2-3 weeks)
1. **Build React/Vue frontend**
2. **Add geometry editing tools**
3. **Implement command history/undo**
4. **Add material/region editors**

---

## 7. Advantages of This Approach

### ✅ **Preserves All Current Work**
- Zero changes to your existing C++ classes
- All current examples/functionality remain working
- Existing build system unchanged

### ✅ **Leverages Current Strengths**
- Uses your existing VTKExporter for visualization data
- Uses your existing face ID system for incremental updates
- Uses your existing mesh generation pipeline

### ✅ **Minimal Risk**
- Web API is a separate layer, no risk to backend
- Can develop/test incrementally
- Easy to debug (C++ backend vs web frontend separately)

### ✅ **Future-Proof**
- Backend remains suitable for desktop applications
- Web API can be extended without changing core classes
- Can add multiple frontend types (web, desktop, mobile)

---

## 8. Technology Dependencies (New additions only)

### C++ Dependencies (add to CMakeLists.txt):
```cmake
# HTTP server library
find_package(httplib REQUIRED)
# or: find_package(Crow REQUIRED)

# JSON library
find_package(nlohmann_json REQUIRED)

# WebSocket library (optional, for real-time updates)
find_package(websocketpp REQUIRED)

target_link_libraries(semiconductor_device_web
    semiconductor_device  # Your existing library
    httplib::httplib
    nlohmann_json::nlohmann_json
)
```

### Frontend Dependencies:
```json
{
  "dependencies": {
    "three": "^0.150.0",
    "react": "^18.0.0",
    "axios": "^1.0.0"
  }
}
```

---

## 9. Example User Workflow

1. **User opens web interface**
2. **Frontend loads existing device** (calls `/api/devices/{id}`)
3. **3D viewer displays geometry** (using your VTKExporter data)
4. **User issues command** (e.g., "add oxide layer")
5. **Frontend sends command** (POST to `/api/devices/{id}/layers`)
6. **Backend processes** (uses your existing `addLayer()` method)
7. **WebSocket sends delta** (only changed faces using your face IDs)
8. **Frontend updates incrementally** (Three.js updates only changed geometry)

---

## Conclusion

This approach gives you **online geometry updates** while preserving your excellent C++ backend completely. The web API acts as a thin wrapper, translating HTTP/WebSocket commands into calls to your existing classes, and converting your existing VTK export data into web-friendly formats.

**Total preservation of current investment + modern web interface capabilities.**
