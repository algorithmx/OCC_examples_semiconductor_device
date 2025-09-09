#include "VTKExporter.h"
#include "BoundaryMesh.h"
#include "SemiconductorDevice.h"

#include <iostream>
#include <cmath>
#include <algorithm>

bool VTKExporter::exportMesh(const BoundaryMesh& mesh, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return false;
    }
    
    writeVTKHeader(file, "Boundary Mesh");
    writeVTKPoints(file, mesh);
    writeVTKCells(file, mesh);
    
    // Cell types (5 = triangle)
    file << "CELL_TYPES " << mesh.getElementCount() << std::endl;
    for (size_t i = 0; i < mesh.getElementCount(); i++) {
        file << "5" << std::endl;
    }
    
    // Ensure file ends properly
    file << std::endl;
    file.close();
    std::cout << "Exported mesh to VTK file: " << filename << std::endl;
    return true;
}

bool VTKExporter::exportMeshWithCustomData(const BoundaryMesh& mesh, 
                                          const std::string& filename,
                                          const std::vector<int>& materialIds, 
                                          const std::vector<int>& regionIds, 
                                          const std::vector<std::string>& /* layerNames */) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return false;
    }
    
    writeVTKHeader(file, "Semiconductor Device Boundary Mesh with Custom Regions");
    writeVTKPoints(file, mesh);
    writeVTKCells(file, mesh);
    
    // Cell types (5 = triangle)
    file << "CELL_TYPES " << mesh.getElementCount() << std::endl;
    for (size_t i = 0; i < mesh.getElementCount(); i++) {
        file << "5" << std::endl;
    }
    
    // Cell data - this is where we add region information
    file << "CELL_DATA " << mesh.getElementCount() << std::endl;
    
    // Material ID data
    if (!materialIds.empty() && materialIds.size() >= mesh.getElementCount()) {
        file << "SCALARS MaterialID int 1" << std::endl;
        file << "LOOKUP_TABLE default" << std::endl;
        for (size_t i = 0; i < mesh.getElementCount(); i++) {
            file << materialIds[i] << std::endl;
        }
        file << std::endl;
    }
    
    // Region ID data
    if (!regionIds.empty() && regionIds.size() >= mesh.getElementCount()) {
        file << "SCALARS RegionID int 1" << std::endl;
        file << "LOOKUP_TABLE default" << std::endl;
        for (size_t i = 0; i < mesh.getElementCount(); i++) {
            file << regionIds[i] << std::endl;
        }
        file << std::endl;
    }
    
    // Face ID data (existing functionality)
    file << "SCALARS FaceID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    const auto& elements = mesh.getElements();
    for (const auto& element : elements) {
        file << element->faceId << std::endl;
    }
    file << std::endl;
    
    // Element quality data
    file << "SCALARS ElementQuality float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : elements) {
        double quality = mesh.calculateElementQuality(*element);
        file << quality << std::endl;
    }
    file << std::endl;
    
    // Element area data
    file << "SCALARS ElementArea float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : elements) {
        file << element->area << std::endl;
    }
    
    file.close();
    std::cout << "Exported mesh with custom region data to VTK file: " << filename << std::endl;
    return true;
}

bool VTKExporter::exportMeshWithRegions(const BoundaryMesh& mesh,
                                       const DeviceLayer& layer,
                                       int layerIndex,
                                       const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return false;
    }
    
    writeVTKHeader(file, "Semiconductor Device Boundary Mesh with Regions");
    writeVTKPoints(file, mesh);
    writeVTKCells(file, mesh);
    
    // Cell types (5 = triangle)
    file << "CELL_TYPES " << mesh.getElementCount() << std::endl;
    for (size_t i = 0; i < mesh.getElementCount(); i++) {
        file << "5" << std::endl;
    }
    
    // Cell data with region information
    writeVTKCellData(file, mesh, layer, layerIndex);
    
    file.close();
    std::cout << "Exported mesh with region data to VTK file: " << filename << std::endl;
    return true;
}

bool VTKExporter::exportDeviceWithRegions(const SemiconductorDevice& device, 
                                         const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Collect all layer meshes and calculate total nodes/elements
    std::vector<const BoundaryMesh*> layerMeshes;
    std::vector<const DeviceLayer*> meshLayers;
    size_t totalNodes = 0;
    size_t totalElements = 0;
    
    const auto& layers = device.getLayers();
    for (const auto& layer : layers) {
        const BoundaryMesh* mesh = layer->getBoundaryMesh();
        if (mesh && mesh->getNodeCount() > 0 && mesh->getElementCount() > 0) {
            layerMeshes.push_back(mesh);
            meshLayers.push_back(layer.get());
            totalNodes += mesh->getNodeCount();
            totalElements += mesh->getElementCount();
        }
    }
    
    if (layerMeshes.empty()) {
        std::cerr << "No layer meshes available for export" << std::endl;
        return false;
    }
    
    // Write VTK header
    writeVTKHeader(file, "Semiconductor Device Mesh");
    
    // Export all points
    file << "POINTS " << totalNodes << " float" << std::endl;
    for (const auto* mesh : layerMeshes) {
        const auto& nodes = mesh->getNodes();
        for (const auto& node : nodes) {
            file << node->point.X() << " " << node->point.Y() << " " << node->point.Z() << std::endl;
        }
    }
    
    // Export all cells with adjusted node indices
    file << "CELLS " << totalElements << " " << (totalElements * 4) << std::endl;
    size_t nodeOffset = 0;
    for (const auto* mesh : layerMeshes) {
        const auto& elements = mesh->getElements();
        for (const auto& element : elements) {
            file << "3 " << (element->nodeIds[0] + nodeOffset) << " "
                 << (element->nodeIds[1] + nodeOffset) << " "
                 << (element->nodeIds[2] + nodeOffset) << std::endl;
        }
        nodeOffset += mesh->getNodeCount();
    }
    
    // Cell types (all triangles)
    file << "CELL_TYPES " << totalElements << std::endl;
    for (size_t i = 0; i < totalElements; i++) {
        file << "5" << std::endl;
    }
    
    // Cell data with region information
    file << "CELL_DATA " << totalElements << std::endl;
    
    // Material ID data
    file << "SCALARS MaterialID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (size_t layerIdx = 0; layerIdx < meshLayers.size(); layerIdx++) {
        const DeviceLayer* layer = meshLayers[layerIdx];
        const BoundaryMesh* mesh = layerMeshes[layerIdx];
        int materialId = materialTypeToID(layer->getMaterial().type);
        for (size_t i = 0; i < mesh->getElementCount(); i++) {
            file << materialId << std::endl;
        }
    }
    file << std::endl;
    
    // Region ID data
    file << "SCALARS RegionID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (size_t layerIdx = 0; layerIdx < meshLayers.size(); layerIdx++) {
        const DeviceLayer* layer = meshLayers[layerIdx];
        const BoundaryMesh* mesh = layerMeshes[layerIdx];
        int regionId = deviceRegionToID(layer->getRegion());
        for (size_t i = 0; i < mesh->getElementCount(); i++) {
            file << regionId << std::endl;
        }
    }
    file << std::endl;
    
    // Layer index data
    file << "SCALARS LayerIndex int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (size_t layerIdx = 0; layerIdx < meshLayers.size(); layerIdx++) {
        const BoundaryMesh* mesh = layerMeshes[layerIdx];
        for (size_t i = 0; i < mesh->getElementCount(); i++) {
            file << layerIdx << std::endl;
        }
    }
    file << std::endl;
    
    // Element quality data
    file << "SCALARS ElementQuality float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto* mesh : layerMeshes) {
        const auto& elements = mesh->getElements();
        for (const auto& element : elements) {
            double quality = mesh->calculateElementQuality(*element);
            file << quality << std::endl;
        }
    }
    file << std::endl;
    
    // Element area data
    file << "SCALARS ElementArea float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto* mesh : layerMeshes) {
        const auto& elements = mesh->getElements();
        for (const auto& element : elements) {
            file << element->area << std::endl;
        }
    }
    
    file.close();
    std::cout << "Exported multi-region mesh to VTK file: " << filename << std::endl;
    std::cout << "  Total layers: " << layerMeshes.size() << std::endl;
    std::cout << "  Total nodes: " << totalNodes << std::endl;
    std::cout << "  Total elements: " << totalElements << std::endl;
    
    // Print mapping information for user reference
    std::cout << "\nRegion Data Legend:" << std::endl;
    std::cout << "  Material IDs:" << std::endl;
    for (size_t layerIdx = 0; layerIdx < meshLayers.size(); layerIdx++) {
        const DeviceLayer* layer = meshLayers[layerIdx];
        std::cout << "    " << materialTypeToID(layer->getMaterial().type) 
                  << " = " << layer->getMaterial().name << std::endl;
    }
    std::cout << "  Region IDs:" << std::endl;
    for (size_t layerIdx = 0; layerIdx < meshLayers.size(); layerIdx++) {
        const DeviceLayer* layer = meshLayers[layerIdx];
        std::cout << "    " << deviceRegionToID(layer->getRegion()) 
                  << " = " << deviceRegionToName(layer->getRegion()) 
                  << " (" << layer->getName() << ")" << std::endl;
    }
    
    return true;
}

int VTKExporter::materialTypeToID(MaterialType material) {
    return static_cast<int>(material);
}

int VTKExporter::deviceRegionToID(DeviceRegion region) {
    return static_cast<int>(region);
}

std::string VTKExporter::materialTypeToName(MaterialType material) {
    switch(material) {
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

std::string VTKExporter::deviceRegionToName(DeviceRegion region) {
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

void VTKExporter::writeVTKHeader(std::ofstream& file, const std::string& title) {
    file << "# vtk DataFile Version 3.0" << std::endl;
    file << title << std::endl;
    file << "ASCII" << std::endl;
    file << "DATASET UNSTRUCTURED_GRID" << std::endl;
}

void VTKExporter::writeVTKPoints(std::ofstream& file, const BoundaryMesh& mesh) {
    file << "POINTS " << mesh.getNodeCount() << " float" << std::endl;
    const auto& nodes = mesh.getNodes();
    for (const auto& node : nodes) {
        file << node->point.X() << " " << node->point.Y() << " " << node->point.Z() << std::endl;
    }
}

void VTKExporter::writeVTKCells(std::ofstream& file, const BoundaryMesh& mesh, int pointOffset) {
    file << "CELLS " << mesh.getElementCount() << " " << (mesh.getElementCount() * 4) << std::endl;
    const auto& elements = mesh.getElements();
    for (const auto& element : elements) {
        file << "3 " << (element->nodeIds[0] + pointOffset) << " " 
             << (element->nodeIds[1] + pointOffset) << " " << (element->nodeIds[2] + pointOffset) << std::endl;
    }
}

void VTKExporter::writeVTKCellData(std::ofstream& file, 
                                  const BoundaryMesh& mesh,
                                  const DeviceLayer& layer,
                                  int layerIndex) {
    size_t numElements = mesh.getElementCount();
    
    // Cell data section header
    file << "CELL_DATA " << numElements << std::endl;
    
    // Material ID data
    file << "SCALARS MaterialID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    int materialId = materialTypeToID(layer.getMaterial().type);
    for (size_t i = 0; i < numElements; i++) {
        file << materialId << std::endl;
    }
    file << std::endl;
    
    // Region ID data
    file << "SCALARS RegionID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    int regionId = deviceRegionToID(layer.getRegion());
    for (size_t i = 0; i < numElements; i++) {
        file << regionId << std::endl;
    }
    file << std::endl;
    
    // Layer index data
    file << "SCALARS LayerIndex int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (size_t i = 0; i < numElements; i++) {
        file << layerIndex << std::endl;
    }
    file << std::endl;
    
    // Face ID data (existing functionality)
    file << "SCALARS FaceID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    const auto& elements = mesh.getElements();
    for (const auto& element : elements) {
        file << element->faceId << std::endl;
    }
    file << std::endl;
    
    // Element quality data
    file << "SCALARS ElementQuality float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : elements) {
        double quality = mesh.calculateElementQuality(*element);
        file << quality << std::endl;
    }
    file << std::endl;
    
    // Element area data
    file << "SCALARS ElementArea float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : elements) {
        file << element->area << std::endl;
    }
}

double VTKExporter::calculateTriangleQuality(const std::array<double, 3>& p1,
                                           const std::array<double, 3>& p2,
                                           const std::array<double, 3>& p3) {
    // Calculate side lengths
    double dx12 = p2[0] - p1[0], dy12 = p2[1] - p1[1], dz12 = p2[2] - p1[2];
    double dx23 = p3[0] - p2[0], dy23 = p3[1] - p2[1], dz23 = p3[2] - p2[2];
    double dx31 = p1[0] - p3[0], dy31 = p1[1] - p3[1], dz31 = p1[2] - p3[2];
    
    double a = std::sqrt(dx12*dx12 + dy12*dy12 + dz12*dz12);
    double b = std::sqrt(dx23*dx23 + dy23*dy23 + dz23*dz23);
    double c = std::sqrt(dx31*dx31 + dy31*dy31 + dz31*dz31);
    
    // Calculate quality using the ratio of area to perimeter squared
    double perimeter = a + b + c;
    if (perimeter < 1e-12) return 0.0;
    
    double area = calculateTriangleArea(p1, p2, p3);
    double quality = 4.0 * std::sqrt(3.0) * area / (perimeter * perimeter);
    
    return std::max(0.0, std::min(1.0, quality));
}

double VTKExporter::calculateTriangleArea(const std::array<double, 3>& p1,
                                        const std::array<double, 3>& p2,
                                        const std::array<double, 3>& p3) {
    // Calculate area using cross product
    double v1x = p2[0] - p1[0], v1y = p2[1] - p1[1], v1z = p2[2] - p1[2];
    double v2x = p3[0] - p1[0], v2y = p3[1] - p1[1], v2z = p3[2] - p1[2];
    
    // Cross product
    double nx = v1y * v2z - v1z * v2y;
    double ny = v1z * v2x - v1x * v2z;
    double nz = v1x * v2y - v1y * v2x;
    
    // Magnitude of cross product divided by 2
    return 0.5 * std::sqrt(nx*nx + ny*ny + nz*nz);
}
