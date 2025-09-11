#include "GeometryBuilder.h"

#include <iostream>
#include <stdexcept>
#include <cmath>

// OpenCASCADE includes
#include <TopoDS.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <STEPControl_Writer.hxx>
#include <IGESControl_Writer.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI_Writer.hxx>
#include <BRepTools.hxx>
#include <gp_Ax2.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <Geom_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <ShapeFix_Shape.hxx>
#include <BRepLib.hxx>
#include <Standard_Failure.hxx>

// Basic primitive creation
TopoDS_Solid GeometryBuilder::createBox(const gp_Pnt& corner, const Dimensions3D& dimensions) {
    BRepPrimAPI_MakeBox boxMaker(corner, dimensions.length, dimensions.width, dimensions.height);
    boxMaker.Build();
    
    if (!boxMaker.IsDone()) {
        throw std::runtime_error("Failed to create box");
    }
    
    return boxMaker.Solid();
}

TopoDS_Solid GeometryBuilder::createBox(const gp_Pnt& corner1, const gp_Pnt& corner2) {
    BRepPrimAPI_MakeBox boxMaker(corner1, corner2);
    boxMaker.Build();
    
    if (!boxMaker.IsDone()) {
        throw std::runtime_error("Failed to create box");
    }
    
    return boxMaker.Solid();
}

TopoDS_Solid GeometryBuilder::createCylinder(const gp_Pnt& center, const gp_Vec& axis, 
                                           double radius, double height) {
    gp_Ax2 axisSystem(center, gp_Dir(axis));
    BRepPrimAPI_MakeCylinder cylMaker(axisSystem, radius, height);
    cylMaker.Build();
    
    if (!cylMaker.IsDone()) {
        throw std::runtime_error("Failed to create cylinder");
    }
    
    return cylMaker.Solid();
}

TopoDS_Solid GeometryBuilder::createSphere(const gp_Pnt& center, double radius) {
    BRepPrimAPI_MakeSphere sphereMaker(center, radius);
    sphereMaker.Build();
    
    if (!sphereMaker.IsDone()) {
        throw std::runtime_error("Failed to create sphere");
    }
    
    return sphereMaker.Solid();
}

TopoDS_Solid GeometryBuilder::createCone(const gp_Pnt& apex, const gp_Vec& axis, 
                                       double radius1, double radius2, double height) {
    gp_Ax2 axisSystem(apex, gp_Dir(axis));
    BRepPrimAPI_MakeCone coneMaker(axisSystem, radius1, radius2, height);
    coneMaker.Build();
    
    if (!coneMaker.IsDone()) {
        throw std::runtime_error("Failed to create cone");
    }
    
    return coneMaker.Solid();
}

// Advanced primitives for semiconductor geometries
TopoDS_Solid GeometryBuilder::createRectangularWafer(double length, double width, double thickness) {
    return createBox(gp_Pnt(-length/2, -width/2, 0), Dimensions3D(length, width, thickness));
}

TopoDS_Solid GeometryBuilder::createCircularWafer(double radius, double thickness) {
    return createCylinder(gp_Pnt(0, 0, 0), gp_Vec(0, 0, 1), radius, thickness);
}

// Trapezoid with NURBS (Bezier) shoulder profile extruded along +Y
TopoDS_Solid GeometryBuilder::createTrapezoidWithNURBSShoulders(
    const gp_Pnt& origin,
    double bottomWidth,
    double topWidth,
    double height,
    double depth,
    double shoulderRadius,
    double shoulderSharpness)
{
    if (bottomWidth <= 0 || topWidth <= 0 || height <= 0 || depth <= 0) {
        throw std::invalid_argument("createTrapezoidWithNURBSShoulders: dimensions must be positive");
    }
    if (shoulderRadius < 0) shoulderRadius = 0;
    if (shoulderSharpness < 0) shoulderSharpness = 0;
    if (shoulderSharpness > 1.0) shoulderSharpness = 1.0;

    // Build 2D profile in local X-Z plane at Y=0, then extrude along +Y by depth.
    // Center the top width above the bottom center.
    const double xCenter = bottomWidth * 0.5;
    const double topLeftX = xCenter - topWidth * 0.5;
    const double topRightX = xCenter + topWidth * 0.5;

    // Key profile points (local coordinates, Y=0)
    gp_Pnt pBL(0.0, 0.0, 0.0);                 // Bottom Left
    gp_Pnt pBR(bottomWidth, 0.0, 0.0);         // Bottom Right
    gp_Pnt pTR(topRightX, 0.0, height);        // Top Right
    gp_Pnt pTL(topLeftX, 0.0, height);         // Top Left

    // If shoulderRadius is zero, build a linear trapezoid
    if (shoulderRadius <= 1e-15) {
        BRepBuilderAPI_MakeWire w;
        w.Add(BRepBuilderAPI_MakeEdge(pBL, pBR));
        w.Add(BRepBuilderAPI_MakeEdge(pBR, pTR));
        w.Add(BRepBuilderAPI_MakeEdge(pTR, pTL));
        w.Add(BRepBuilderAPI_MakeEdge(pTL, pBL));
        BRepBuilderAPI_MakeFace f(w.Wire());
        if (!f.IsDone()) throw std::runtime_error("Failed to make trapezoid face");
        BRepPrimAPI_MakePrism prism(f.Face(), gp_Vec(0, depth, 0));
        if (!prism.IsDone()) throw std::runtime_error("Failed to extrude trapezoid prism");
        TopoDS_Shape solidShape = prism.Shape();
        // Translate to origin
        gp_Trsf tr;
        tr.SetTranslation(gp_Vec(origin.X(), origin.Y(), origin.Z()));
        return TopoDS::Solid(BRepBuilderAPI_Transform(solidShape, tr).Shape());
    }

    // Build Bezier (cubic) curves for shoulders
    const double r = std::min(shoulderRadius, std::min(height, bottomWidth) * 0.5);
    const double s = shoulderSharpness;

    // Right shoulder: pBR -> pTR with inward curvature
    TColgp_Array1OfPnt rightPoles(1, 4);
    rightPoles.SetValue(1, pBR);
    rightPoles.SetValue(4, pTR);
    // Control points: move inward (towards center) and upward from bottom,
    // and slightly outward from top going down to shape the shoulder.
    gp_Pnt rP1(std::max(pBR.X() - r * (1.0 + s), xCenter), 0.0, std::min(r, height * 0.5));
    gp_Pnt rP2(std::min(pTR.X() + r * (0.25 * s), bottomWidth), 0.0, std::max(height - r, height * 0.5));
    rightPoles.SetValue(2, rP1);
    rightPoles.SetValue(3, rP2);
    Handle(Geom_BezierCurve) rightCurve = new Geom_BezierCurve(rightPoles);

    // Left shoulder: pTL -> pBL (note reversed to keep wire orientation consistent)
    TColgp_Array1OfPnt leftPoles(1, 4);
    leftPoles.SetValue(1, pTL);
    leftPoles.SetValue(4, pBL);
    gp_Pnt lP1(std::max(pTL.X() - r * (0.25 * s), 0.0), 0.0, std::max(height - r, height * 0.5));
    gp_Pnt lP2(std::min(pBL.X() + r * (1.0 + s), xCenter), 0.0, std::min(r, height * 0.5));
    leftPoles.SetValue(2, lP1);
    leftPoles.SetValue(3, lP2);
    Handle(Geom_BezierCurve) leftCurve = new Geom_BezierCurve(leftPoles);

    // Edges: bottom, right shoulder (Bezier), top, left shoulder (Bezier)
    BRepBuilderAPI_MakeWire w;
    TopoDS_Edge eBottom = BRepBuilderAPI_MakeEdge(pBL, pBR);
    TopoDS_Edge eRight = BRepBuilderAPI_MakeEdge(rightCurve);
    TopoDS_Edge eTop = BRepBuilderAPI_MakeEdge(pTR, pTL);
    TopoDS_Edge eLeft = BRepBuilderAPI_MakeEdge(leftCurve);

    w.Add(eBottom);
    w.Add(eRight);
    w.Add(eTop);
    w.Add(eLeft);

    if (!w.IsDone()) throw std::runtime_error("Failed to build trapezoid NURBS wire");

    BRepBuilderAPI_MakeFace f(w.Wire());
    if (!f.IsDone()) throw std::runtime_error("Failed to make trapezoid NURBS face");

    BRepPrimAPI_MakePrism prism(f.Face(), gp_Vec(0, depth, 0));
    if (!prism.IsDone()) throw std::runtime_error("Failed to extrude trapezoid NURBS prism");

    TopoDS_Shape solidShape = prism.Shape();

    // Translate to origin
    gp_Trsf tr;
    tr.SetTranslation(gp_Vec(origin.X(), origin.Y(), origin.Z()));
    TopoDS_Shape moved = BRepBuilderAPI_Transform(solidShape, tr).Shape();

    return TopoDS::Solid(moved);
}

// Extrusion operations
TopoDS_Solid GeometryBuilder::extrudeProfile(const Profile2D& profile, const gp_Vec& direction) {
    if (profile.points.size() < 3) {
        throw std::invalid_argument("Profile must have at least 3 points");
    }

    try {
        // Create wire from profile points
        BRepBuilderAPI_MakeWire wireMaker;

        for (size_t i = 0; i < profile.points.size(); i++) {
            size_t next = (i + 1) % profile.points.size();
            if (!profile.closed && next == 0) break;

            BRepBuilderAPI_MakeEdge edgeMaker(profile.points[i], profile.points[next]);
            wireMaker.Add(edgeMaker.Edge());
        }

        if (!wireMaker.IsDone()) {
            throw std::runtime_error("Failed to create profile wire");
        }

        // Create face from wire
        BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
        if (!faceMaker.IsDone()) {
            throw std::runtime_error("Failed to create profile face");
        }

        // Extrude the face
        BRepPrimAPI_MakePrism prismMaker(faceMaker.Face(), direction);
        if (!prismMaker.IsDone()) {
            throw std::runtime_error("Failed to extrude profile");
        }

        // Safely extract a solid from the result shape
        TopoDS_Shape extruded = prismMaker.Shape();
        if (extruded.ShapeType() == TopAbs_SOLID) {
            return TopoDS::Solid(extruded);
        }
        // If it's a compound or something else, extract the first solid
        TopExp_Explorer exp(extruded, TopAbs_SOLID);
        if (exp.More()) {
            return TopoDS::Solid(exp.Current());
        }
        throw std::runtime_error("Extrusion did not produce a solid");
    } catch (...) {
        // Convert any OCC exception to std::runtime_error to avoid abort
        throw std::runtime_error("OpenCASCADE error during extrudeProfile");
    }
}

// Boolean operations
TopoDS_Shape GeometryBuilder::unionShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    try {
        BRepAlgoAPI_Fuse fuseMaker(shape1, shape2);
        fuseMaker.SetFuzzyValue(5e-9);
        fuseMaker.SetNonDestructive(true);
        fuseMaker.SetRunParallel(true);
        fuseMaker.Build();

        if (!fuseMaker.IsDone()) {
            throw std::runtime_error("Failed to perform union operation");
        }

        return fuseMaker.Shape();
    } catch (const Standard_Failure& e) {
        const char* msg = e.GetMessageString();
        throw std::runtime_error(std::string("OpenCASCADE error during union (fuse): ") + (msg ? msg : "<no message>"));
    } catch (...) {
        throw std::runtime_error("OpenCASCADE error during union (fuse)");
    }
}

TopoDS_Shape GeometryBuilder::intersectShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    try {
        BRepAlgoAPI_Common commonMaker(shape1, shape2);
        commonMaker.SetFuzzyValue(5e-9);
        commonMaker.SetNonDestructive(true);
        commonMaker.SetRunParallel(true);
        commonMaker.Build();

        if (!commonMaker.IsDone()) {
            throw std::runtime_error("Failed to perform intersection operation");
        }

        return commonMaker.Shape();
    } catch (const Standard_Failure& e) {
        const char* msg = e.GetMessageString();
        throw std::runtime_error(std::string("OpenCASCADE error during intersection (common): ") + (msg ? msg : "<no message>"));
    } catch (...) {
        throw std::runtime_error("OpenCASCADE error during intersection (common)");
    }
}

TopoDS_Shape GeometryBuilder::subtractShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    try {
        // First attempt
        {
            BRepAlgoAPI_Cut cutMaker(shape1, shape2);
            cutMaker.SetFuzzyValue(5e-9); // 5 nm
            cutMaker.SetNonDestructive(true);
            cutMaker.SetRunParallel(true);
            cutMaker.Build();
            if (cutMaker.IsDone()) {
                return cutMaker.Shape();
            }
        }
        // Retry with larger fuzzy and pre-repaired inputs
        {
            TopoDS_Shape s1 = repairShape(shape1);
            TopoDS_Shape s2 = repairShape(shape2);
            BRepAlgoAPI_Cut cutMaker2(s1, s2);
            cutMaker2.SetFuzzyValue(5e-8); // 50 nm
            cutMaker2.SetNonDestructive(true);
            cutMaker2.SetRunParallel(true);
            cutMaker2.Build();
            if (cutMaker2.IsDone()) {
                return cutMaker2.Shape();
            }
            throw std::runtime_error("Failed to perform subtraction operation (after retry)");
        }
    } catch (const Standard_Failure& e) {
        const char* msg = e.GetMessageString();
        throw std::runtime_error(std::string("OpenCASCADE error during subtraction (cut): ") + (msg ? msg : "<no message>"));
    } catch (...) {
        throw std::runtime_error("OpenCASCADE error during subtraction (cut)");
    }
}

// Semiconductor-specific geometries
TopoDS_Solid GeometryBuilder::createMOSFET(double gateLength, double gateWidth, double gateThickness,
                                         double sourceLength, double drainLength, 
                                         double channelLength, double oxideThickness,
                                         double substrateThickness) {
    // Create substrate
    TopoDS_Solid substrate = createBox(
        gp_Pnt(0, 0, 0), 
        Dimensions3D(sourceLength + channelLength + drainLength, gateWidth, substrateThickness)
    );
    
    // Create oxide layer
    TopoDS_Solid oxide = createBox(
        gp_Pnt(sourceLength, 0, substrateThickness),
        Dimensions3D(channelLength, gateWidth, oxideThickness)
    );
    
    // Create gate
    TopoDS_Solid gate = createBox(
        gp_Pnt(sourceLength + (channelLength - gateLength)/2, 0, substrateThickness + oxideThickness),
        Dimensions3D(gateLength, gateWidth, gateThickness)
    );
    
    // Combine all parts
    TopoDS_Shape combined = unionShapes(substrate, oxide);
    combined = unionShapes(combined, gate);
    
    // Extract solid from the combined shape
    TopoDS_Solid result;
    if (combined.ShapeType() == TopAbs_SOLID) {
        result = TopoDS::Solid(combined);
    } else {
        // If the result is a compound, extract the first solid
        TopExp_Explorer explorer(combined, TopAbs_SOLID);
        if (explorer.More()) {
            result = TopoDS::Solid(explorer.Current());
        } else {
            throw std::runtime_error("Failed to create MOSFET: No solid found in combined geometry");
        }
    }
    return result;
}

TopoDS_Solid GeometryBuilder::createDiode(double anodeArea, double cathodeArea, 
                                        double junctionThickness, double totalThickness) {
    double sideLength = sqrt(std::max(anodeArea, cathodeArea));
    
    // Create p-region (anode)
    TopoDS_Solid pRegion = createBox(
        gp_Pnt(-sideLength/2, -sideLength/2, 0),
        Dimensions3D(sideLength, sideLength, totalThickness - junctionThickness)
    );
    
    // Create n-region (cathode) 
    double nSide = sqrt(cathodeArea);
    TopoDS_Solid nRegion = createBox(
        gp_Pnt(-nSide/2, -nSide/2, totalThickness - junctionThickness),
        Dimensions3D(nSide, nSide, junctionThickness)
    );
    
    TopoDS_Shape combined = unionShapes(pRegion, nRegion);
    TopoDS_Solid result;
    result = TopoDS::Solid(combined);
    return result;
}

// Wire creation utilities
TopoDS_Wire GeometryBuilder::createRectangularWire(const gp_Pnt& corner, double length, double width) {
    BRepBuilderAPI_MakeWire wireMaker;
    
    gp_Pnt p1 = corner;
    gp_Pnt p2(corner.X() + length, corner.Y(), corner.Z());
    gp_Pnt p3(corner.X() + length, corner.Y() + width, corner.Z());
    gp_Pnt p4(corner.X(), corner.Y() + width, corner.Z());
    
    wireMaker.Add(BRepBuilderAPI_MakeEdge(p1, p2));
    wireMaker.Add(BRepBuilderAPI_MakeEdge(p2, p3));
    wireMaker.Add(BRepBuilderAPI_MakeEdge(p3, p4));
    wireMaker.Add(BRepBuilderAPI_MakeEdge(p4, p1));
    
    return wireMaker.Wire();
}

TopoDS_Edge GeometryBuilder::createLineSegment(const gp_Pnt& start, const gp_Pnt& end) {
    BRepBuilderAPI_MakeEdge edgeMaker(start, end);
    return edgeMaker.Edge();
}

// Face creation utilities
TopoDS_Face GeometryBuilder::createRectangularFace(const gp_Pnt& corner, double length, double width) {
    TopoDS_Wire wire = createRectangularWire(corner, length, width);
    BRepBuilderAPI_MakeFace faceMaker(wire);
    return faceMaker.Face();
}

// Transformation utilities
TopoDS_Shape GeometryBuilder::translate(const TopoDS_Shape& shape, const gp_Vec& translation) {
    gp_Trsf transform;
    transform.SetTranslation(translation);
    
    BRepBuilderAPI_Transform transformer(shape, transform);
    return transformer.Shape();
}

TopoDS_Shape GeometryBuilder::rotate(const TopoDS_Shape& shape, const gp_Ax1& axis, double angle) {
    gp_Trsf transform;
    transform.SetRotation(axis, angle);
    
    BRepBuilderAPI_Transform transformer(shape, transform);
    return transformer.Shape();
}

// Shape analysis utilities
double GeometryBuilder::calculateVolume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

double GeometryBuilder::calculateSurfaceArea(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape, properties);
    return properties.Mass();
}

gp_Pnt GeometryBuilder::calculateCentroid(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.CentreOfMass();
}

std::pair<gp_Pnt, gp_Pnt> GeometryBuilder::getBoundingBox(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    
    double xmin, ymin, zmin, xmax, ymax, zmax;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    
    return {gp_Pnt(xmin, ymin, zmin), gp_Pnt(xmax, ymax, zmax)};
}

// Shape validation and repair
bool GeometryBuilder::isValidShape(const TopoDS_Shape& shape) {
    try {
        if (shape.IsNull()) return false;
        BRepCheck_Analyzer analyzer(shape);
        return analyzer.IsValid();
    } catch (const Standard_Failure&) {
        return false;
    } catch (...) {
        return false;
    }
}

TopoDS_Shape GeometryBuilder::repairShape(const TopoDS_Shape& shape) {
    try {
        if (shape.IsNull()) return shape;
        Handle(ShapeFix_Shape) fixer = new ShapeFix_Shape(shape);
        fixer->SetPrecision(1e-9);
        fixer->SetMinTolerance(1e-9);
        fixer->SetMaxTolerance(1e-3);
        fixer->Perform();
        TopoDS_Shape fixed = fixer->Shape();
        // Enforce SameParameter to tidy up edge/curve consistency
        BRepLib::SameParameter(fixed, 1e-9, true);
        return fixed;
    } catch (...) {
        return shape;
    }
}

TopoDS_Shape GeometryBuilder::simplifyShape(const TopoDS_Shape& shape, double tolerance) {
    try {
        if (shape.IsNull()) return shape;
        TopoDS_Shape simplified = shape;
        BRepLib::SameParameter(simplified, tolerance, true);
        return simplified;
    } catch (...) {
        return shape;
    }
}

// Export utilities
bool GeometryBuilder::exportSTEP(const TopoDS_Shape& shape, const std::string& filename) {
    try {
        STEPControl_Writer writer;
        IFSelect_ReturnStatus status = writer.Transfer(shape, STEPControl_AsIs);
        
        if (status != IFSelect_RetDone) {
            return false;
        }
        
        status = writer.Write(filename.c_str());
        return (status == IFSelect_RetDone);
        
    } catch (...) {
        return false;
    }
}

bool GeometryBuilder::exportIGES(const TopoDS_Shape& shape, const std::string& filename) {
    try {
        IGESControl_Writer writer;
        writer.AddShape(shape);
        writer.ComputeModel();
        return writer.Write(filename.c_str());
        
    } catch (...) {
        return false;
    }
}

bool GeometryBuilder::exportSTL(const TopoDS_Shape& shape, const std::string& filename) {
    try {
        // Generate mesh first
        BRepMesh_IncrementalMesh mesh(shape, 0.1);
        mesh.Perform();
        
        StlAPI_Writer writer;
        return writer.Write(shape, filename.c_str());
        
    } catch (...) {
        return false;
    }
}

bool GeometryBuilder::exportBREP(const TopoDS_Shape& shape, const std::string& filename) {
    try {
        return BRepTools::Write(shape, filename.c_str());
    } catch (...) {
        return false;
    }
}

// Face extraction utility
std::vector<TopoDS_Face> GeometryBuilder::extractFaces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Face> faces;
    
    TopExp_Explorer faceExp(shape, TopAbs_FACE);
    for (; faceExp.More(); faceExp.Next()) {
        TopoDS_Face face = TopoDS::Face(faceExp.Current());
        faces.push_back(face);
    }
    
    return faces;
}
