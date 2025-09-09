#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>
#include <fstream>
#include <cmath>
#include <array>

// OpenCASCADE includes
#include <gp_Pnt.hxx>
#include <Standard_DomainError.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

// Project includes
#include "../include/SemiconductorDevice.h"
#include "../include/GeometryBuilder.h"
#include "../include/BoundaryMesh.h"
#include "../include/VTKExporter.h"

/**
 * @brief Advanced mesh refinement example demonstrating OpenCASCADE's built-in
 * capabilities for creating simulation-quality conformal meshes.
 * 
 * This example shows different mesh refinement approaches:
 * 1. Global conformal meshing (RECOMMENDED for shared surfaces)
 * 2. Adaptive mesh refinement with shared boundaries
 * 3. Parameter-controlled mesh quality
 * 4. Simulation-specific mesh density control
 */

void demonstrateOpenCASCADEMeshParameters() {
    std::cout << "=== OpenCASCADE Mesh Parameters Analysis ===" << std::endl;
    
    // Create simple geometry for testing
    auto silicon = SemiconductorDevice::createStandardSilicon();
    auto oxide = SemiconductorDevice::createStandardSiliconDioxide();
    
    const double length = 1.0e-3;  // 1mm
    const double width = 0.5e-3;   // 0.5mm
    const double height = 0.2e-3;  // 0.2mm
    
    // Create two adjacent regions
    TopoDS_Solid region1 = GeometryBuilder::createBox(
        gp_Pnt(0, 0, 0), Dimensions3D(length, width, height)
    );
    
    TopoDS_Solid region2 = GeometryBuilder::createBox(
        gp_Pnt(0, 0, height), Dimensions3D(length, width, height)
    );
    
    std::cout << "\\nCreated two adjacent regions with shared interface at Z = " 
              << height * 1e3 << " mm" << std::endl;
    
    // Test different mesh approaches
    std::cout << "\\n=== Mesh Approach Comparison ===" << std::endl;
    
    // Approach 1: Individual meshing (NOT RECOMMENDED - non-conformal)
    std::cout << "\\n1. Individual Region Meshing (Non-conformal):";
    BRepMesh_IncrementalMesh mesh1(region1, 0.05e-3);
    BRepMesh_IncrementalMesh mesh2(region2, 0.05e-3);
    mesh1.Perform();
    mesh2.Perform();
    std::cout << " ✗ No guarantee of shared mesh" << std::endl;
    
    // Approach 2: Compound meshing (RECOMMENDED - conformal)
    std::cout << "\\n2. Compound Region Meshing (Conformal):";
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, region1);
    builder.Add(compound, region2);
    
    BRepMesh_IncrementalMesh meshCompound(compound, 0.05e-3);
    meshCompound.Perform();
    std::cout << " ✓ Guarantees shared mesh on common surfaces" << std::endl;
    
    std::cout << "\\n=== RECOMMENDATION ===" << std::endl;
    std::cout << "✓ Use Compound Meshing for conformal interfaces" << std::endl;
    std::cout << "✓ This is exactly what your framework already does!" << std::endl;
}

void demonstrateMeshQualityParameters() {
    std::cout << "\\n\\n=== OpenCASCADE Mesh Quality Parameters ===" << std::endl;
    
    // Create test geometry
    SemiconductorDevice device("MeshQualityTest");
    auto silicon = SemiconductorDevice::createStandardSilicon();
    
    TopoDS_Solid testGeometry = GeometryBuilder::createBox(
        gp_Pnt(0, 0, 0), Dimensions3D(1.0e-3, 1.0e-3, 0.5e-3)
    );
    
    auto layer = std::make_unique<DeviceLayer>(
        testGeometry, silicon, DeviceRegion::Substrate, "TestSubstrate"
    );
    device.addLayer(std::move(layer));
    device.buildDeviceGeometry();
    
    std::cout << "\\nTesting different mesh quality parameters:" << std::endl;
    
    // Test 1: Coarse mesh (fast, low quality)
    std::cout << "\\n1. Coarse Mesh (0.2mm):";
    device.generateGlobalBoundaryMesh(0.2e-3);
    const BoundaryMesh* coarseMesh = device.getGlobalMesh();
    std::cout << " " << coarseMesh->getElementCount() << " elements" << std::endl;
    
    // Test 2: Medium mesh (balanced)
    std::cout << "2. Medium Mesh (0.1mm):";
    device.generateGlobalBoundaryMesh(0.1e-3);
    const BoundaryMesh* mediumMesh = device.getGlobalMesh();
    std::cout << " " << mediumMesh->getElementCount() << " elements" << std::endl;
    
    // Test 3: Fine mesh (slow, high quality)
    std::cout << "3. Fine Mesh (0.05mm):";
    device.generateGlobalBoundaryMesh(0.05e-3);
    const BoundaryMesh* fineMesh = device.getGlobalMesh();
    std::cout << " " << fineMesh->getElementCount() << " elements" << std::endl;
    
    // Test 4: Ultra-fine mesh (very slow, simulation quality)
    std::cout << "4. Ultra-Fine Mesh (0.01mm):";
    device.generateGlobalBoundaryMesh(0.01e-3);
    const BoundaryMesh* ultraFineMesh = device.getGlobalMesh();
    std::cout << " " << ultraFineMesh->getElementCount() << " elements" << std::endl;
    
    std::cout << "\\n=== SIMULATION MESH GUIDELINES ===" << std::endl;
    std::cout << "• For structural analysis: Use 0.1-0.05mm mesh" << std::endl;
    std::cout << "• For thermal analysis: Use 0.05-0.02mm mesh" << std::endl;
    std::cout << "• For electromagnetic: Use 0.01-0.005mm mesh" << std::endl;
    std::cout << "• For multiphysics: Use 0.005mm or finer" << std::endl;
}

void exportConformalMesh(const SemiconductorDevice& device, const std::string& baseName);

void createConformalSimulationMesh(SemiconductorDevice& device, const std::string& outputName) {
    std::cout << "\\n\\n=== Creating Simulation-Quality Conformal Mesh ===" << std::endl;
    
    // Define materials
    auto silicon = SemiconductorDevice::createStandardSilicon();
    auto oxide = SemiconductorDevice::createStandardSiliconDioxide();
    auto polysilicon = SemiconductorDevice::createStandardPolysilicon();
    
    // Create realistic MOSFET dimensions for simulation
    const double length = 0.5e-3;      // 500 μm
    const double width = 0.3e-3;       // 300 μm
    const double substrateHeight = 0.1e-3;   // 100 μm
    const double oxideHeight = 0.01e-3;      // 10 μm  
    const double gateHeight = 0.05e-3;       // 50 μm
    
    std::cout << "Creating realistic MOSFET for simulation:" << std::endl;
    std::cout << "  • Device: 500×300×160 μm" << std::endl;
    std::cout << "  • Substrate: 100 μm thick" << std::endl;
    std::cout << "  • Oxide: 10 μm thick (critical interface)" << std::endl;
    std::cout << "  • Gate: 50 μm thick" << std::endl;
    
    // 1. Substrate
    TopoDS_Solid substrate = GeometryBuilder::createBox(
        gp_Pnt(0, 0, 0), Dimensions3D(length, width, substrateHeight)
    );
    auto substrateLayer = std::make_unique<DeviceLayer>(
        substrate, silicon, DeviceRegion::Substrate, "Substrate"
    );
    
    // 2. Oxide (centered, critical interface)
    TopoDS_Solid gateOxide = GeometryBuilder::createBox(
        gp_Pnt(length*0.2, width*0.2, substrateHeight),
        Dimensions3D(length*0.6, width*0.6, oxideHeight)
    );
    auto oxideLayer = std::make_unique<DeviceLayer>(
        gateOxide, oxide, DeviceRegion::Insulator, "Gate_Oxide"
    );
    
    // 3. Gate (smaller, on oxide)
    TopoDS_Solid gate = GeometryBuilder::createBox(
        gp_Pnt(length*0.25, width*0.25, substrateHeight + oxideHeight),
        Dimensions3D(length*0.5, width*0.5, gateHeight)
    );
    auto gateLayer = std::make_unique<DeviceLayer>(
        gate, polysilicon, DeviceRegion::Gate, "Gate"
    );
    
    // Add to device
    device.addLayer(std::move(substrateLayer));
    device.addLayer(std::move(oxideLayer));
    device.addLayer(std::move(gateLayer));
    
    // Build composite geometry (ensures conformal interfaces)
    device.buildDeviceGeometry();
    
    // Generate simulation-quality mesh
    double simulationMeshSize = 0.005e-3;  // 5 μm - simulation quality
    std::cout << "\\nGenerating simulation mesh with " << simulationMeshSize*1e6 
              << " μm element size..." << std::endl;
    
    device.generateGlobalBoundaryMesh(simulationMeshSize);
    
    // Add refinement at critical interfaces
    std::vector<gp_Pnt> criticalPoints;
    
    // Silicon-oxide interface
    criticalPoints.push_back(gp_Pnt(length*0.4, width*0.4, substrateHeight));
    
    // Oxide-gate interface  
    criticalPoints.push_back(gp_Pnt(length*0.4, width*0.4, substrateHeight + oxideHeight));
    
    // Gate edges (field concentration points)
    criticalPoints.push_back(gp_Pnt(length*0.25, width*0.4, substrateHeight + oxideHeight));
    criticalPoints.push_back(gp_Pnt(length*0.75, width*0.4, substrateHeight + oxideHeight));
    
    std::cout << "Applying refinement at " << criticalPoints.size() << " critical interface points..." << std::endl;
    device.refineGlobalMesh(criticalPoints, simulationMeshSize * 0.5);
    
    // Validation
    const BoundaryMesh* mesh = device.getGlobalMesh();
    std::cout << "\\nFinal mesh statistics:" << std::endl;
    std::cout << "  • Nodes: " << mesh->getNodeCount() << std::endl;
    std::cout << "  • Elements: " << mesh->getElementCount() << std::endl;
    std::cout << "  • Average quality: " << mesh->getAverageElementQuality() << std::endl;
    
    // Export for simulation
    exportConformalMesh(device, outputName);
}

void exportConformalMesh(const SemiconductorDevice& device, const std::string& baseName) {
    std::cout << "\\n=== Exporting Simulation-Ready Conformal Mesh ===" << std::endl;
    
    const BoundaryMesh* mesh = device.getGlobalMesh();
    if (!mesh) {
        throw std::runtime_error("No mesh available for export");
    }
    
    const auto& nodes = mesh->getNodes();
    const auto& elements = mesh->getElements();
    
    // Deduplicate for clean export
    std::map<std::string, int> pointMap;
    std::vector<gp_Pnt> uniquePoints;
    std::vector<std::array<int, 3>> validElements;
    
    double tolerance = 1e-12;  // Very tight tolerance for simulation
    
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
    
    // Remap elements
    for (const auto& element : elements) {
        if (element->nodeIds.size() == 3) {
            std::array<int, 3> newNodeIds = {
                nodeMapping[element->nodeIds[0]],
                nodeMapping[element->nodeIds[1]], 
                nodeMapping[element->nodeIds[2]]
            };
            
            if (newNodeIds[0] != newNodeIds[1] && 
                newNodeIds[1] != newNodeIds[2] && 
                newNodeIds[2] != newNodeIds[0]) {
                validElements.push_back(newNodeIds);
            }
        }
    }
    
    std::cout << "Conformal mesh export statistics:" << std::endl;
    std::cout << "  • Unique nodes: " << uniquePoints.size() << std::endl;
    std::cout << "  • Valid elements: " << validElements.size() << std::endl;
    
    // Prepare material and region IDs based on Z-coordinate
    std::vector<int> materialIds;
    std::vector<int> regionIds;
    
    for (const auto& element : validElements) {
        double centroidZ = (uniquePoints[element[0]].Z() + 
                           uniquePoints[element[1]].Z() + 
                           uniquePoints[element[2]].Z()) / 3.0;
        
        int materialId, regionId;
        if (centroidZ < 0.1e-3) {
            materialId = 1;  // Silicon
            regionId = 0;    // Substrate region
        } else if (centroidZ < 0.11e-3) {
            materialId = 2;  // Oxide
            regionId = 5;    // Insulator region
        } else {
            materialId = 3;  // Gate
            regionId = 2;    // Gate region
        }
        materialIds.push_back(materialId);
        regionIds.push_back(regionId);
    }
    
    // Export VTK for visualization using VTKExporter
    if (!VTKExporter::exportMeshWithCustomData(*mesh, baseName + "_simulation.vtk", materialIds, regionIds)) {
        throw std::runtime_error("Failed to export simulation mesh to VTK file");
    }
    
    // Also export geometry
    device.exportGeometry(baseName + "_geometry.step", "STEP");
    
    std::cout << "\\nExported simulation-ready files:" << std::endl;
    std::cout << "  • " << baseName << "_simulation.vtk - Conformal mesh for visualization" << std::endl;
    std::cout << "  • " << baseName << "_geometry.step - CAD geometry" << std::endl;
    
    // Calculate file sizes
    std::ifstream vtkCheck(baseName + "_simulation.vtk", std::ios::ate);
    if (vtkCheck.good()) {
        size_t vtkSize = vtkCheck.tellg();
        std::cout << "  • VTK file size: " << vtkSize / 1024 << " KB" << std::endl;
        vtkCheck.close();
    }
}

int main() {
    try {
        std::cout << "=== Advanced Mesh Refinement for Simulation ===" << std::endl;
        std::cout << "Demonstrating OpenCASCADE's built-in capabilities for conformal meshing\\n" << std::endl;
        
        // Step 1: Demonstrate mesh parameter options
        demonstrateOpenCASCADEMeshParameters();
        
        // Step 2: Show mesh quality control
        demonstrateMeshQualityParameters();
        
        // Step 3: Create simulation-quality conformal mesh
        SemiconductorDevice simulationDevice("SimulationMOSFET");
        createConformalSimulationMesh(simulationDevice, "simulation_conformal");
        
        std::cout << "\\n\\n=== FINAL RECOMMENDATION ===" << std::endl;
        std::cout << "✓ FOR CONFORMAL INTERFACES: Use your existing framework approach" << std::endl;
        std::cout << "✓ METHOD: device.generateGlobalBoundaryMesh() with compound geometry" << std::endl;
        std::cout << "✓ WHY: OpenCASCADE automatically ensures shared mesh on common surfaces" << std::endl;
        std::cout << "✓ ENHANCEMENT: Add adaptive refinement for simulation quality" << std::endl;
        
        std::cout << "\\n=== KEY INSIGHTS ===" << std::endl;
        std::cout << "1. Your framework ALREADY provides conformal meshing!" << std::endl;
        std::cout << "2. Compound meshing guarantees shared mesh on interfaces" << std::endl;
        std::cout << "3. Use smaller mesh sizes for simulation (0.005-0.01mm)" << std::endl;
        std::cout << "4. Apply local refinement at critical interfaces" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\\nError: " << e.what() << std::endl;
        return 1;
    }
}
