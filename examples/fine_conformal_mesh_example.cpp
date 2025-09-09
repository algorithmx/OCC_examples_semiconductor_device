#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>

// OpenCASCADE includes
#include <gp_Pnt.hxx>
#include <Standard_DomainError.hxx>

// Project includes
#include "../include/SemiconductorDevice.h"
#include "../include/GeometryBuilder.h"
#include "../include/BoundaryMesh.h"
#include "../include/VTKExporter.h"

void createFineConformalMeshDevice(SemiconductorDevice& device) {
    std::cout << "Creating device with fine conformal mesh..." << std::endl;
    
    try {
        // Define materials
        auto silicon = SemiconductorDevice::createStandardSilicon();
        auto oxide = SemiconductorDevice::createStandardSiliconDioxide();
        auto polysilicon = SemiconductorDevice::createStandardPolysilicon();
        
        // Device dimensions in millimeters for better numerical stability
        const double length = 2.0e-3;       // 2 mm
        const double width = 1.0e-3;        // 1 mm
        const double substrateHeight = 0.5e-3;   // 0.5 mm
        const double oxideHeight = 0.05e-3;      // 0.05 mm
        const double gateHeight = 0.2e-3;        // 0.2 mm
        
        // 1. Substrate layer (base)
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0),
            Dimensions3D(length, width, substrateHeight)
        );
        
        auto substrateLayer = std::make_unique<DeviceLayer>(
            substrate, silicon, DeviceRegion::Substrate, "Substrate"
        );
        
        // 2. Gate oxide layer (centered on substrate)
        TopoDS_Solid gateOxide = GeometryBuilder::createBox(
            gp_Pnt(length * 0.25, width * 0.25, substrateHeight),
            Dimensions3D(length * 0.5, width * 0.5, oxideHeight)
        );
        
        auto oxideLayer = std::make_unique<DeviceLayer>(
            gateOxide, oxide, DeviceRegion::Insulator, "Gate_Oxide"
        );
        
        // 3. Gate electrode (smaller, on top of oxide)
        TopoDS_Solid gate = GeometryBuilder::createBox(
            gp_Pnt(length * 0.3, width * 0.3, substrateHeight + oxideHeight),
            Dimensions3D(length * 0.4, width * 0.4, gateHeight)
        );
        
        auto gateLayer = std::make_unique<DeviceLayer>(
            gate, polysilicon, DeviceRegion::Gate, "Gate"
        );
        
        // Add layers to device
        device.addLayer(std::move(substrateLayer));
        device.addLayer(std::move(oxideLayer));
        device.addLayer(std::move(gateLayer));
        
        std::cout << "✓ Created device with " << device.getLayerCount() << " layers" << std::endl;
        
    } catch (const Standard_DomainError& e) {
        std::cerr << "OpenCASCADE geometry error: " << e.GetMessageString() << std::endl;
        throw std::runtime_error("Failed to create device geometry");
    } catch (const std::exception& e) {
        std::cerr << "Error creating device: " << e.what() << std::endl;
        throw;
    }
}

void generateFineConformalMesh(SemiconductorDevice& device) {
    std::cout << "\nGenerating fine conformal boundary mesh..." << std::endl;
    
    // Build composite geometry
    device.buildDeviceGeometry();
    
    // Use a much finer mesh size - 10 micrometers instead of 100
    double fineMeshSize = 0.01e-3;  // 10 μm - very fine mesh
    
    std::cout << "Selected fine mesh size: " << fineMeshSize * 1e6 << " μm" << std::endl;
    
    // Generate the global conformal boundary mesh with fine size
    device.generateGlobalBoundaryMesh(fineMeshSize);
    
    // Add refinement points at critical interfaces
    std::vector<gp_Pnt> refinementPoints;
    
    // Interface between substrate and oxide
    refinementPoints.push_back(gp_Pnt(1.0e-3, 0.5e-3, 0.5e-3));
    
    // Interface between oxide and gate
    refinementPoints.push_back(gp_Pnt(1.0e-3, 0.5e-3, 0.55e-3));
    
    // Gate edges
    refinementPoints.push_back(gp_Pnt(0.6e-3, 0.5e-3, 0.65e-3));
    refinementPoints.push_back(gp_Pnt(1.4e-3, 0.5e-3, 0.65e-3));
    
    // Apply additional refinement with even finer mesh
    if (!refinementPoints.empty()) {
        std::cout << "Applying local refinement at " << refinementPoints.size() 
                  << " critical points..." << std::endl;
        device.refineGlobalMesh(refinementPoints, fineMeshSize * 0.5);
    }
    
    std::cout << "✓ Fine conformal mesh generation completed" << std::endl;
}

void exportFineMeshWithDeduplication(const SemiconductorDevice& device, const std::string& filename) {
    std::cout << "\nExporting fine mesh with deduplication..." << std::endl;
    
    const BoundaryMesh* mesh = device.getGlobalMesh();
    if (!mesh) {
        throw std::runtime_error("No global mesh available for export");
    }
    
    const auto& nodes = mesh->getNodes();
    const auto& elements = mesh->getElements();
    
    std::cout << "Original mesh: " << nodes.size() << " nodes, " << elements.size() << " elements" << std::endl;
    
    // Create a map to deduplicate points with tolerance
    std::map<std::string, int> pointMap;
    std::vector<gp_Pnt> uniquePoints;
    std::vector<std::array<int, 3>> validElements;
    
    double tolerance = 1e-9;  // 1 nanometer tolerance
    
    // Helper function to create a key for a point
    auto makePointKey = [tolerance](const gp_Pnt& p) -> std::string {
        long long x = static_cast<long long>(p.X() / tolerance);
        long long y = static_cast<long long>(p.Y() / tolerance);
        long long z = static_cast<long long>(p.Z() / tolerance);
        return std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);
    };
    
    // Deduplicate points
    std::vector<int> nodeMapping(nodes.size());
    for (size_t i = 0; i < nodes.size(); i++) {
        std::string key = makePointKey(nodes[i]->point);
        auto it = pointMap.find(key);
        if (it == pointMap.end()) {
            int newIndex = static_cast<int>(uniquePoints.size());
            pointMap[key] = newIndex;
            uniquePoints.push_back(nodes[i]->point);
            nodeMapping[i] = newIndex;
        } else {
            nodeMapping[i] = it->second;
        }
    }
    
    // Remap elements to use deduplicated point indices
    for (const auto& element : elements) {
        if (element->nodeIds.size() == 3) {
            std::array<int, 3> newNodeIds = {
                nodeMapping[element->nodeIds[0]],
                nodeMapping[element->nodeIds[1]], 
                nodeMapping[element->nodeIds[2]]
            };
            
            // Check for degenerate triangles (all three points the same)
            if (newNodeIds[0] != newNodeIds[1] && 
                newNodeIds[1] != newNodeIds[2] && 
                newNodeIds[2] != newNodeIds[0]) {
                validElements.push_back(newNodeIds);
            }
        }
    }
    
    std::cout << "Deduplicated mesh: " << uniquePoints.size() << " unique nodes, " 
              << validElements.size() << " valid elements" << std::endl;
    
    // Prepare material IDs based on Z-coordinate of element centroid
    std::vector<int> materialIds;
    std::vector<int> regionIds;
    
    for (const auto& element : validElements) {
        // Calculate centroid Z coordinate
        double centroidZ = (uniquePoints[element[0]].Z() + 
                           uniquePoints[element[1]].Z() + 
                           uniquePoints[element[2]].Z()) / 3.0;
        
        int materialId, regionId;
        if (centroidZ < 0.5e-3) {
            materialId = 0;  // Substrate
            regionId = 0;    // Substrate region
        } else if (centroidZ < 0.55e-3) {
            materialId = 1;  // Oxide
            regionId = 5;    // Insulator region
        } else {
            materialId = 2;  // Gate
            regionId = 2;    // Gate region
        }
        materialIds.push_back(materialId);
        regionIds.push_back(regionId);
    }
    
    // Use VTKExporter to export with region data
    if (!VTKExporter::exportMeshWithCustomData(*mesh, filename, materialIds, regionIds)) {
        throw std::runtime_error("Failed to export mesh to VTK file: " + filename);
    }
    
    std::cout << "✓ Exported fine mesh to " << filename << std::endl;
    std::cout << "  • " << uniquePoints.size() << " unique nodes (no duplicates)" << std::endl;
    std::cout << "  • " << validElements.size() << " triangular elements" << std::endl;
    std::cout << "  • Material ID data for visualization" << std::endl;
}

int main() {
    try {
        std::cout << "=== Fine Conformal Mesh Example ===" << std::endl;
        std::cout << "Generating a truly fine mesh for ParaView visualization\n" << std::endl;
        
        // Create semiconductor device
        SemiconductorDevice device("Fine_MOSFET_Conformal");
        
        // Step 1: Create device with fine layers
        createFineConformalMeshDevice(device);
        
        // Step 2: Generate fine conformal mesh
        generateFineConformalMesh(device);
        
        // Step 3: Validate the device
        auto validation = device.validateDevice();
        std::cout << "\nValidation Results:" << std::endl;
        std::cout << "  " << validation.geometryMessage << std::endl;
        std::cout << "  " << validation.meshMessage << std::endl;
        
        // Step 4: Print device statistics
        std::cout << "\nDevice Statistics:" << std::endl;
        device.printDeviceInfo();
        
        // Step 5: Export with deduplication
        exportFineMeshWithDeduplication(device, "fine_conformal_mesh.vtk");
        
        // Also export geometry for comparison
        device.exportGeometry("fine_conformal_device.step", "STEP");
        device.exportGeometry("fine_conformal_device.brep", "BREP");
        
        std::cout << "\n=== ParaView Instructions ===" << std::endl;
        std::cout << "1. Open ParaView" << std::endl;
        std::cout << "2. File > Open > fine_conformal_mesh.vtk" << std::endl;
        std::cout << "3. Click 'Apply'" << std::endl;
        std::cout << "4. Set Coloring to 'MaterialID'" << std::endl;
        std::cout << "5. Try both 'Surface' and 'Wireframe' representations" << std::endl;
        std::cout << "6. You should now see the fine mesh detail!" << std::endl;
        
        std::cout << "\n=== Success ===" << std::endl;
        std::cout << "Fine conformal mesh generated and exported successfully!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }
}
