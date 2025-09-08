#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"

#include <iostream>
#include <memory>

int main() {
    try {
        std::cout << "=== Basic Shapes and Semiconductor Device Example ===" << std::endl;
        
        // Test basic geometry creation
        std::cout << "\n1. Creating basic geometric shapes..." << std::endl;
        
        // Create a simple box
        TopoDS_Solid box = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0), 
            Dimensions3D(1.0, 0.5, 0.2)
        );
        
        std::cout << "   ✓ Box created" << std::endl;
        std::cout << "   Volume: " << GeometryBuilder::calculateVolume(box) << " m³" << std::endl;
        
        // Create a cylinder
        TopoDS_Solid cylinder = GeometryBuilder::createCylinder(
            gp_Pnt(2, 0, 0), 
            gp_Vec(0, 0, 1), 
            0.3, 0.5
        );
        
        std::cout << "   ✓ Cylinder created" << std::endl;
        std::cout << "   Volume: " << GeometryBuilder::calculateVolume(cylinder) << " m³" << std::endl;
        
        // Create a wafer
        TopoDS_Solid wafer = GeometryBuilder::createCircularWafer(1.0, 0.1);
        
        std::cout << "   ✓ Circular wafer created" << std::endl;
        std::cout << "   Volume: " << GeometryBuilder::calculateVolume(wafer) << " m³" << std::endl;
        
        // Test boolean operations
        std::cout << "\n2. Testing boolean operations..." << std::endl;
        
        TopoDS_Solid box1 = GeometryBuilder::createBox(gp_Pnt(0, 0, 0), Dimensions3D(2, 2, 2));
        TopoDS_Solid box2 = GeometryBuilder::createBox(gp_Pnt(1, 1, 1), Dimensions3D(2, 2, 2));
        
        TopoDS_Shape unionResult = GeometryBuilder::unionShapes(box1, box2);
        TopoDS_Shape intersectResult = GeometryBuilder::intersectShapes(box1, box2);
        TopoDS_Shape subtractResult = GeometryBuilder::subtractShapes(box1, box2);
        
        std::cout << "   ✓ Union operation completed" << std::endl;
        std::cout << "   ✓ Intersection operation completed" << std::endl;
        std::cout << "   ✓ Subtraction operation completed" << std::endl;
        
        // Test semiconductor device creation
        std::cout << "\n3. Creating a simple semiconductor device..." << std::endl;
        
        SemiconductorDevice device("SimpleDevice");
        
        // Create materials
        MaterialProperties silicon(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon");
        MaterialProperties oxide(MaterialType::Silicon_Dioxide, 1.0e-12, 3.9 * 8.854e-12, 9.0, "SiO2");
        
        // Create substrate layer
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0),
            Dimensions3D(2.0, 2.0, 0.5)
        );
        
        auto substrateLayer = std::make_unique<DeviceLayer>(
            substrate, silicon, DeviceRegion::Substrate, "Substrate"
        );
        device.addLayer(std::move(substrateLayer));
        
        // Create oxide layer
        TopoDS_Solid oxideLayer = GeometryBuilder::createBox(
            gp_Pnt(0.5, 0.5, 0.5),
            Dimensions3D(1.0, 1.0, 0.1)
        );
        
        auto oxideLayerPtr = std::make_unique<DeviceLayer>(
            oxideLayer, oxide, DeviceRegion::Insulator, "Oxide"
        );
        device.addLayer(std::move(oxideLayerPtr));
        
        std::cout << "   ✓ Device layers created" << std::endl;
        
        // Build device geometry
        device.buildDeviceGeometry();
        std::cout << "   ✓ Device geometry built" << std::endl;
        
        // Print device information
        device.printDeviceInfo();
        
        // Generate mesh
        std::cout << "\n4. Generating mesh..." << std::endl;
        device.generateGlobalBoundaryMesh(0.2);
        
        DeviceLayer* substratePtr = device.getLayer("Substrate");
        if (substratePtr) {
            substratePtr->generateBoundaryMesh(0.3);
        }
        
        DeviceLayer* oxidePtr = device.getLayer("Oxide");
        if (oxidePtr) {
            oxidePtr->generateBoundaryMesh(0.1);
        }
        
        std::cout << "   ✓ Meshes generated" << std::endl;
        
        // Print updated information
        device.printDeviceInfo();
        
        // Validation
        std::cout << "\n5. Validation..." << std::endl;
        if (device.validateGeometry()) {
            std::cout << "   ✓ Geometry is valid" << std::endl;
        } else {
            std::cout << "   ✗ Geometry is invalid" << std::endl;
        }
        
        if (device.validateMesh()) {
            std::cout << "   ✓ Mesh is valid" << std::endl;
        } else {
            std::cout << "   ✗ Mesh is invalid" << std::endl;
        }
        
        // Export
        std::cout << "\n6. Exporting files..." << std::endl;
        device.exportGeometry("simple_device.step", "STEP");
        device.exportGeometry("simple_device.brep", "BREP");
        device.exportMesh("simple_device.vtk", "VTK");
        
        std::cout << "   ✓ Files exported" << std::endl;
        
        std::cout << "\n=== Basic Shapes Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
