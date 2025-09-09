#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "VTKExporter.h"
#include "BoundaryMesh.h"

#include <iostream>
#include <algorithm>
#include <stdexcept>

// OpenCASCADE includes
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>

// MaterialProperties implementation
MaterialProperties::MaterialProperties(MaterialType t, double cond, double perm, 
                                      double gap, const std::string& n)
    : type(t), conductivity(cond), permittivity(perm), bandGap(gap), name(n) {
}

// DeviceLayer implementation
DeviceLayer::DeviceLayer(const TopoDS_Solid& solid, const MaterialProperties& material,
                        DeviceRegion region, const std::string& name)
    : m_solid(solid), m_material(material), m_region(region), m_name(name) {
}

void DeviceLayer::generateBoundaryMesh(double meshSize) {
    try {
        m_boundaryMesh = std::make_unique<BoundaryMesh>(m_solid, meshSize);
        m_boundaryMesh->generate();
    } catch (const std::exception& e) {
        std::cerr << "Error generating boundary mesh for layer " << m_name 
                  << ": " << e.what() << std::endl;
        throw;
    }
}

void DeviceLayer::refineBoundaryMesh(const std::vector<gp_Pnt>& refinementPoints, double localSize) {
    if (!m_boundaryMesh) {
        throw std::runtime_error("Boundary mesh not generated for layer " + m_name);
    }
    
    m_boundaryMesh->refine(refinementPoints, localSize);
}

double DeviceLayer::getVolume() const {
    return GeometryBuilder::calculateVolume(m_solid);
}

gp_Pnt DeviceLayer::getCentroid() const {
    return GeometryBuilder::calculateCentroid(m_solid);
}

std::vector<TopoDS_Face> DeviceLayer::getBoundaryFaces() const {
    return GeometryBuilder::extractFaces(m_solid);
}

// SemiconductorDevice implementation
SemiconductorDevice::SemiconductorDevice(const std::string& name) 
    : m_deviceName(name), m_characteristicLength(1.0) {
}

void SemiconductorDevice::addLayer(std::unique_ptr<DeviceLayer> layer) {
    if (!layer) {
        throw std::invalid_argument("Cannot add null layer");
    }
    
    // Check for duplicate names
    for (const auto& existingLayer : m_layers) {
        if (existingLayer->getName() == layer->getName()) {
            throw std::invalid_argument("Layer with name '" + layer->getName() + "' already exists");
        }
    }
    
    m_layers.push_back(std::move(layer));
}

void SemiconductorDevice::removeLayer(const std::string& layerName) {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerName](const std::unique_ptr<DeviceLayer>& layer) {
            return layer->getName() == layerName;
        });
    
    if (it != m_layers.end()) {
        m_layers.erase(it);
    } else {
        throw std::invalid_argument("Layer '" + layerName + "' not found");
    }
}

DeviceLayer* SemiconductorDevice::getLayer(const std::string& layerName) {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerName](const std::unique_ptr<DeviceLayer>& layer) {
            return layer->getName() == layerName;
        });
    
    return (it != m_layers.end()) ? it->get() : nullptr;
}

const DeviceLayer* SemiconductorDevice::getLayer(const std::string& layerName) const {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
        [&layerName](const std::unique_ptr<DeviceLayer>& layer) {
            return layer->getName() == layerName;
        });
    
    return (it != m_layers.end()) ? it->get() : nullptr;
}

void SemiconductorDevice::buildDeviceGeometry() {
    if (m_layers.empty()) {
        throw std::runtime_error("No layers defined for device");
    }
    
    try {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        
        for (const auto& layer : m_layers) {
            builder.Add(compound, layer->getSolid());
        }
        
        m_deviceShape = compound;
        
    } catch (const std::exception& e) {
        std::cerr << "Error building device geometry: " << e.what() << std::endl;
        throw;
    }
}

void SemiconductorDevice::generateGlobalBoundaryMesh(double meshSize) {
    if (m_deviceShape.IsNull()) {
        buildDeviceGeometry();
    }
    
    try {
        m_globalMesh = std::make_unique<BoundaryMesh>(m_deviceShape, meshSize);
        m_globalMesh->generate();
    } catch (const std::exception& e) {
        std::cerr << "Error generating global boundary mesh: " << e.what() << std::endl;
        throw;
    }
}

void SemiconductorDevice::refineGlobalMesh(const std::vector<gp_Pnt>& refinementPoints, double localSize) {
    if (!m_globalMesh) {
        throw std::runtime_error("Global mesh not generated");
    }
    
    m_globalMesh->refine(refinementPoints, localSize);
}

void SemiconductorDevice::exportGeometry(const std::string& filename, const std::string& format) const {
    if (m_deviceShape.IsNull()) {
        throw std::runtime_error("Device geometry not built");
    }
    
    bool success = false;
    std::string upperFormat = format;
    std::transform(upperFormat.begin(), upperFormat.end(), upperFormat.begin(), ::toupper);
    
    if (upperFormat == "STEP") {
        success = GeometryBuilder::exportSTEP(m_deviceShape, filename);
    } else if (upperFormat == "IGES") {
        success = GeometryBuilder::exportIGES(m_deviceShape, filename);
    } else if (upperFormat == "STL") {
        success = GeometryBuilder::exportSTL(m_deviceShape, filename);
    } else if (upperFormat == "BREP") {
        success = GeometryBuilder::exportBREP(m_deviceShape, filename);
    } else {
        throw std::invalid_argument("Unsupported export format: " + format);
    }
    
    if (!success) {
        throw std::runtime_error("Failed to export geometry to " + filename);
    }
}

void SemiconductorDevice::exportMesh(const std::string& filename, const std::string& format) const {
    if (!m_globalMesh) {
        throw std::runtime_error("Global mesh not generated");
    }
    
    std::string upperFormat = format;
    std::transform(upperFormat.begin(), upperFormat.end(), upperFormat.begin(), ::toupper);
    
    if (upperFormat == "VTK") {
        if (!VTKExporter::exportMesh(*m_globalMesh, filename)) {
            throw std::runtime_error("Failed to export mesh to VTK file: " + filename);
        }
    } else if (upperFormat == "STL") {
        m_globalMesh->exportToSTL(filename);
    } else if (upperFormat == "GMSH") {
        m_globalMesh->exportToGMSH(filename);
    } else if (upperFormat == "OBJ") {
        m_globalMesh->exportToOBJ(filename);
    } else {
        throw std::invalid_argument("Unsupported mesh export format: " + format);
    }
}

void SemiconductorDevice::exportMeshWithRegions(const std::string& filename, const std::string& format) const {
    std::string upperFormat = format;
    std::transform(upperFormat.begin(), upperFormat.end(), upperFormat.begin(), ::toupper);
    
    if (upperFormat != "VTK") {
        throw std::invalid_argument("Region export currently only supported for VTK format");
    }
    
    if (!VTKExporter::exportDeviceWithRegions(*this, filename)) {
        throw std::runtime_error("Failed to export device mesh with regions to " + filename);
    }
}

std::vector<DeviceLayer*> SemiconductorDevice::getLayersByRegion(DeviceRegion region) {
    std::vector<DeviceLayer*> result;
    
    for (const auto& layer : m_layers) {
        if (layer->getRegion() == region) {
            result.push_back(layer.get());
        }
    }
    
    return result;
}

std::vector<DeviceLayer*> SemiconductorDevice::getLayersByMaterial(MaterialType material) {
    std::vector<DeviceLayer*> result;
    
    for (const auto& layer : m_layers) {
        if (layer->getMaterial().type == material) {
            result.push_back(layer.get());
        }
    }
    
    return result;
}

bool SemiconductorDevice::validateGeometry() const {
    if (m_deviceShape.IsNull()) {
        return false;
    }
    
    return GeometryBuilder::isValidShape(m_deviceShape);
}

bool SemiconductorDevice::validateMesh() const {
    if (!m_globalMesh) {
        return false;
    }
    
    return m_globalMesh->validateMesh();
}

void SemiconductorDevice::printDeviceInfo() const {
    std::cout << "=== Semiconductor Device Info ===" << std::endl;
    std::cout << "Device Name: " << m_deviceName << std::endl;
    std::cout << "Number of Layers: " << m_layers.size() << std::endl;
    std::cout << "Characteristic Length: " << m_characteristicLength << " m" << std::endl;
    
    if (!m_deviceShape.IsNull()) {
        double volume = getTotalVolume();
        std::cout << "Total Volume: " << volume << " m³" << std::endl;
        
        auto bbox = GeometryBuilder::getBoundingBox(m_deviceShape);
        std::cout << "Bounding Box: [" 
                  << bbox.first.X() << ", " << bbox.first.Y() << ", " << bbox.first.Z() << "] to ["
                  << bbox.second.X() << ", " << bbox.second.Y() << ", " << bbox.second.Z() << "]" 
                  << std::endl;
    }
    
    if (m_globalMesh) {
        std::cout << "Global Mesh:" << std::endl;
        std::cout << "  Nodes: " << m_globalMesh->getNodeCount() << std::endl;
        std::cout << "  Elements: " << m_globalMesh->getElementCount() << std::endl;
        std::cout << "  Mesh Size: " << m_globalMesh->getMeshSize() << std::endl;
    }
    
    std::cout << "\n=== Layers ===" << std::endl;
    for (const auto& layer : m_layers) {
        std::cout << "Layer: " << layer->getName() 
                  << " (Material: " << layer->getMaterial().name
                  << ", Region: " << static_cast<int>(layer->getRegion())
                  << ", Volume: " << layer->getVolume() << " m³)" << std::endl;
        
        if (layer->getBoundaryMesh()) {
            std::cout << "  Mesh: " << layer->getBoundaryMesh()->getNodeCount() 
                      << " nodes, " << layer->getBoundaryMesh()->getElementCount() 
                      << " elements" << std::endl;
        }
    }
    std::cout << "===============================" << std::endl;
}

double SemiconductorDevice::getTotalVolume() const {
    if (m_deviceShape.IsNull()) {
        return 0.0;
    }
    
    return GeometryBuilder::calculateVolume(m_deviceShape);
}

std::map<MaterialType, double> SemiconductorDevice::getVolumesByMaterial() const {
    std::map<MaterialType, double> volumes;
    
    for (const auto& layer : m_layers) {
        MaterialType material = layer->getMaterial().type;
        double volume = layer->getVolume();
        
        if (volumes.find(material) != volumes.end()) {
            volumes[material] += volume;
        } else {
            volumes[material] = volume;
        }
    }
    
    return volumes;
}

// Utility functions for export
int SemiconductorDevice::getMaterialTypeId(MaterialType type) {
    return static_cast<int>(type);
}

int SemiconductorDevice::getDeviceRegionId(DeviceRegion region) {
    return static_cast<int>(region);
}

std::string SemiconductorDevice::getMaterialTypeName(MaterialType type) {
    switch(type) {
        case MaterialType::Silicon: return "Silicon";
        case MaterialType::GermaniumSilicon: return "GermaniumSilicon";
        case MaterialType::GalliumArsenide: return "GalliumArsenide";
        case MaterialType::IndiumGalliumArsenide: return "IndiumGalliumArsenide";
        case MaterialType::Silicon_Nitride: return "Silicon_Nitride";
        case MaterialType::Silicon_Dioxide: return "Silicon_Dioxide";
        case MaterialType::Metal_Contact: return "Metal_Contact";
        default: return "Unknown";
    }
}

std::string SemiconductorDevice::getDeviceRegionName(DeviceRegion region) {
    switch(region) {
        case DeviceRegion::Substrate: return "Substrate";
        case DeviceRegion::ActiveRegion: return "ActiveRegion";
        case DeviceRegion::Gate: return "Gate";
        case DeviceRegion::Source: return "Source";
        case DeviceRegion::Drain: return "Drain";
        case DeviceRegion::Insulator: return "Insulator";
        case DeviceRegion::Contact: return "Contact";
        default: return "Unknown";
    }
}

// Material factory methods
MaterialProperties SemiconductorDevice::createStandardSilicon() {
    return MaterialProperties(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon Substrate");
}

MaterialProperties SemiconductorDevice::createStandardSiliconDioxide() {
    return MaterialProperties(MaterialType::Silicon_Dioxide, 1.0e-16, 3.9 * 8.854e-12, 9.0, "SiO2 Gate Oxide");
}

MaterialProperties SemiconductorDevice::createStandardPolysilicon() {
    return MaterialProperties(MaterialType::Metal_Contact, 1.0e5, 1.0 * 8.854e-12, 0.0, "Polysilicon Gate");
}

MaterialProperties SemiconductorDevice::createStandardMetal() {
    return MaterialProperties(MaterialType::Metal_Contact, 1.0e7, 1.0 * 8.854e-12, 0.0, "Metal Contact");
}

// Device template methods
void SemiconductorDevice::createSimpleMOSFET(double length, double width, double substrateHeight,
                                            double oxideHeight, double gateHeight) {
    // Clear existing layers
    m_layers.clear();
    
    // Create standard materials
    auto silicon = createStandardSilicon();
    auto oxide = createStandardSiliconDioxide();
    auto polysilicon = createStandardPolysilicon();
    
    // 1. Substrate layer (base of the device)
    TopoDS_Solid substrate = GeometryBuilder::createBox(
        gp_Pnt(0, 0, 0),
        Dimensions3D(length, width, substrateHeight)
    );
    
    auto substrateLayer = std::make_unique<DeviceLayer>(
        substrate, silicon, DeviceRegion::Substrate, "Substrate"
    );
    addLayer(std::move(substrateLayer));
    
    // 2. Gate oxide layer (center portion on top of substrate)
    TopoDS_Solid oxideBox = GeometryBuilder::createBox(
        gp_Pnt(length*0.25, width*0.25, substrateHeight),
        Dimensions3D(length*0.5, width*0.5, oxideHeight)
    );
    
    auto oxideLayer = std::make_unique<DeviceLayer>(
        oxideBox, oxide, DeviceRegion::Insulator, "Gate_Oxide"
    );
    addLayer(std::move(oxideLayer));
    
    // 3. Gate contact (smaller box on top of oxide)
    TopoDS_Solid gateBox = GeometryBuilder::createBox(
        gp_Pnt(length*0.3, width*0.3, substrateHeight + oxideHeight),
        Dimensions3D(length*0.4, width*0.4, gateHeight)
    );
    
    auto gateLayer = std::make_unique<DeviceLayer>(
        gateBox, polysilicon, DeviceRegion::Gate, "Gate"
    );
    addLayer(std::move(gateLayer));
    
    // Build the device geometry
    buildDeviceGeometry();
}

void SemiconductorDevice::generateAllLayerMeshes(double substrateMeshSize, double oxideMeshSize, double gateMeshSize) {
    DeviceLayer* substrate = getLayer("Substrate");
    if (substrate) {
        substrate->generateBoundaryMesh(substrateMeshSize);
    }
    
    DeviceLayer* oxide = getLayer("Gate_Oxide");
    if (oxide) {
        oxide->generateBoundaryMesh(oxideMeshSize);
    }
    
    DeviceLayer* gate = getLayer("Gate");
    if (gate) {
        gate->generateBoundaryMesh(gateMeshSize);
    }
}

void SemiconductorDevice::generateAllLayerMeshes() {
    // Use reasonable default mesh sizes based on device dimensions
    auto bbox = GeometryBuilder::getBoundingBox(m_deviceShape);
    double deviceSize = std::max({
        bbox.second.X() - bbox.first.X(),
        bbox.second.Y() - bbox.first.Y(),
        bbox.second.Z() - bbox.first.Z()
    });
    
    double substrateMeshSize = deviceSize / 5.0;   // Coarse mesh
    double oxideMeshSize = deviceSize / 20.0;      // Fine mesh
    double gateMeshSize = deviceSize / 12.0;       // Medium mesh
    
    generateAllLayerMeshes(substrateMeshSize, oxideMeshSize, gateMeshSize);
}

// Validation and export workflow
SemiconductorDevice::ValidationResult SemiconductorDevice::validateDevice() const {
    ValidationResult result;
    
    result.geometryValid = validateGeometry();
    result.geometryMessage = result.geometryValid ? 
        "✓ Device geometry is valid" : "✗ Device geometry is invalid";
    
    result.meshValid = validateMesh();
    result.meshMessage = result.meshValid ? 
        "✓ Device mesh is valid" : "✗ Device mesh is invalid";
    
    return result;
}

void SemiconductorDevice::exportDeviceComplete(const std::string& baseName, bool includeRegions) const {
    // Export geometry
    exportGeometry(baseName + ".step", "STEP");
    
    // Export traditional mesh
    exportMesh(baseName + "_traditional.vtk", "VTK");
    
    // Export enhanced mesh with regions if requested
    if (includeRegions) {
        exportMeshWithRegions(baseName + "_with_regions.vtk", "VTK");
    }
    
    // Print summary
    std::cout << "\nExported device files:" << std::endl;
    std::cout << "  • " << baseName << ".step - 3D geometry (STEP format)" << std::endl;
    std::cout << "  • " << baseName << "_traditional.vtk - Traditional mesh" << std::endl;
    if (includeRegions) {
        std::cout << "  • " << baseName << "_with_regions.vtk - Enhanced mesh with region data" << std::endl;
    }
}
