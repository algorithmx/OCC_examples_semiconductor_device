#ifndef GEOMETRY_BUILDER_H
#define GEOMETRY_BUILDER_H

#include <vector>
#include <memory>
#include <string>

// OpenCASCADE includes
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <Standard_Real.hxx>

/**
 * @brief Structure defining 3D dimensions
 */
struct Dimensions3D {
    double length;
    double width;
    double height;
    
    Dimensions3D(double l = 1.0, double w = 1.0, double h = 1.0) 
        : length(l), width(w), height(h) {}
};

/**
 * @brief Structure defining 2D profile for extrusion
 */
struct Profile2D {
    std::vector<gp_Pnt> points;
    bool closed;
    
    Profile2D(bool isClosed = true) : closed(isClosed) {}
    void addPoint(const gp_Pnt& point) { points.push_back(point); }
    void addPoint(double x, double y) { points.emplace_back(x, y, 0.0); }
};

/**
 * @brief Utility class for building 3D geometries for semiconductor devices
 */
class GeometryBuilder {
private:
    static constexpr double DEFAULT_TOLERANCE = 1e-6;
    
public:
    // Basic primitive creation
    static TopoDS_Solid createBox(const gp_Pnt& corner, const Dimensions3D& dimensions);
    static TopoDS_Solid createBox(const gp_Pnt& corner1, const gp_Pnt& corner2);
    static TopoDS_Solid createCylinder(const gp_Pnt& center, const gp_Vec& axis, 
                                      double radius, double height);
    static TopoDS_Solid createSphere(const gp_Pnt& center, double radius);
    static TopoDS_Solid createCone(const gp_Pnt& apex, const gp_Vec& axis, 
                                  double radius1, double radius2, double height);
    
    // Advanced primitives for semiconductor geometries
    static TopoDS_Solid createRectangularWafer(double length, double width, double thickness);
    static TopoDS_Solid createCircularWafer(double radius, double thickness);
    static TopoDS_Solid createFinFET(double finWidth, double finHeight, double finLength, 
                                    double gateLength, double sourceLength, double drainLength);
    
    // Extrusion and sweep operations
    static TopoDS_Solid extrudeProfile(const Profile2D& profile, const gp_Vec& direction);
    static TopoDS_Solid extrudeAlongPath(const Profile2D& profile, const TopoDS_Wire& path);
    static TopoDS_Solid sweepProfile(const TopoDS_Wire& profile, const TopoDS_Wire& path);
    static TopoDS_Solid revolveProfile(const TopoDS_Wire& profile, const gp_Ax1& axis, double angle);
    
    // Boolean operations
    static TopoDS_Shape unionShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2);
    static TopoDS_Shape intersectShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2);
    static TopoDS_Shape subtractShapes(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2);
    
    // Multi-shape boolean operations
    static TopoDS_Shape unionMultipleShapes(const std::vector<TopoDS_Shape>& shapes);
    static TopoDS_Shape intersectMultipleShapes(const std::vector<TopoDS_Shape>& shapes);
    
    // Semiconductor-specific geometries
    static TopoDS_Solid createMOSFET(double gateLength, double gateWidth, double gateThickness,
                                    double sourceLength, double drainLength, 
                                    double channelLength, double oxideThickness,
                                    double substrateThickness);
    
    static TopoDS_Solid createBJT(double emitterRadius, double baseThickness, 
                                 double collectorRadius, double collectorThickness);
    
    static TopoDS_Solid createDiode(double anodeArea, double cathodeArea, 
                                   double junctionThickness, double totalThickness);
    
    static TopoDS_Solid createCapacitor(double plateLength, double plateWidth,
                                       double plateThickness, double dielectricThickness);
    
    // Wire and edge creation utilities
    static TopoDS_Wire createRectangularWire(const gp_Pnt& corner, double length, double width);
    static TopoDS_Wire createCircularWire(const gp_Pnt& center, double radius);
    static TopoDS_Wire createPolygonalWire(const std::vector<gp_Pnt>& points, bool closed = true);
    static TopoDS_Wire createBSplineWire(const std::vector<gp_Pnt>& points, bool closed = false);
    
    static TopoDS_Edge createLineSegment(const gp_Pnt& start, const gp_Pnt& end);
    static TopoDS_Edge createArc(const gp_Pnt& start, const gp_Pnt& middle, const gp_Pnt& end);
    static TopoDS_Edge createCircularArc(const gp_Pnt& center, double radius, 
                                        double startAngle, double endAngle);
    
    // Face creation utilities
    static TopoDS_Face createPlanarFace(const TopoDS_Wire& outerWire);
    static TopoDS_Face createPlanarFace(const TopoDS_Wire& outerWire, 
                                       const std::vector<TopoDS_Wire>& holes);
    static TopoDS_Face createRectangularFace(const gp_Pnt& corner, double length, double width);
    static TopoDS_Face createCircularFace(const gp_Pnt& center, double radius);
    
    // Transformation utilities
    static TopoDS_Shape translate(const TopoDS_Shape& shape, const gp_Vec& translation);
    static TopoDS_Shape rotate(const TopoDS_Shape& shape, const gp_Ax1& axis, double angle);
    static TopoDS_Shape scale(const TopoDS_Shape& shape, const gp_Pnt& center, double factor);
    static TopoDS_Shape mirror(const TopoDS_Shape& shape, const gp_Ax2& plane);
    
    // Array operations
    static std::vector<TopoDS_Shape> linearArray(const TopoDS_Shape& shape, 
                                                const gp_Vec& direction, int count);
    static std::vector<TopoDS_Shape> circularArray(const TopoDS_Shape& shape, 
                                                  const gp_Ax1& axis, int count);
    static std::vector<TopoDS_Shape> rectangularArray(const TopoDS_Shape& shape,
                                                     const gp_Vec& dir1, int count1,
                                                     const gp_Vec& dir2, int count2);
    
    // Filleting and chamfering
    static TopoDS_Shape filletEdges(const TopoDS_Shape& shape, 
                                   const std::vector<TopoDS_Edge>& edges, double radius);
    static TopoDS_Shape filletAllEdges(const TopoDS_Shape& shape, double radius);
    static TopoDS_Shape chamferEdges(const TopoDS_Shape& shape, 
                                    const std::vector<TopoDS_Edge>& edges, double distance);
    
    // Shape analysis utilities
    static double calculateVolume(const TopoDS_Shape& shape);
    static double calculateSurfaceArea(const TopoDS_Shape& shape);
    static gp_Pnt calculateCentroid(const TopoDS_Shape& shape);
    static std::pair<gp_Pnt, gp_Pnt> getBoundingBox(const TopoDS_Shape& shape);
    
    // Shape validation and repair
    static bool isValidShape(const TopoDS_Shape& shape);
    static TopoDS_Shape repairShape(const TopoDS_Shape& shape);
    static TopoDS_Shape simplifyShape(const TopoDS_Shape& shape, double tolerance = DEFAULT_TOLERANCE);
    
    // Import/Export utilities
    static TopoDS_Shape importSTEP(const std::string& filename);
    static TopoDS_Shape importIGES(const std::string& filename);
    static TopoDS_Shape importSTL(const std::string& filename);
    static TopoDS_Shape importBREP(const std::string& filename);
    
    static bool exportSTEP(const TopoDS_Shape& shape, const std::string& filename);
    static bool exportIGES(const TopoDS_Shape& shape, const std::string& filename);
    static bool exportSTL(const TopoDS_Shape& shape, const std::string& filename);
    static bool exportBREP(const TopoDS_Shape& shape, const std::string& filename);
    
    // Mesh-related geometry operations
    static TopoDS_Shape createMeshBoundary(const std::vector<gp_Pnt>& nodes,
                                          const std::vector<std::array<int, 3>>& triangles);
    static std::vector<TopoDS_Face> extractFaces(const TopoDS_Shape& shape);
    static std::vector<TopoDS_Edge> extractEdges(const TopoDS_Shape& shape);
    
    // Semiconductor device specific utilities
    static TopoDS_Solid createSubstrate(const Profile2D& profile, double thickness);
    static TopoDS_Solid createDopedRegion(const TopoDS_Solid& substrate, 
                                         const Profile2D& dopingProfile, double depth);
    static std::vector<TopoDS_Solid> createContactPads(const TopoDS_Shape& device,
                                                       const std::vector<gp_Pnt>& locations,
                                                       double padSize, double padThickness);
    
    // Advanced operations for device modeling
    static TopoDS_Shape createInterfaceBoundary(const TopoDS_Shape& shape1, 
                                               const TopoDS_Shape& shape2, 
                                               double tolerance = DEFAULT_TOLERANCE);
    static std::vector<TopoDS_Face> identifyContactSurfaces(const TopoDS_Shape& device);
    static TopoDS_Shape createEncapsulation(const TopoDS_Shape& device, double thickness);
};

#endif // GEOMETRY_BUILDER_H
