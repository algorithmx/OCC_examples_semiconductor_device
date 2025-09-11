#ifndef SEMICONDUCTOR_DEVICE_H
#define SEMICONDUCTOR_DEVICE_H

#include <memory>
#include <vector>
#include <string>
#include <map>

// OpenCASCADE includes
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <TopTools_ListOfShape.hxx>

// Forward declarations
class GeometryBuilder;
class BoundaryMesh;

/**
 * @brief Enum representing different semiconductor materials
 */
enum class MaterialType {
    Silicon,
    GermaniumSilicon,
    GalliumArsenide,
    IndiumGalliumArsenide,
    Silicon_Nitride,
    Silicon_Dioxide,
    Metal_Contact
};

/**
 * @brief Structure representing material properties
 */
struct MaterialProperties {
    MaterialType type;
    double conductivity;        // S/m
    double permittivity;       // F/m
    double bandGap;           // eV
    std::string name;
    
    MaterialProperties(MaterialType t = MaterialType::Silicon, 
                      double cond = 1.0e-4, 
                      double perm = 11.7 * 8.854e-12, 
                      double gap = 1.12,
                      const std::string& n = "Silicon");
};

/**
 * @brief Enum for different device regions
 */
enum class DeviceRegion {
    Substrate,
    ActiveRegion,
    Gate,
    Source,
    Drain,
    Insulator,
    Contact
};

/**
 * @brief Class representing a region within the semiconductor device
 */
class DeviceLayer {
private:
    TopoDS_Solid m_solid;
    MaterialProperties m_material;
    DeviceRegion m_region;
    std::string m_name;
    std::unique_ptr<BoundaryMesh> m_boundaryMesh;

public:
    DeviceLayer(const TopoDS_Solid& solid, 
                const MaterialProperties& material,
                DeviceRegion region,
                const std::string& name);
    ~DeviceLayer();
    
    // Getters
    const TopoDS_Solid& getSolid() const { return m_solid; }
    const MaterialProperties& getMaterial() const { return m_material; }
    DeviceRegion getRegion() const { return m_region; }
    const std::string& getName() const { return m_name; }
    const BoundaryMesh* getBoundaryMesh() const { return m_boundaryMesh.get(); }
    
    // Setters
    void setMaterial(const MaterialProperties& material) { m_material = material; }
    void setName(const std::string& name) { m_name = name; }
    
    // Mesh operations
    void generateBoundaryMesh(double meshSize = 0.1);
    void refineBoundaryMesh(const std::vector<gp_Pnt>& refinementPoints, double localSize);
    
    // Geometric operations
    double getVolume() const;
    gp_Pnt getCentroid() const;
    std::vector<TopoDS_Face> getBoundaryFaces() const;
};

/**
 * @brief Main class representing a complete semiconductor device
 */
class SemiconductorDevice {
private:
    std::vector<std::unique_ptr<DeviceLayer>> m_layers;
    std::string m_deviceName;
    double m_characteristicLength;
    
    // Overall device geometry
    TopoDS_Shape m_deviceShape;
    
    // Mesh management
    std::unique_ptr<BoundaryMesh> m_globalMesh;

public:
    explicit SemiconductorDevice(const std::string& name = "SemiconductorDevice");
    ~SemiconductorDevice();
    // Movable but not copyable (unique_ptr members)
    SemiconductorDevice(SemiconductorDevice&&) noexcept;
    SemiconductorDevice& operator=(SemiconductorDevice&&) noexcept;
    
    // Layer management
    void addLayer(std::unique_ptr<DeviceLayer> layer);
    void removeLayer(const std::string& layerName);
    DeviceLayer* getLayer(const std::string& layerName);
    const DeviceLayer* getLayer(const std::string& layerName) const;
    size_t getLayerCount() const { return m_layers.size(); }
    const std::vector<std::unique_ptr<DeviceLayer>>& getLayers() const { return m_layers; }
    
    // Device properties
    const std::string& getName() const { return m_deviceName; }
    void setName(const std::string& name) { m_deviceName = name; }
    
    double getCharacteristicLength() const { return m_characteristicLength; }
    void setCharacteristicLength(double length) { m_characteristicLength = length; }
    
    // Geometry operations
    void buildDeviceGeometry();
    const TopoDS_Shape& getDeviceShape() const { return m_deviceShape; }
    
    // Mesh operations
    void generateGlobalBoundaryMesh(double meshSize = 0.1);
    void refineGlobalMesh(const std::vector<gp_Pnt>& refinementPoints, double localSize);
    const BoundaryMesh* getGlobalMesh() const { return m_globalMesh.get(); }
    
    // Analysis and export
    void exportGeometry(const std::string& filename, const std::string& format = "STEP") const;
    void exportMesh(const std::string& filename, const std::string& format = "VTK") const;
    void exportMeshWithRegions(const std::string& filename, const std::string& format = "VTK") const;
    
    // Utility functions
    std::vector<DeviceLayer*> getLayersByRegion(DeviceRegion region);
    std::vector<DeviceLayer*> getLayersByMaterial(MaterialType material);
    
    // Device validation
    bool validateGeometry() const;
    bool validateMesh() const;
    
    // Statistics
    void printDeviceInfo() const;
    double getTotalVolume() const;
    std::map<MaterialType, double> getVolumesByMaterial() const;
    
    // Utility functions for export
    static int getMaterialTypeId(MaterialType type);
    static int getDeviceRegionId(DeviceRegion region);
    static std::string getMaterialTypeName(MaterialType type);
    static std::string getDeviceRegionName(DeviceRegion region);
    
    // Material factory methods
    static MaterialProperties createStandardSilicon();
    static MaterialProperties createStandardSiliconDioxide();
    static MaterialProperties createStandardPolysilicon();
    static MaterialProperties createStandardMetal();
    
    // Device template methods
    void createSimpleMOSFET(double length, double width, double substrateHeight,
                           double oxideHeight, double gateHeight);
    void generateAllLayerMeshes(double substrateMeshSize, double oxideMeshSize, double gateMeshSize);
    void generateAllLayerMeshes(); // With default mesh sizes
    
    // Validation and export workflow
    struct ValidationResult {
        bool geometryValid;
        bool meshValid;
        std::string geometryMessage;
        std::string meshMessage;
    };
    
    ValidationResult validateDevice() const;
    void exportDeviceComplete(const std::string& baseName, bool includeRegions = true) const;
};

#endif // SEMICONDUCTOR_DEVICE_H
