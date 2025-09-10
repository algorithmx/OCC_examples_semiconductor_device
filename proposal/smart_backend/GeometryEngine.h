#ifndef GEOMETRY_ENGINE_H
#define GEOMETRY_ENGINE_H

#include "../include/SemiconductorDevice.h"
#include "../include/GeometryBuilder.h"
#include "../include/VTKExporter.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <array>

/**
 * @brief Minimal extension to existing classes for Three.js support
 * 
 * This class wraps your existing SemiconductorDevice, GeometryBuilder, 
 * and VTKExporter classes with a thin command interface. Your existing
 * classes remain completely unchanged.
 */

// Simple command result structure
struct CommandResult {
    bool success = false;
    bool geometryChanged = false;
    bool meshChanged = false;
    std::string message;
    std::string vtkData;           // Always available for ParaView
    nlohmann::json basicStats;     // Simple stats for UI display
    
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

// Simple geometry data for Three.js (not VTK processing!)
struct GeometryDelta {
    std::vector<std::array<float, 3>> vertices;  // Raw vertex positions
    std::vector<uint32_t> indices;               // Triangle indices
    std::vector<int> materialIds;                // Material ID per triangle
    std::vector<std::string> materialNames;      // Material names
    
    nlohmann::json toJSON() const {
        nlohmann::json json;
        
        // Flatten vertices for Three.js BufferAttribute
        std::vector<float> flatVertices;
        for (const auto& vertex : vertices) {
            flatVertices.insert(flatVertices.end(), vertex.begin(), vertex.end());
        }
        
        json["vertices"] = flatVertices;
        json["indices"] = indices;
        json["material_ids"] = materialIds;
        json["material_names"] = materialNames;
        json["vertex_count"] = vertices.size();
        json["triangle_count"] = indices.size() / 3;
        
        return json;
    }
};

// Layer specification for commands
struct LayerSpec {
    std::string geometry;      // "box", "cylinder", "sphere"
    std::string material;      // "silicon", "oxide", "metal"
    std::string region;        // "substrate", "gate", "contact"
    std::string name;
    std::vector<double> dimensions;  // geometry-specific dimensions
    std::vector<double> position;    // [x, y, z] position
    
    static LayerSpec fromJSON(const nlohmann::json& json) {
        LayerSpec spec;
        spec.geometry = json.value("geometry", "box");
        spec.material = json.value("material", "silicon");
        spec.region = json.value("region", "substrate");
        spec.name = json.value("name", "Layer");
        spec.dimensions = json.value("dimensions", std::vector<double>{1e-3, 1e-3, 1e-3});
        spec.position = json.value("position", std::vector<double>{0, 0, 0});
        return spec;
    }
};

/**
 * @brief Thin wrapper around your existing classes
 * 
 * This class does NOT replace your existing implementation. It just
 * provides a command interface and simple data extraction for Three.js.
 * Your VTK export and all existing functionality remains unchanged.
 */
class GeometryEngine {
private:
    std::unique_ptr<SemiconductorDevice> m_device;  // Your existing class
    std::string m_deviceName;
    
    // Helper methods (use your existing classes)
    std::unique_ptr<DeviceLayer> createLayerFromSpec(const LayerSpec& spec);
    MaterialProperties getMaterialProperties(const std::string& materialName);
    DeviceRegion getDeviceRegion(const std::string& regionName);
    TopoDS_Solid createGeometry(const LayerSpec& spec);
    
public:
    explicit GeometryEngine(const std::string& deviceName = "Device");
    ~GeometryEngine() = default;
    
    // Command interface (wraps your existing methods)
    CommandResult addLayer(const LayerSpec& spec);
    CommandResult removeLayer(const std::string& layerName);
    CommandResult generateMesh(double meshSize = 1e-6);
    CommandResult refineMesh(const std::vector<double>& refinementPoints, double localSize);
    CommandResult performBooleanOperation(const std::string& operation, 
                                        const std::string& layer1, 
                                        const std::string& layer2);
    CommandResult validateDevice();
    
    // JSON command interface (for API)
    CommandResult executeCommand(const nlohmann::json& command);
    
    // VTK export (uses your existing VTKExporter - unchanged)
    std::string exportCurrentVTK() const;
    void exportVTKToFile(const std::string& filename) const;
    void exportSTEPToFile(const std::string& filename) const;
    void exportAllFormats(const std::string& basePath) const;
    
    // Simple data extraction for Three.js (not VTK conversion)
    GeometryDelta getGeometryDelta() const;
    nlohmann::json getDeviceInfo() const;
    nlohmann::json getMeshStatistics() const;
    
    // Direct access to your existing classes (if needed)
    SemiconductorDevice* getDevice() { return m_device.get(); }
    const SemiconductorDevice* getDevice() const { return m_device.get(); }
};

/**
 * @brief Example implementation of key methods
 */

// Constructor - uses your existing SemiconductorDevice
inline GeometryEngine::GeometryEngine(const std::string& deviceName) 
    : m_deviceName(deviceName) {
    m_device = std::make_unique<SemiconductorDevice>(deviceName);
}

// Add layer command - uses your existing methods
inline CommandResult GeometryEngine::addLayer(const LayerSpec& spec) {
    try {
        // Use your existing classes
        auto layer = createLayerFromSpec(spec);
        m_device->addLayer(std::move(layer));        // Your existing method
        m_device->buildDeviceGeometry();            // Your existing method
        
        return CommandResult{
            .success = true,
            .geometryChanged = true,
            .message = "Layer '" + spec.name + "' added successfully",
            .vtkData = exportCurrentVTK(),           // Always available for ParaView
            .basicStats = getDeviceInfo()            // Simple info for UI
        };
    } catch (const std::exception& e) {
        return CommandResult{
            .success = false,
            .message = "Failed to add layer: " + std::string(e.what())
        };
    }
}

// Generate mesh - uses your existing BoundaryMesh
inline CommandResult GeometryEngine::generateMesh(double meshSize) {
    try {
        m_device->generateGlobalBoundaryMesh(meshSize);  // Your existing method
        
        return CommandResult{
            .success = true,
            .meshChanged = true,
            .message = "Mesh generated with size " + std::to_string(meshSize),
            .vtkData = exportCurrentVTK(),                // ParaView export
            .basicStats = getMeshStatistics()             // For Three.js UI
        };
    } catch (const std::exception& e) {
        return CommandResult{
            .success = false,
            .message = "Mesh generation failed: " + std::string(e.what())
        };
    }
}

// VTK export - uses your existing VTKExporter (completely unchanged)
inline std::string GeometryEngine::exportCurrentVTK() const {
    if (!m_device) return "";
    
    // Use your existing VTK export capability
    std::string tempFile = "/tmp/device_" + std::to_string(rand()) + ".vtk";
    
    // Your existing method works exactly as before
    bool success = m_device->exportMesh(tempFile, "VTK");
    
    if (success) {
        // Read file content and return as string
        std::ifstream file(tempFile);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        std::remove(tempFile.c_str());  // Clean up temp file
        return content;
    }
    
    return "";
}

// Simple data extraction for Three.js (not complex VTK processing)
inline GeometryDelta GeometryEngine::getGeometryDelta() const {
    GeometryDelta delta;
    
    if (!m_device) return delta;
    
    const BoundaryMesh* mesh = m_device->getGlobalMesh();
    if (!mesh) return delta;
    
    // Simple extraction from your existing BoundaryMesh
    const auto& nodes = mesh->getNodes();
    const auto& elements = mesh->getElements();
    
    // Extract vertices (Three.js can handle this format efficiently)
    delta.vertices.reserve(nodes.size());
    for (const auto& node : nodes) {
        delta.vertices.push_back({
            static_cast<float>(node->point.X()),
            static_cast<float>(node->point.Y()),
            static_cast<float>(node->point.Z())
        });
    }
    
    // Extract triangle indices and materials
    delta.indices.reserve(elements.size() * 3);
    delta.materialIds.reserve(elements.size());
    
    for (const auto& element : elements) {
        // Triangle indices for Three.js BufferGeometry
        delta.indices.insert(delta.indices.end(), {
            static_cast<uint32_t>(element->nodeIds[0]),
            static_cast<uint32_t>(element->nodeIds[1]),
            static_cast<uint32_t>(element->nodeIds[2])
        });
        
        // Material ID (Three.js will handle material assignment)
        delta.materialIds.push_back(element->faceId);  // Use your existing face ID
    }
    
    // Extract material names from your device layers
    const auto& layers = m_device->getLayers();
    for (const auto& layer : layers) {
        delta.materialNames.push_back(layer->getMaterial().name);
    }
    
    return delta;  // Three.js handles the rest!
}

/**
 * @brief Helper to create layer from specification using your existing classes
 */
inline std::unique_ptr<DeviceLayer> GeometryEngine::createLayerFromSpec(const LayerSpec& spec) {
    // Use your existing GeometryBuilder methods
    TopoDS_Solid solid = createGeometry(spec);
    MaterialProperties material = getMaterialProperties(spec.material);
    DeviceRegion region = getDeviceRegion(spec.region);
    
    // Create layer using your existing DeviceLayer constructor
    return std::make_unique<DeviceLayer>(solid, material, region, spec.name);
}

/**
 * @brief Create geometry using your existing GeometryBuilder
 */
inline TopoDS_Solid GeometryEngine::createGeometry(const LayerSpec& spec) {
    gp_Pnt position(
        spec.position.size() > 0 ? spec.position[0] : 0.0,
        spec.position.size() > 1 ? spec.position[1] : 0.0,
        spec.position.size() > 2 ? spec.position[2] : 0.0
    );
    
    if (spec.geometry == "box" && spec.dimensions.size() >= 3) {
        Dimensions3D dims(spec.dimensions[0], spec.dimensions[1], spec.dimensions[2]);
        return GeometryBuilder::createBox(position, dims);  // Your existing method
    } else if (spec.geometry == "cylinder" && spec.dimensions.size() >= 2) {
        return GeometryBuilder::createCylinder(position, gp_Vec(0, 0, 1),
                                             spec.dimensions[0], spec.dimensions[1]);
    } else if (spec.geometry == "sphere" && spec.dimensions.size() >= 1) {
        return GeometryBuilder::createSphere(position, spec.dimensions[0]);
    }
    
    // Default to box if specification is invalid
    return GeometryBuilder::createBox(position, Dimensions3D(1e-3, 1e-3, 1e-3));
}

#endif // GEOMETRY_ENGINE_H
