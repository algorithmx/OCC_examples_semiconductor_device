#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"
#include "VTKExporter.h"

#include <iostream>
#include <memory>

int main() {
    try {
        std::cout << "Creating simplified MOSFET structure..." << std::endl;
        
        // Create semiconductor device
        SemiconductorDevice device("Simple_MOSFET");
        device.setCharacteristicLength(1.0e-6);
        
        // Simplified dimensions for clear structure visualization
        double width = 100e-6;           // 100 μm width
        double length = 100e-6;          // 100 μm length  
        double substrateHeight = 20e-6;  // 20 μm substrate
        double oxideHeight = 2e-6;       // 2 μm oxide
        double gateHeight = 5e-6;        // 5 μm gate
        double contactHeight = 8e-6;     // 8 μm contacts
        
        // MOSFET layout
        double gateLength = 30e-6;       // 30 μm gate length
        double sourceLength = 35e-6;     // 35 μm source region
        double drainLength = 35e-6;      // 35 μm drain region
        
        // Create materials
        auto silicon = SemiconductorDevice::createStandardSilicon();
        auto oxide = SemiconductorDevice::createStandardSiliconDioxide();
        auto polysilicon = SemiconductorDevice::createStandardPolysilicon();
        auto metal = SemiconductorDevice::createStandardMetal();
        
        // 1. Substrate (base layer)
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0),
            Dimensions3D(length, width, substrateHeight)
        );
        device.addLayer(std::make_unique<DeviceLayer>(
            substrate, silicon, DeviceRegion::Substrate, "Substrate"
        ));
        
        // 2. Gate oxide (center region)
        TopoDS_Solid gateOxide = GeometryBuilder::createBox(
            gp_Pnt(sourceLength, width*0.2, substrateHeight),
            Dimensions3D(gateLength, width*0.6, oxideHeight)
        );
        device.addLayer(std::make_unique<DeviceLayer>(
            gateOxide, oxide, DeviceRegion::Insulator, "Gate_Oxide"
        ));
        
        // 3. Gate (on top of oxide)
        TopoDS_Solid gate = GeometryBuilder::createBox(
            gp_Pnt(sourceLength + gateLength*0.1, width*0.25, substrateHeight + oxideHeight),
            Dimensions3D(gateLength*0.8, width*0.5, gateHeight)
        );
        device.addLayer(std::make_unique<DeviceLayer>(
            gate, polysilicon, DeviceRegion::Gate, "Gate"
        ));
        
        // 4. Source contact
        TopoDS_Solid source = GeometryBuilder::createBox(
            gp_Pnt(0, width*0.15, substrateHeight + oxideHeight),
            Dimensions3D(sourceLength*0.8, width*0.7, contactHeight)
        );
        device.addLayer(std::make_unique<DeviceLayer>(
            source, metal, DeviceRegion::Source, "Source"
        ));
        
        // 5. Drain contact
        double drainStart = sourceLength + gateLength;
        TopoDS_Solid drain = GeometryBuilder::createBox(
            gp_Pnt(drainStart + drainLength*0.2, width*0.15, substrateHeight + oxideHeight),
            Dimensions3D(drainLength*0.8, width*0.7, contactHeight)
        );
        device.addLayer(std::make_unique<DeviceLayer>(
            drain, metal, DeviceRegion::Drain, "Drain"
        ));
        
        // Build device geometry
        device.buildDeviceGeometry();
        std::cout << "Structure created: 5 layers (Substrate/Oxide/Gate/Source/Drain)" << std::endl;
        
        // Generate meshes for all layers first
        std::cout << "Generating boundary meshes..." << std::endl;
        device.generateAllLayerMeshes(); // Generate meshes for all layers with default sizes
        
        // Also generate global mesh
        device.generateGlobalBoundaryMesh(4e-6); // 4 μm mesh size
        
        // Export single VTK file with region information
        std::cout << "Exporting mesh to simple_mosfet.vtk..." << std::endl;
        device.exportMeshWithRegions("simple_mosfet.vtk", "VTK");
        
        // Print basic device info
        auto volumes = device.getVolumesByMaterial();
        std::cout << "\nDevice Summary:" << std::endl;
        std::cout << "  Total volume: " << device.getTotalVolume()*1e12 << " μm³" << std::endl;
        std::cout << "  Mesh elements: " << device.getGlobalMesh()->getElementCount() << std::endl;
        std::cout << "  Output: simple_mosfet.vtk" << std::endl;
        
        std::cout << "\nVisualization: Open simple_mosfet.vtk in ParaView" << std::endl;
        std::cout << "  - Color by 'RegionID' to see device regions" << std::endl;
        std::cout << "  - Use transparency to see internal structure" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
