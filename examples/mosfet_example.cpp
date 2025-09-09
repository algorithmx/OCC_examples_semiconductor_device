#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"
#include "VTKExporter.h"

#include <iostream>
#include <memory>

int main() {
    try {
        std::cout << "=== Realistic MOSFET Device Example ===" << std::endl;
        std::cout << "Comprehensive MOSFET modeling with utility methods and optimized visualization" << std::endl;
        
        // Create a new semiconductor device using modern approach
        SemiconductorDevice device("Realistic_MOSFET");
        device.setCharacteristicLength(1.0e-6); // 1 micrometer characteristic length
        
        // Device dimensions - optimized for visualization (reduced height/depth)
        double width = 200e-6;           // 200 μm width (lateral dimension)
        double length = 200e-6;          // 200 μm length (lateral dimension)  
        double substrateHeight = 25e-6;  // 25 μm substrate (reduced for better aspect ratio)
        double oxideHeight = 3e-6;       // 3 μm oxide (realistic but visible)
        double gateHeight = 8e-6;        // 8 μm gate (good for visualization)
        
        // MOSFET-specific dimensions
        double gateLength = 50e-6;       // 50 μm gate length (active region)
        double sourceLength = 75e-6;     // 75 μm source region length
        double drainLength = 75e-6;      // 75 μm drain region length
        double contactHeight = 12e-6;    // 12 μm contact height
        
        std::cout << "\nDevice dimensions (optimized for visualization):" << std::endl;
        std::cout << "  Total Width (Y): " << width*1e6 << " μm" << std::endl;
        std::cout << "  Total Length (X): " << length*1e6 << " μm" << std::endl;
        std::cout << "  Gate Length: " << gateLength*1e6 << " μm" << std::endl;
        std::cout << "  Source/Drain Length: " << sourceLength*1e6 << "/" << drainLength*1e6 << " μm" << std::endl;
        std::cout << "  Substrate height (Z): " << substrateHeight*1e6 << " μm" << std::endl;
        std::cout << "  Oxide height (Z): " << oxideHeight*1e6 << " μm" << std::endl;
        std::cout << "  Gate height (Z): " << gateHeight*1e6 << " μm" << std::endl;
        std::cout << "  Contact height (Z): " << contactHeight*1e6 << " μm" << std::endl;
        std::cout << "  Total device height: " << (substrateHeight + oxideHeight + std::max(gateHeight, contactHeight))*1e6 << " μm" << std::endl;
        
        // Create MOSFET with Source/Drain regions (custom structure)
        std::cout << "\nCreating MOSFET structure with Source/Drain regions..." << std::endl;
        
        // Create standard materials using utility methods
        auto silicon = SemiconductorDevice::createStandardSilicon();
        auto oxide = SemiconductorDevice::createStandardSiliconDioxide();
        auto polysilicon = SemiconductorDevice::createStandardPolysilicon();
        auto metal = SemiconductorDevice::createStandardMetal();
        
        // 1. Substrate layer (base of the entire device)
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0),
            Dimensions3D(length, width, substrateHeight)
        );
        auto substrateLayer = std::make_unique<DeviceLayer>(
            substrate, silicon, DeviceRegion::Substrate, "Substrate"
        );
        device.addLayer(std::move(substrateLayer));
        
        // 2. Gate oxide layer (center region only)
        double oxideStart = sourceLength;
        TopoDS_Solid gateOxide = GeometryBuilder::createBox(
            gp_Pnt(oxideStart, width*0.25, substrateHeight),
            Dimensions3D(gateLength, width*0.5, oxideHeight)
        );
        auto oxideLayer = std::make_unique<DeviceLayer>(
            gateOxide, oxide, DeviceRegion::Insulator, "Gate_Oxide"
        );
        device.addLayer(std::move(oxideLayer));
        
        // 3. Gate contact (on top of oxide)
        double gateStart = oxideStart + gateLength*0.1; // slightly inset
        double gateWidth = gateLength*0.8;
        TopoDS_Solid gate = GeometryBuilder::createBox(
            gp_Pnt(gateStart, width*0.3, substrateHeight + oxideHeight),
            Dimensions3D(gateWidth, width*0.4, gateHeight)
        );
        auto gateLayer = std::make_unique<DeviceLayer>(
            gate, polysilicon, DeviceRegion::Gate, "Gate"
        );
        device.addLayer(std::move(gateLayer));
        
        // 4. Source contact (left side)
        TopoDS_Solid source = GeometryBuilder::createBox(
            gp_Pnt(0, width*0.1, substrateHeight + oxideHeight),
            Dimensions3D(sourceLength*0.8, width*0.8, contactHeight)
        );
        auto sourceLayer = std::make_unique<DeviceLayer>(
            source, metal, DeviceRegion::Source, "Source_Contact"
        );
        device.addLayer(std::move(sourceLayer));
        
        // 5. Drain contact (right side)
        double drainStart = sourceLength + gateLength;
        TopoDS_Solid drain = GeometryBuilder::createBox(
            gp_Pnt(drainStart + drainLength*0.2, width*0.1, substrateHeight + oxideHeight),
            Dimensions3D(drainLength*0.8, width*0.8, contactHeight)
        );
        auto drainLayer = std::make_unique<DeviceLayer>(
            drain, metal, DeviceRegion::Drain, "Drain_Contact"
        );
        device.addLayer(std::move(drainLayer));
        
        // Build the complete device geometry
        device.buildDeviceGeometry();
        std::cout << "✓ MOSFET structure with Source/Drain regions created successfully" << std::endl;
        
        // Print initial device information
        device.printDeviceInfo();
        
        // Generate meshes for all layers with optimized sizing
        std::cout << "\nGenerating meshes with optimized sizing for 5-layer device..." << std::endl;
        
        // Generate individual layer meshes with custom sizing
        DeviceLayer* substratePtr = device.getLayer("Substrate");
        if (substratePtr) {
            substratePtr->generateBoundaryMesh(8e-6); // 8 μm for substrate
        }
        
        DeviceLayer* oxidePtr = device.getLayer("Gate_Oxide");
        if (oxidePtr) {
            oxidePtr->generateBoundaryMesh(2e-6); // 2 μm for thin oxide
        }
        
        DeviceLayer* gatePtr = device.getLayer("Gate");
        if (gatePtr) {
            gatePtr->generateBoundaryMesh(4e-6); // 4 μm for gate
        }
        
        DeviceLayer* sourcePtr = device.getLayer("Source_Contact");
        if (sourcePtr) {
            sourcePtr->generateBoundaryMesh(5e-6); // 5 μm for source contact
        }
        
        DeviceLayer* drainPtr = device.getLayer("Drain_Contact");
        if (drainPtr) {
            drainPtr->generateBoundaryMesh(5e-6); // 5 μm for drain contact
        }
        
        std::cout << "✓ All 5 layer meshes generated with visualization-optimized sizing" << std::endl;
        
        // Also generate global mesh for comparison
        std::cout << "\nGenerating global device mesh..." << std::endl;
        device.generateGlobalBoundaryMesh(6e-6); // 6 μm global mesh (finer for smaller device)
        
        // Print updated device information with mesh statistics
        device.printDeviceInfo();
        
        // Comprehensive validation using utility method
        std::cout << "\nValidating device..." << std::endl;
        auto validation = device.validateDevice();
        std::cout << validation.geometryMessage << std::endl;
        std::cout << validation.meshMessage << std::endl;
        
        // Export comprehensive set (geometry + traditional VTK + regions VTK)
        std::cout << "\nExporting results..." << std::endl;
        device.exportDeviceComplete("realistic_mosfet", true);
        
        // Additional individual exports for quick iteration
        device.exportGeometry("realistic_mosfet_geometry.step", "STEP");
        device.exportMesh("realistic_mosfet_global.vtk", "VTK");
        device.exportMeshWithRegions("realistic_mosfet_regions.vtk", "VTK");
        
        // Calculate and display volumes by material with modern formatting
        auto volumes = device.getVolumesByMaterial();
        std::cout << "\nVolume Analysis:" << std::endl;
        for (const auto& pair : volumes) {
            std::cout << "  Material " << static_cast<int>(pair.first) 
                      << " (" << SemiconductorDevice::getMaterialTypeName(pair.first) << "): " 
                      << pair.second*1e12 << " μm³" << std::endl;
        }
        
        std::cout << "\nFiles Generated:" << std::endl;
        std::cout << "  • realistic_mosfet.step - 3D geometry (optimized for visualization)" << std::endl;
        std::cout << "  • realistic_mosfet_traditional.vtk - Traditional mesh" << std::endl;
        std::cout << "  • realistic_mosfet_with_regions.vtk - Enhanced mesh with region data" << std::endl;
        std::cout << "  • realistic_mosfet_geometry.step - Individual geometry export" << std::endl;
        std::cout << "  • realistic_mosfet_global.vtk - Global mesh" << std::endl;
        std::cout << "  • realistic_mosfet_regions.vtk - Regional mesh" << std::endl;
        
        std::cout << "\nVisualization Tips:" << std::endl;
        std::cout << "  1. Open *_with_regions.vtk files in ParaView for best visualization" << std::endl;
        std::cout << "  2. Color by 'MaterialID' to distinguish materials:" << std::endl;
        std::cout << "     - Silicon (0), SiO2 (5), Polysilicon (6), Metal (6)" << std::endl;
        std::cout << "  3. Color by 'RegionID' to see device regions:" << std::endl;
        std::cout << "     - Substrate (0), Gate (2), Insulator (5), Source (3), Drain (4)" << std::endl;
        std::cout << "  4. Color by 'ElementQuality' to analyze mesh quality and identify problem areas" << std::endl;
        std::cout << "  5. Use transparency and clipping planes to see internal structure" << std::endl;
        std::cout << "  6. Try cross-sectional views along X-axis to see S-G-D alignment" << std::endl;
        std::cout << "  7. Reduced Z-height makes cross-sectional views clearer" << std::endl;
        
        std::cout << "\nDevice Characteristics:" << std::endl;
        std::cout << "  - Complete 5-layer MOSFET: Substrate + Gate Oxide + Gate + Source + Drain" << std::endl;
        std::cout << "  - Realistic S-G-D layout with proper spacing and alignment" << std::endl;
        std::cout << "  - Optimized aspect ratio (5.5:1 lateral to vertical) for better visualization" << std::endl;
        std::cout << "  - Fine mesh sizing appropriate for each material and region" << std::endl;
        std::cout << "  - Comprehensive export formats for different analysis tools" << std::endl;
        
        std::cout << "\n=== Realistic MOSFET Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
