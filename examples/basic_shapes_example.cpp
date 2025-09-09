#include "GeometryBuilder.h"

#include <iostream>

int main() {
    try {
        std::cout << "=== Basic Geometric Shapes and Operations Example ===" << std::endl;
        std::cout << "Demonstrating geometric primitives, boolean operations, and shape utilities" << std::endl;
        
        // Test basic geometry creation
        std::cout << "\n1. Creating basic geometric shapes..." << std::endl;
        
        // Create a simple box (representing a silicon die)
        double boxLength = 10e-3, boxWidth = 8e-3, boxHeight = 0.5e-3; // 10mm x 8mm x 0.5mm
        TopoDS_Solid box = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0), 
            Dimensions3D(boxLength, boxWidth, boxHeight)
        );
        
        std::cout << "   ✓ Silicon die (box) created: " 
                  << boxLength*1e3 << "mm x " << boxWidth*1e3 << "mm x " << boxHeight*1e3 << "mm" << std::endl;
        std::cout << "     Volume: " << GeometryBuilder::calculateVolume(box)*1e9 << " mm³" << std::endl;
        
        // Create a cylinder (representing a via or pillar)
        double cylRadius = 50e-6, cylHeight = 100e-6; // 50 μm radius, 100 μm height
        TopoDS_Solid cylinder = GeometryBuilder::createCylinder(
            gp_Pnt(5e-3, 4e-3, 0), // Center on the die
            gp_Vec(0, 0, 1), 
            cylRadius, cylHeight
        );
        
        std::cout << "   ✓ Through-silicon via (cylinder) created: radius " 
                  << cylRadius*1e6 << " μm, height " << cylHeight*1e6 << " μm" << std::endl;
        std::cout << "     Volume: " << GeometryBuilder::calculateVolume(cylinder)*1e12 << " μm³" << std::endl;
        
        // Create a wafer (representing a silicon wafer)
        double waferRadius = 100e-3, waferThickness = 0.75e-3; // 200mm diameter wafer, 0.75mm thick
        TopoDS_Solid wafer = GeometryBuilder::createCircularWafer(waferRadius, waferThickness);
        
        std::cout << "   ✓ Silicon wafer created: diameter " 
                  << waferRadius*2e3 << " mm, thickness " << waferThickness*1e3 << " mm" << std::endl;
        std::cout << "     Volume: " << GeometryBuilder::calculateVolume(wafer)*1e6 << " cm³" << std::endl;
        
        // Test boolean operations (semiconductor manufacturing context)
        std::cout << "\n2. Testing boolean operations (semiconductor manufacturing contexts)..." << std::endl;
        
        // Create a substrate and an etch region
        TopoDS_Solid substrate = GeometryBuilder::createBox(gp_Pnt(0, 0, 0), Dimensions3D(200e-6, 200e-6, 50e-6));
        TopoDS_Solid etchRegion = GeometryBuilder::createBox(gp_Pnt(75e-6, 75e-6, 10e-6), Dimensions3D(50e-6, 50e-6, 30e-6));
        
        // Union: Combining multiple device layers
        TopoDS_Solid layer1 = GeometryBuilder::createBox(gp_Pnt(0, 0, 0), Dimensions3D(100e-6, 100e-6, 10e-6));
        TopoDS_Solid layer2 = GeometryBuilder::createBox(gp_Pnt(50e-6, 50e-6, 10e-6), Dimensions3D(100e-6, 100e-6, 10e-6));
        TopoDS_Shape unionResult = GeometryBuilder::unionShapes(layer1, layer2);
        double unionVolume = GeometryBuilder::calculateVolume(unionResult);
        
        // Intersection: Finding overlap regions between layers
        TopoDS_Shape intersectResult = GeometryBuilder::intersectShapes(layer1, layer2);
        double intersectVolume = GeometryBuilder::calculateVolume(intersectResult);
        
        // Subtraction: Etching/patterning operations
        TopoDS_Shape etchedSubstrate = GeometryBuilder::subtractShapes(substrate, etchRegion);
        double etchedVolume = GeometryBuilder::calculateVolume(etchedSubstrate);
        
        std::cout << "   ✓ Union (layer combination): " << unionVolume*1e15 << " μm³" << std::endl;
        std::cout << "   ✓ Intersection (overlap region): " << intersectVolume*1e15 << " μm³" << std::endl;
        std::cout << "   ✓ Subtraction (etched substrate): " << etchedVolume*1e15 << " μm³" << std::endl;
        
        // Demonstrate geometric utility functions
        std::cout << "\n3. Testing geometric utility functions..." << std::endl;
        
        // Test bounding box calculation
        auto bbox = GeometryBuilder::getBoundingBox(box);
        std::cout << "   ✓ Silicon die bounding box:" << std::endl;
        std::cout << "     Min: [" << bbox.first.X()*1e3 << ", " << bbox.first.Y()*1e3 << ", " << bbox.first.Z()*1e3 << "] mm" << std::endl;
        std::cout << "     Max: [" << bbox.second.X()*1e3 << ", " << bbox.second.Y()*1e3 << ", " << bbox.second.Z()*1e3 << "] mm" << std::endl;
        
        // Test centroid calculation
        gp_Pnt waferCentroid = GeometryBuilder::calculateCentroid(wafer);
        std::cout << "   ✓ Wafer centroid: [" 
                  << waferCentroid.X()*1e3 << ", " << waferCentroid.Y()*1e3 << ", " << waferCentroid.Z()*1e3 << "] mm" << std::endl;
        
        // Test shape validation
        bool isBoxValid = GeometryBuilder::isValidShape(box);
        bool isCylinderValid = GeometryBuilder::isValidShape(cylinder);
        bool isWaferValid = GeometryBuilder::isValidShape(wafer);
        
        std::cout << "   ✓ Shape validation:" << std::endl;
        std::cout << "     Silicon die: " << (isBoxValid ? "Valid" : "Invalid") << std::endl;
        std::cout << "     Via cylinder: " << (isCylinderValid ? "Valid" : "Invalid") << std::endl;
        std::cout << "     Wafer: " << (isWaferValid ? "Valid" : "Invalid") << std::endl;
        
        // Export basic shapes for visualization
        std::cout << "\n4. Exporting geometric shapes..." << std::endl;
        
        bool boxExported = GeometryBuilder::exportSTEP(box, "silicon_die.step");
        bool cylinderExported = GeometryBuilder::exportSTEP(cylinder, "via_cylinder.step");
        bool waferExported = GeometryBuilder::exportSTEP(wafer, "silicon_wafer.step");
        bool unionExported = GeometryBuilder::exportSTEP(unionResult, "union_layers.step");
        bool etchedExported = GeometryBuilder::exportSTEP(etchedSubstrate, "etched_substrate.step");
        
        std::cout << "   " << (boxExported ? "✓" : "✗") << " Silicon die exported: silicon_die.step" << std::endl;
        std::cout << "   " << (cylinderExported ? "✓" : "✗") << " Via cylinder exported: via_cylinder.step" << std::endl;
        std::cout << "   " << (waferExported ? "✓" : "✗") << " Wafer exported: silicon_wafer.step" << std::endl;
        std::cout << "   " << (unionExported ? "✓" : "✗") << " Combined layers exported: union_layers.step" << std::endl;
        std::cout << "   " << (etchedExported ? "✓" : "✗") << " Etched substrate exported: etched_substrate.step" << std::endl;
        
        std::cout << "\nFiles Created:" << std::endl;
        std::cout << "  • silicon_die.step - Basic rectangular silicon die" << std::endl;
        std::cout << "  • via_cylinder.step - Through-silicon via geometry" << std::endl;
        std::cout << "  • silicon_wafer.step - Full wafer geometry" << std::endl;
        std::cout << "  • union_layers.step - Result of layer combination" << std::endl;
        std::cout << "  • etched_substrate.step - Substrate after etching" << std::endl;
        
        std::cout << "\nVisualization Tips:" << std::endl;
        std::cout << "  - Open .step files in CAD software (FreeCAD, SolidWorks, etc.)" << std::endl;
        std::cout << "  - Use these shapes as building blocks for more complex devices" << std::endl;
        std::cout << "  - Boolean operations are fundamental in semiconductor fabrication modeling" << std::endl;
        
        std::cout << "\n=== Basic Shapes Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
