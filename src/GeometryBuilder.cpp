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

// Extrusion operations
TopoDS_Solid GeometryBuilder::extrudeProfile(const Profile2D& profile, const gp_Vec& direction) {
    if (profile.points.size() < 3) {
        throw std::invalid_argument("Profile must have at least 3 points");
    }
    
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
    
    TopoDS_Solid result;
    result = TopoDS::Solid(prismMaker.Shape());
    return result;
}

// Boolean operations
TopoDS_Shape GeometryBuilder::unionShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    BRepAlgoAPI_Fuse fuseMaker(shape1, shape2);
    fuseMaker.Build();
    
    if (!fuseMaker.IsDone()) {
        throw std::runtime_error("Failed to perform union operation");
    }
    
    return fuseMaker.Shape();
}

TopoDS_Shape GeometryBuilder::intersectShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    BRepAlgoAPI_Common commonMaker(shape1, shape2);
    commonMaker.Build();
    
    if (!commonMaker.IsDone()) {
        throw std::runtime_error("Failed to perform intersection operation");
    }
    
    return commonMaker.Shape();
}

TopoDS_Shape GeometryBuilder::subtractShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2) {
    BRepAlgoAPI_Cut cutMaker(shape1, shape2);
    cutMaker.Build();
    
    if (!cutMaker.IsDone()) {
        throw std::runtime_error("Failed to perform subtraction operation");
    }
    
    return cutMaker.Shape();
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
    return !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid();
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
