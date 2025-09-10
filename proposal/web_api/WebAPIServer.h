#ifndef WEBAPI_SERVER_H
#define WEBAPI_SERVER_H

#include "../include/SemiconductorDevice.h"
#include "../include/GeometryBuilder.h"
#include "../include/VTKExporter.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <string>

/**
 * @brief Web API Server that wraps existing SemiconductorDevice functionality
 * 
 * This class provides HTTP REST endpoints that call your existing C++ classes
 * without modifying them. It acts as a bridge between web frontend and your
 * proven backend implementation.
 */
class WebAPIServer {
private:
    std::map<std::string, std::unique_ptr<SemiconductorDevice>> m_devices;
    httplib::Server m_server;
    int m_port;
    
    // Device ID generation
    std::string generateDeviceId();
    
    // JSON conversion helpers
    nlohmann::json deviceToJson(const SemiconductorDevice& device) const;
    nlohmann::json layerToJson(const DeviceLayer& layer) const;
    nlohmann::json meshToThreeJS(const BoundaryMesh& mesh) const;
    
    // Error handling
    void sendError(httplib::Response& res, int code, const std::string& message);
    void sendSuccess(httplib::Response& res, const nlohmann::json& data);
    
public:
    explicit WebAPIServer(int port = 8080);
    ~WebAPIServer() = default;
    
    // Server lifecycle
    void start();
    void stop();
    bool isRunning() const;
    
    // Setup all endpoints
    void setupEndpoints();
    
    // Device management endpoints (wrap SemiconductorDevice methods)
    void handleCreateDevice(const httplib::Request& req, httplib::Response& res);
    void handleGetDevice(const httplib::Request& req, httplib::Response& res);
    void handleListDevices(const httplib::Request& req, httplib::Response& res);
    void handleDeleteDevice(const httplib::Request& req, httplib::Response& res);
    
    // Layer management endpoints (wrap DeviceLayer methods)
    void handleAddLayer(const httplib::Request& req, httplib::Response& res);
    void handleUpdateLayer(const httplib::Request& req, httplib::Response& res);
    void handleRemoveLayer(const httplib::Request& req, httplib::Response& res);
    void handleListLayers(const httplib::Request& req, httplib::Response& res);
    
    // Geometry operations (wrap GeometryBuilder methods)
    void handleBooleanOperation(const httplib::Request& req, httplib::Response& res);
    void handleTransformOperation(const httplib::Request& req, httplib::Response& res);
    void handleCreatePrimitive(const httplib::Request& req, httplib::Response& res);
    
    // Mesh operations (wrap BoundaryMesh methods)
    void handleGenerateMesh(const httplib::Request& req, httplib::Response& res);
    void handleRefineMesh(const httplib::Request& req, httplib::Response& res);
    void handleMeshStatistics(const httplib::Request& req, httplib::Response& res);
    
    // Export endpoints (wrap existing export methods)
    void handleExportGeometry(const httplib::Request& req, httplib::Response& res);
    void handleExportMesh(const httplib::Request& req, httplib::Response& res);
    
    // Visualization endpoints (convert VTKExporter output to web formats)
    void handleGetVisualizationData(const httplib::Request& req, httplib::Response& res);
    void handleGetMeshForThreeJS(const httplib::Request& req, httplib::Response& res);
    
    // Device validation (wrap existing validation methods)
    void handleValidateDevice(const httplib::Request& req, httplib::Response& res);
    
    // Device templates (wrap existing template methods)
    void handleCreateMOSFET(const httplib::Request& req, httplib::Response& res);
    
    // CORS support for web frontend
    void enableCORS();
    
    // Get device by ID (internal helper)
    SemiconductorDevice* getDevice(const std::string& deviceId);
    const SemiconductorDevice* getDevice(const std::string& deviceId) const;
};

/**
 * @brief Utility class for converting between your existing classes and JSON
 * 
 * This class handles serialization/deserialization without modifying your
 * existing SemiconductorDevice, DeviceLayer, etc. classes.
 */
class DeviceSerializer {
public:
    // Convert existing classes to JSON (read-only, no modification)
    static nlohmann::json deviceToJson(const SemiconductorDevice& device);
    static nlohmann::json layerToJson(const DeviceLayer& layer);
    static nlohmann::json materialToJson(const MaterialProperties& material);
    static nlohmann::json meshToJson(const BoundaryMesh& mesh);
    
    // Create new instances from JSON (uses existing constructors)
    static MaterialProperties jsonToMaterial(const nlohmann::json& json);
    static DeviceRegion jsonToDeviceRegion(const std::string& regionName);
    static MaterialType jsonToMaterialType(const std::string& typeName);
    
    // Convert mesh data to Three.js-compatible format
    static nlohmann::json meshToThreeJS(const BoundaryMesh& mesh);
    static nlohmann::json meshToWebGL(const BoundaryMesh& mesh);
    
    // Convert VTK data to web formats (uses your existing VTKExporter)
    static nlohmann::json vtkToThreeJS(const std::string& vtkData);
};

/**
 * @brief Incremental update tracker using existing face ID system
 * 
 * This class leverages your existing BoundaryMesh face IDs to track
 * geometry changes for efficient frontend updates.
 */
class IncrementalUpdater {
public:
    struct GeometryDelta {
        std::vector<int> addedFaceIds;
        std::vector<int> removedFaceIds;
        std::vector<int> modifiedFaceIds;
        nlohmann::json addedMeshData;
        nlohmann::json modifiedMeshData;
    };
    
private:
    std::map<std::string, std::map<int, uint64_t>> m_deviceFaceHashes;
    
    // Calculate simple hash for face mesh elements
    uint64_t calculateFaceHash(const std::vector<MeshElement*>& elements) const;
    
public:
    // Track geometry changes using your existing face ID system
    GeometryDelta calculateDelta(const std::string& deviceId, 
                                const SemiconductorDevice& device);
    
    // Export only changed geometry (uses existing VTKExporter methods)
    nlohmann::json exportDeltaMesh(const SemiconductorDevice& device, 
                                  const GeometryDelta& delta);
    
    // Update tracking state
    void updateDeviceState(const std::string& deviceId, 
                          const SemiconductorDevice& device);
};

#endif // WEBAPI_SERVER_H
