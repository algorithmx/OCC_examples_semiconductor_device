#include "GeometryBuilder.h"

#include <iostream>

int main() {
    try {
        std::cout << "=== Trapezoid with NURBS Shoulders Example ===" << std::endl;
        std::cout << "Demonstrating trapezoidal prism geometry with curved shoulders" << std::endl;
        
        // Test basic trapezoid with NURBS shoulders
        std::cout << "\n1. Creating trapezoid with NURBS shoulders..." << std::endl;
        
        // Create a trapezoidal gate structure with rounded shoulders 
        // (typical for semiconductor etching processes)
        double bottomWidth = 2e-6;      // 2 μm base width
        double topWidth = 1e-6;         // 1 μm top width
        double height = 0.5e-6;         // 500 nm height  
        double depth = 10e-6;           // 10 μm depth
        double shoulderRadius = 0.1e-6; // 100 nm shoulder curvature
        double shoulderSharpness = 0.7; // More aggressive shoulder curve
        
        TopoDS_Solid trapezoidGate = GeometryBuilder::createTrapezoidWithNURBSShoulders(
            gp_Pnt(0, 0, 0),
            bottomWidth,
            topWidth,
            height,
            depth,
            shoulderRadius,
            shoulderSharpness
        );
        
        std::cout << "   ✓ Trapezoid gate created: " 
                  << bottomWidth*1e6 << " μm × " << topWidth*1e6 << " μm × " 
                  << height*1e6 << " μm × " << depth*1e6 << " μm" << std::endl;
        std::cout << "     Shoulder radius: " << shoulderRadius*1e6 << " μm" << std::endl;
        std::cout << "     Shoulder sharpness: " << shoulderSharpness << std::endl;
        std::cout << "     Volume: " << GeometryBuilder::calculateVolume(trapezoidGate)*1e18 << " μm³" << std::endl;
        
        // Test linear trapezoid (shoulderRadius = 0)
        std::cout << "\n2. Creating linear trapezoid (no shoulder curvature)..." << std::endl;
        
        TopoDS_Solid linearTrapezoid = GeometryBuilder::createTrapezoidWithNURBSShoulders(
            gp_Pnt(3e-6, 0, 0),  // Offset position
            bottomWidth,
            topWidth,
            height,
            depth,
            0.0,  // No shoulder radius -> linear edges
            0.5
        );
        
        std::cout << "   ✓ Linear trapezoid created for comparison" << std::endl;
        std::cout << "     Volume: " << GeometryBuilder::calculateVolume(linearTrapezoid)*1e18 << " μm³" << std::endl;
        
        // Test various shoulder sharpness values
        std::cout << "\n3. Creating trapezoids with different shoulder sharpness..." << std::endl;
        
        std::vector<double> sharpnessValues = {0.0, 0.3, 0.5, 0.7, 1.0};
        std::vector<TopoDS_Solid> trapezoids;
        
        for (size_t i = 0; i < sharpnessValues.size(); i++) {
            double sharpness = sharpnessValues[i];
            TopoDS_Solid trapezoid = GeometryBuilder::createTrapezoidWithNURBSShoulders(
                gp_Pnt(0, (i+1) * 12e-6, 0),  // Offset in Y direction
                bottomWidth,
                topWidth,
                height,
                depth,
                shoulderRadius,
                sharpness
            );
            trapezoids.push_back(trapezoid);
            
            double volume = GeometryBuilder::calculateVolume(trapezoid);
            std::cout << "   ✓ Sharpness " << sharpness << ": Volume = " 
                      << volume*1e18 << " μm³" << std::endl;
        }
        
        // Test shape validation
        std::cout << "\n4. Testing shape validation..." << std::endl;
        
        bool isGateValid = GeometryBuilder::isValidShape(trapezoidGate);
        bool isLinearValid = GeometryBuilder::isValidShape(linearTrapezoid);
        
        std::cout << "   ✓ Shape validation:" << std::endl;
        std::cout << "     NURBS trapezoid: " << (isGateValid ? "Valid" : "Invalid") << std::endl;
        std::cout << "     Linear trapezoid: " << (isLinearValid ? "Valid" : "Invalid") << std::endl;
        
        // Test geometric analysis
        std::cout << "\n5. Geometric analysis..." << std::endl;
        
        auto bbox = GeometryBuilder::getBoundingBox(trapezoidGate);
        gp_Pnt centroid = GeometryBuilder::calculateCentroid(trapezoidGate);
        double surfaceArea = GeometryBuilder::calculateSurfaceArea(trapezoidGate);
        
        std::cout << "   ✓ NURBS trapezoid bounding box:" << std::endl;
        std::cout << "     Min: [" << bbox.first.X()*1e6 << ", " << bbox.first.Y()*1e6 
                  << ", " << bbox.first.Z()*1e6 << "] μm" << std::endl;
        std::cout << "     Max: [" << bbox.second.X()*1e6 << ", " << bbox.second.Y()*1e6 
                  << ", " << bbox.second.Z()*1e6 << "] μm" << std::endl;
        std::cout << "   ✓ Centroid: [" << centroid.X()*1e6 << ", " << centroid.Y()*1e6 
                  << ", " << centroid.Z()*1e6 << "] μm" << std::endl;
        std::cout << "   ✓ Surface area: " << surfaceArea*1e12 << " μm²" << std::endl;
        
        // Export shapes for visualization
        std::cout << "\n6. Exporting geometries..." << std::endl;
        
        bool gateExported = GeometryBuilder::exportSTEP(trapezoidGate, "trapezoid_nurbs_gate.step");
        bool linearExported = GeometryBuilder::exportSTEP(linearTrapezoid, "trapezoid_linear.step");
        
        std::cout << "   " << (gateExported ? "✓" : "✗") 
                  << " NURBS trapezoid exported: trapezoid_nurbs_gate.step" << std::endl;
        std::cout << "   " << (linearExported ? "✓" : "✗") 
                  << " Linear trapezoid exported: trapezoid_linear.step" << std::endl;
        
        // Export array of different sharpness values
        for (size_t i = 0; i < trapezoids.size(); i++) {
            std::string filename = "trapezoid_sharpness_" + std::to_string(sharpnessValues[i]) + ".step";
            bool exported = GeometryBuilder::exportSTEP(trapezoids[i], filename);
            std::cout << "   " << (exported ? "✓" : "✗") 
                      << " Sharpness " << sharpnessValues[i] << " exported: " << filename << std::endl;
        }
        
        std::cout << "\nFiles Created:" << std::endl;
        std::cout << "  • trapezoid_nurbs_gate.step - Main NURBS shouldered trapezoid" << std::endl;
        std::cout << "  • trapezoid_linear.step - Linear comparison trapezoid" << std::endl;
        std::cout << "  • trapezoid_sharpness_*.step - Various shoulder sharpness examples" << std::endl;
        
        std::cout << "\nUsage Applications:" << std::endl;
        std::cout << "  - Realistic gate profiles in semiconductor manufacturing" << std::endl;
        std::cout << "  - Etching simulation geometries with rounded shoulders" << std::endl;
        std::cout << "  - Fin structures with controlled sidewall profiles" << std::endl;
        std::cout << "  - Any device feature requiring smooth trapezoidal transitions" << std::endl;
        
        std::cout << "\nVisualization Tips:" << std::endl;
        std::cout << "  - Open .step files in CAD software to see the NURBS shoulder curves" << std::endl;
        std::cout << "  - Compare different sharpness values to see curve variation" << std::endl;
        std::cout << "  - Use shoulderRadius to control the smoothness of the transition" << std::endl;
        std::cout << "  - Combine with boolean operations for complex device structures" << std::endl;
        
        std::cout << "\n=== Trapezoid NURBS Shoulders Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
