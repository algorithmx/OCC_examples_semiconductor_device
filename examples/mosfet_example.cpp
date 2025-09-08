#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"

#include <iostream>
#include <memory>

int main() {
    try {
        std::cout << "=== MOSFET Device Creation Example ===" << std::endl;
        std::cout << "Creating a realistic 65nm technology node MOSFET device" << std::endl;
        
        // Create a new semiconductor device
        SemiconductorDevice device("MOSFET_65nm_Example");
        device.setCharacteristicLength(1.0e-6); // 1 micrometer
        
        // Define material properties with realistic values
        MaterialProperties siliconSubstrate(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon Substrate");
        MaterialProperties oxideLayer(MaterialType::Silicon_Dioxide, 1.0e-16, 3.9 * 8.854e-12, 9.0, "SiO2 Gate Oxide");
        MaterialProperties polyGate(MaterialType::Metal_Contact, 1.0e5, 1.0 * 8.854e-12, 0.0, "Polysilicon Gate");
        
        // Realistic MOSFET dimensions for 65nm technology node
        // Note: Using larger dimensions for better OpenCASCADE precision
        double gateLength = 65.0e-9 * 1000;    // 65 nm scaled to 65 μm for modeling
        double gateWidth = 10.0e-6 * 1000;     // 10 μm scaled to 10 mm
        double gateThickness = 150.0e-9 * 1000; // 150 nm poly gate scaled to 150 μm
        double sourceLength = 0.5e-6 * 1000;   // 0.5 μm scaled to 0.5 mm
        double drainLength = 0.5e-6 * 1000;    // 0.5 μm scaled to 0.5 mm  
        double channelLength = 65.0e-9 * 1000; // Same as gate length
        double oxideThickness = 2.0e-9 * 1000; // 2 nm oxide scaled to 2 μm
        double substrateThickness = 50.0e-6 * 1000; // 50 μm scaled to 50 mm
        
        std::cout << "\nDevice dimensions (scaled 1000x for modeling precision):" << std::endl;
        std::cout << "  Gate Length: " << gateLength*1e6 << " μm" << std::endl;
        std::cout << "  Gate Width: " << gateWidth*1e3 << " mm" << std::endl;
        std::cout << "  Channel Length: " << channelLength*1e6 << " μm" << std::endl;
        std::cout << "  Oxide Thickness: " << oxideThickness*1e6 << " μm" << std::endl;
        std::cout << "  Gate Thickness: " << gateThickness*1e6 << " μm" << std::endl;
        std::cout << "  Source/Drain Length: " << sourceLength*1e3 << " mm" << std::endl;
        std::cout << "  Substrate Thickness: " << substrateThickness*1e3 << " mm" << std::endl;
        
        TopoDS_Solid mosfetGeometry = GeometryBuilder::createMOSFET(
            gateLength, gateWidth, gateThickness,
            sourceLength, drainLength, channelLength,
            oxideThickness, substrateThickness
        );
        
        // Create individual layers for the device
        
        // 1. Substrate layer
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0),
            Dimensions3D(sourceLength + channelLength + drainLength, gateWidth, substrateThickness)
        );
        
        auto substrateLayer = std::make_unique<DeviceLayer>(
            substrate, siliconSubstrate, DeviceRegion::Substrate, "Substrate"
        );
        device.addLayer(std::move(substrateLayer));
        
        // 2. Gate oxide layer
        TopoDS_Solid oxide = GeometryBuilder::createBox(
            gp_Pnt(sourceLength, 0, substrateThickness),
            Dimensions3D(channelLength, gateWidth, oxideThickness)
        );
        
        auto oxideLayerPtr = std::make_unique<DeviceLayer>(
            oxide, oxideLayer, DeviceRegion::Insulator, "Gate_Oxide"
        );
        device.addLayer(std::move(oxideLayerPtr));
        
        // 3. Gate contact
        TopoDS_Solid gate = GeometryBuilder::createBox(
            gp_Pnt(sourceLength + (channelLength - gateLength)/2, 0, substrateThickness + oxideThickness),
            Dimensions3D(gateLength, gateWidth, gateThickness)
        );
        
        auto gateLayer = std::make_unique<DeviceLayer>(
            gate, polyGate, DeviceRegion::Gate, "Gate"
        );
        device.addLayer(std::move(gateLayer));
        
        // Build the complete device geometry
        device.buildDeviceGeometry();
        
        // Print device information
        device.printDeviceInfo();
        
        // Generate boundary mesh for the entire device
        std::cout << "\nGenerating boundary mesh..." << std::endl;
        device.generateGlobalBoundaryMesh(0.1e-6); // 0.1 μm mesh size
        
        // Generate individual layer meshes
        std::cout << "\nGenerating individual layer meshes..." << std::endl;
        DeviceLayer* substrateLayerPtr = device.getLayer("Substrate");
        if (substrateLayerPtr) {
            substrateLayerPtr->generateBoundaryMesh(0.2e-6); // Coarser mesh for substrate
        }
        
        DeviceLayer* oxideLayerPtr2 = device.getLayer("Gate_Oxide");
        if (oxideLayerPtr2) {
            oxideLayerPtr2->generateBoundaryMesh(0.02e-6); // Fine mesh for oxide
        }
        
        DeviceLayer* gateLayerPtr = device.getLayer("Gate");
        if (gateLayerPtr) {
            gateLayerPtr->generateBoundaryMesh(0.05e-6); // Medium mesh for gate
        }
        
        // Print updated device information with mesh statistics
        device.printDeviceInfo();
        
        // Validate the device
        if (device.validateGeometry()) {
            std::cout << "\n✓ Device geometry is valid" << std::endl;
        } else {
            std::cout << "\n✗ Device geometry is invalid" << std::endl;
        }
        
        if (device.validateMesh()) {
            std::cout << "✓ Device mesh is valid" << std::endl;
        } else {
            std::cout << "✗ Device mesh is invalid" << std::endl;
        }
        
        // Export geometry and mesh
        std::cout << "\nExporting results..." << std::endl;
        device.exportGeometry("mosfet_geometry.step", "STEP");
        device.exportGeometry("mosfet_geometry.brep", "BREP");
        device.exportMesh("mosfet_mesh.vtk", "VTK");
        
        // Calculate volumes by material
        auto volumesByMaterial = device.getVolumesByMaterial();
        std::cout << "\nVolume by Material:" << std::endl;
        for (const auto& pair : volumesByMaterial) {
            std::cout << "  Material " << static_cast<int>(pair.first) 
                      << ": " << pair.second << " m³" << std::endl;
        }
        
        std::cout << "\n=== MOSFET Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
