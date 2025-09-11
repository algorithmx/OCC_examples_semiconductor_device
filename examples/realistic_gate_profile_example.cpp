#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"

#include <iostream>

int main() {
    try {
        std::cout << "=== Realistic Gate Profile MOSFET Example ===" << std::endl;
        std::cout << "Demonstrating realistic gate structures using trapezoid with NURBS shoulders" << std::endl;
        
        // Create device
        SemiconductorDevice device("Realistic_MOSFET_with_NURBS_Gate");
        
        // Define materials
        MaterialProperties silicon(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon");
        MaterialProperties oxide(MaterialType::Silicon_Dioxide, 1.0e-12, 3.9 * 8.854e-12, 9.0, "SiO2");
        MaterialProperties polysilicon(MaterialType::Silicon, 1.0e3, 11.7 * 8.854e-12, 1.12, "Poly-Si");
        MaterialProperties metal(MaterialType::Metal_Contact, 1.0e7, 1.0 * 8.854e-12, 0.0, "Al");
        
        // Device dimensions (micrometers) - using larger dimensions for stable geometry
        const double deviceLength = 10e-6;   // 10 μm
        const double deviceWidth = 8e-6;     // 8 μm
        const double substrateThickness = 2e-6;  // 2 μm
        const double oxideThickness = 0.2e-6;    // 200 nm gate oxide (thicker for stability)
        
        std::cout << "\n1. Creating substrate layer..." << std::endl;
        
        // Create silicon substrate
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0), 
            Dimensions3D(deviceLength, deviceWidth, substrateThickness)
        );
        
        auto substrateLayer = std::make_unique<DeviceLayer>(
            substrate, silicon, DeviceRegion::Substrate, "Silicon_Substrate"
        );
        device.addLayer(std::move(substrateLayer));
        
        std::cout << "   ✓ Silicon substrate created: " 
                  << deviceLength*1e6 << " × " << deviceWidth*1e6 << " × " 
                  << substrateThickness*1e6 << " μm" << std::endl;
        
        std::cout << "\n2. Creating gate oxide layer..." << std::endl;
        
        try {
            // Create gate oxide layer
            TopoDS_Solid gateOxide = GeometryBuilder::createBox(
                gp_Pnt(2e-6, 0, substrateThickness), 
                Dimensions3D(6e-6, deviceWidth, oxideThickness)
            );
            
            if (!GeometryBuilder::isValidShape(gateOxide)) {
                std::cerr << "Error: Gate oxide geometry is invalid" << std::endl;
                return 1;
            }
            
            auto oxideLayer = std::make_unique<DeviceLayer>(
                gateOxide, oxide, DeviceRegion::Insulator, "Gate_Oxide"
            );
            device.addLayer(std::move(oxideLayer));
            
            std::cout << "   ✓ Gate oxide created: thickness " << oxideThickness*1e9 << " nm" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error creating gate oxide: " << e.what() << std::endl;
            return 1;
        }
        
        std::cout << "\n3. Creating realistic gate structure with NURBS shoulders..." << std::endl;
        
        // Gate dimensions
        const double gateBottomWidth = 3e-6;     // 3 μm bottom width
        const double gateTopWidth = 2e-6;        // 2 μm top width (tapered due to etching)
        const double gateHeight = 0.6e-6;        // 600 nm gate height
        const double shoulderRadius = 0.1e-6;    // 100 nm shoulder radius (reduced for stability)
        const double shoulderSharpness = 0.7;    // Aggressive etching profile
        
        try {
            
            std::cout << "   Creating gate with parameters:" << std::endl;
            std::cout << "     Position: (" << 3.5e-6*1e6 << ", 0, " << (substrateThickness + oxideThickness)*1e6 << ") μm" << std::endl;
            std::cout << "     Bottom width: " << gateBottomWidth*1e6 << " μm" << std::endl;
            std::cout << "     Top width: " << gateTopWidth*1e6 << " μm" << std::endl;
            std::cout << "     Height: " << gateHeight*1e6 << " μm" << std::endl;
            std::cout << "     Depth: " << deviceWidth*1e6 << " μm" << std::endl;
            std::cout << "     Shoulder radius: " << shoulderRadius*1e6 << " μm" << std::endl;
            std::cout << "     Shoulder sharpness: " << shoulderSharpness << std::endl;
            
            // Create gate using new NURBS shoulder method
            TopoDS_Solid realisticGate = GeometryBuilder::createTrapezoidWithNURBSShoulders(
                gp_Pnt(3.5e-6, 0, substrateThickness + oxideThickness), // Centered position
                gateBottomWidth,
                gateTopWidth, 
                gateHeight,
                deviceWidth,  // Full device width
                shoulderRadius,
                shoulderSharpness
            );
            
            if (!GeometryBuilder::isValidShape(realisticGate)) {
                std::cerr << "Error: Gate geometry is invalid" << std::endl;
                return 1;
            }
            
            auto gateLayer = std::make_unique<DeviceLayer>(
                realisticGate, polysilicon, DeviceRegion::Gate, "Realistic_Gate"
            );
            device.addLayer(std::move(gateLayer));
        } catch (const std::exception& e) {
            std::cerr << "Error creating gate: " << e.what() << std::endl;
            return 1;
        }
        
        std::cout << "   ✓ Realistic gate created with NURBS shoulders:" << std::endl;
        std::cout << "     Bottom width: " << gateBottomWidth*1e6 << " μm" << std::endl;
        std::cout << "     Top width: " << gateTopWidth*1e6 << " μm" << std::endl;
        std::cout << "     Height: " << gateHeight*1e6 << " μm" << std::endl;
        std::cout << "     Shoulder radius: " << shoulderRadius*1e6 << " μm" << std::endl;
        std::cout << "     Shoulder sharpness: " << shoulderSharpness << std::endl;
        
        std::cout << "\n4. Creating source and drain contact pads..." << std::endl;
        
        // Source contact (left side)
        TopoDS_Solid sourceContact = GeometryBuilder::createTrapezoidWithNURBSShoulders(
            gp_Pnt(0.5e-6, 0, substrateThickness + oxideThickness),
            1.2e-6,    // Bottom width
            1.0e-6,    // Top width  
            0.3e-6,    // Height
            deviceWidth,
            0.1e-6,    // Shoulder radius
            0.5        // Moderate sharpness
        );
        
        auto sourceLayer = std::make_unique<DeviceLayer>(
            sourceContact, metal, DeviceRegion::Contact, "Source_Contact"
        );
        device.addLayer(std::move(sourceLayer));
        
        // Drain contact (right side)
        TopoDS_Solid drainContact = GeometryBuilder::createTrapezoidWithNURBSShoulders(
            gp_Pnt(8.3e-6, 0, substrateThickness + oxideThickness),
            1.2e-6,    // Bottom width
            1.0e-6,    // Top width
            0.3e-6,    // Height
            deviceWidth,
            0.1e-6,    // Shoulder radius
            0.5        // Moderate sharpness
        );
        
        auto drainLayer = std::make_unique<DeviceLayer>(
            drainContact, metal, DeviceRegion::Contact, "Drain_Contact"
        );
        device.addLayer(std::move(drainLayer));
        
        std::cout << "   ✓ Source and drain contacts created with realistic profiles" << std::endl;
        
        std::cout << "\n5. Building complete device geometry..." << std::endl;
        
        // Build device geometry
        device.buildDeviceGeometry();
        
        std::cout << "   ✓ Device geometry assembled" << std::endl;
        
        // Print device information
        device.printDeviceInfo();
        
        std::cout << "\n6. Generating mesh..." << std::endl;
        
        // Generate fine mesh suitable for semiconductor device simulation
        double meshSize = 0.05e-6;  // 50 nm mesh size (increased for stability)
        device.generateGlobalBoundaryMesh(meshSize);
        
        std::cout << "   ✓ Fine mesh generated with " << meshSize*1e9 << " nm element size" << std::endl;
        
        // Get mesh statistics
        const BoundaryMesh* mesh = device.getGlobalMesh();
        if (mesh) {
            std::cout << "   ✓ Mesh statistics:" << std::endl;
            std::cout << "     Mesh object available" << std::endl;
        }
        
        std::cout << "\n7. Exporting device..." << std::endl;
        
        // Export geometry
        device.exportGeometry("realistic_nurbs_mosfet.step", "STEP");
        std::cout << "   ✓ Geometry exported: realistic_nurbs_mosfet.step" << std::endl;
        
        // Export mesh for simulation
        device.exportMesh("realistic_nurbs_mosfet.vtk", "VTK");
        std::cout << "   ✓ Mesh exported: realistic_nurbs_mosfet.vtk" << std::endl;
        
        // Also export in GMSH format for FEM solvers (if available)
        try {
            device.exportMesh("realistic_nurbs_mosfet.msh", "GMSH");
            std::cout << "   ✓ GMSH mesh exported: realistic_nurbs_mosfet.msh" << std::endl;
        } catch (...) {
            std::cout << "   ✗ GMSH export not available" << std::endl;
        }
        
        std::cout << "\n8. Device analysis..." << std::endl;
        
        // Calculate total volume and volume by material
        double totalVolume = device.getTotalVolume();
        std::cout << "   ✓ Total device volume: " << totalVolume*1e18 << " μm³" << std::endl;
        
        // Calculate volumes by material type
        auto volumesByMaterial = device.getVolumesByMaterial();
        for (const auto& pair : volumesByMaterial) {
            std::string materialName = SemiconductorDevice::getMaterialTypeName(pair.first);
            std::cout << "     " << materialName << ": " << pair.second*1e18 << " μm³" << std::endl;
        }
        
        // Validate geometry and mesh
        bool geometryValid = device.validateGeometry();
        bool meshValid = device.validateMesh();
        
        std::cout << "   ✓ Validation results:" << std::endl;
        std::cout << "     Geometry: " << (geometryValid ? "Valid" : "Invalid") << std::endl;
        std::cout << "     Mesh: " << (meshValid ? "Valid" : "Invalid") << std::endl;
        
        std::cout << "\nFiles Created:" << std::endl;
        std::cout << "  • realistic_nurbs_mosfet.step - Complete device geometry" << std::endl;
        std::cout << "  • realistic_nurbs_mosfet.vtk - Mesh for ParaView visualization" << std::endl;
        std::cout << "  • realistic_nurbs_mosfet.msh - Mesh for FEM simulation" << std::endl;
        
        std::cout << "\nKey Features Demonstrated:" << std::endl;
        std::cout << "  - Realistic gate profile with NURBS shoulder curves" << std::endl;
        std::cout << "  - Tapered gate structure (wider at bottom, narrower at top)" << std::endl;
        std::cout << "  - Smooth shoulder transitions mimicking real etching processes" << std::endl;
        std::cout << "  - Multiple contact structures with varying profiles" << std::endl;
        std::cout << "  - Fine mesh suitable for device simulation" << std::endl;
        
        std::cout << "\nSimulation Applications:" << std::endl;
        std::cout << "  - Electrical device simulation (TCAD)" << std::endl;
        std::cout << "  - Process simulation and optimization" << std::endl;
        std::cout << "  - Parasitic extraction" << std::endl;
        std::cout << "  - Thermal analysis of realistic device geometries" << std::endl;
        
        std::cout << "\n=== Realistic Gate Profile MOSFET Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
