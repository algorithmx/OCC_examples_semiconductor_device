#include "BoundaryMesh.h"
#include "VTKExporter.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>
#include <limits>

// OpenCASCADE includes
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Poly_Connect.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Vec.hxx>

BoundaryMesh::BoundaryMesh(const TopoDS_Shape& shape, double meshSize)
    : m_shape(shape), m_meshSize(meshSize), m_minMeshSize(meshSize * 0.1), 
      m_maxMeshSize(meshSize * 10.0), m_minAngle(0.0), m_maxAngle(0.0), 
      m_avgElementQuality(0.0) {
}

void BoundaryMesh::generate() {
    try {
        // Clear existing mesh data
        m_nodes.clear();
        m_elements.clear();
        m_faces.clear();
        
        // Generate triangulation
        generateTriangulation();
        
        // Extract mesh data from OpenCASCADE triangulation
        extractMeshData();
        
        // Calculate element properties
        calculateElementProperties();
        
        // Build connectivity information
        buildConnectivity();
        
        // Analyze mesh quality
        analyzeMeshQuality();
        
        std::cout << "Boundary mesh generated: " << getNodeCount() 
                  << " nodes, " << getElementCount() << " elements" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating boundary mesh: " << e.what() << std::endl;
        throw;
    }
}

void BoundaryMesh::generateTriangulation() {
    // Use OpenCASCADE incremental mesh algorithm
    BRepMesh_IncrementalMesh meshAlgo(m_shape, m_meshSize);
    meshAlgo.Perform();
    
    if (!meshAlgo.IsDone()) {
        throw std::runtime_error("Failed to generate triangulation");
    }
}

void BoundaryMesh::extractMeshData() {
    TopExp_Explorer faceExp(m_shape, TopAbs_FACE);
    int faceId = 0;
    int nodeOffset = 0;
    int elementId = 0;
    
    for (; faceExp.More(); faceExp.Next(), faceId++) {
        TopoDS_Face face;
        face = TopoDS::Face(faceExp.Current());
        
        // Create boundary face
        auto boundaryFace = std::make_unique<BoundaryFace>(face, faceId, 
                                                          "Face_" + std::to_string(faceId));
        
        // Get triangulation from face
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        
        if (triangulation.IsNull()) {
            continue;
        }
        
        // Extract nodes
        const TColgp_Array1OfPnt& nodes = triangulation->Nodes();
        for (int i = nodes.Lower(); i <= nodes.Upper(); i++) {
            gp_Pnt point = nodes(i);
            if (!location.IsIdentity()) {
                point.Transform(location.Transformation());
            }
            
            auto meshNode = std::make_unique<MeshNode>(point, nodeOffset + i - nodes.Lower());
            m_nodes.push_back(std::move(meshNode));
        }
        
        // Extract triangles
        const Poly_Array1OfTriangle& triangles = triangulation->Triangles();
        for (int i = triangles.Lower(); i <= triangles.Upper(); i++) {
            const Poly_Triangle& triangle = triangles(i);
            int n1, n2, n3;
            triangle.Get(n1, n2, n3);
            
            // Adjust indices based on node offset
            std::array<int, 3> nodeIds = {
                nodeOffset + n1 - nodes.Lower(),
                nodeOffset + n2 - nodes.Lower(), 
                nodeOffset + n3 - nodes.Lower()
            };
            
            auto meshElement = std::make_unique<MeshElement>(nodeIds, elementId++, faceId);
            boundaryFace->elementIds.push_back(meshElement->id);
            m_elements.push_back(std::move(meshElement));
        }
        
        m_faces.push_back(std::move(boundaryFace));
        nodeOffset += nodes.Upper() - nodes.Lower() + 1;
    }
}

void BoundaryMesh::calculateElementProperties() {
    for (auto& element : m_elements) {
        if (element->nodeIds.size() != 3) continue;
        
        // Get the three vertices of the triangle
        const gp_Pnt& p1 = m_nodes[element->nodeIds[0]]->point;
        const gp_Pnt& p2 = m_nodes[element->nodeIds[1]]->point;
        const gp_Pnt& p3 = m_nodes[element->nodeIds[2]]->point;
        
        // Calculate centroid
        element->centroid = gp_Pnt(
            (p1.X() + p2.X() + p3.X()) / 3.0,
            (p1.Y() + p2.Y() + p3.Y()) / 3.0,
            (p1.Z() + p2.Z() + p3.Z()) / 3.0
        );
        
        // Calculate area using cross product
        gp_Vec v1(p1, p2);
        gp_Vec v2(p1, p3);
        gp_Vec normal = v1.Crossed(v2);
        element->area = 0.5 * normal.Magnitude();
    }
}

void BoundaryMesh::buildConnectivity() {
    // Build node-to-element connectivity
    for (const auto& element : m_elements) {
        for (int nodeId : element->nodeIds) {
            if (nodeId < static_cast<int>(m_nodes.size())) {
                m_nodes[nodeId]->elementIds.push_back(element->id);
            }
        }
    }
}

double BoundaryMesh::calculateElementQuality(const MeshElement& element) const {
    if (element.nodeIds.size() != 3) return 0.0;
    
    const gp_Pnt& p1 = m_nodes[element.nodeIds[0]]->point;
    const gp_Pnt& p2 = m_nodes[element.nodeIds[1]]->point;
    const gp_Pnt& p3 = m_nodes[element.nodeIds[2]]->point;
    
    // Calculate side lengths
    double a = p1.Distance(p2);
    double b = p2.Distance(p3);
    double c = p3.Distance(p1);
    
    // Calculate quality using the ratio of area to perimeter squared
    // This gives a value between 0 (degenerate) and 1 (equilateral)
    double perimeter = a + b + c;
    if (perimeter < 1e-12) return 0.0;
    
    double area = element.area;
    double quality = 4.0 * sqrt(3.0) * area / (perimeter * perimeter);
    
    return std::max(0.0, std::min(1.0, quality));
}

double BoundaryMesh::calculateTriangleAngle(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3) const {
    gp_Vec v1(p2, p1);
    gp_Vec v2(p2, p3);
    
    double dot = v1.Dot(v2);
    double mag1 = v1.Magnitude();
    double mag2 = v2.Magnitude();
    
    if (mag1 < 1e-12 || mag2 < 1e-12) return 0.0;
    
    double cosAngle = dot / (mag1 * mag2);
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
    
    return acos(cosAngle);
}

void BoundaryMesh::regenerate(double newMeshSize) {
    m_meshSize = newMeshSize;
    m_minMeshSize = newMeshSize * 0.1;
    m_maxMeshSize = newMeshSize * 10.0;
    generate();
}

void BoundaryMesh::refine(const std::vector<gp_Pnt>& refinementPoints, double localSize) {
    // This is a simplified implementation
    // In a full implementation, you would use more sophisticated adaptive refinement
    std::cout << "Refining mesh around " << refinementPoints.size() 
              << " points with local size " << localSize << std::endl;
    
    // For now, just regenerate with smaller mesh size
    double oldMeshSize = m_meshSize;
    m_meshSize = std::min(m_meshSize, localSize);
    generate();
    m_meshSize = oldMeshSize;
}

void BoundaryMesh::adaptiveMeshRefinement(double qualityThreshold) {
    std::vector<MeshElement*> lowQualityElements = getLowQualityElements(qualityThreshold);
    
    std::cout << "Found " << lowQualityElements.size() 
              << " low quality elements for refinement" << std::endl;
    
    // Collect refinement points at centroids of low quality elements
    std::vector<gp_Pnt> refinementPoints;
    for (const auto* element : lowQualityElements) {
        refinementPoints.push_back(element->centroid);
    }
    
    if (!refinementPoints.empty()) {
        refine(refinementPoints, m_meshSize * 0.5);
    }
}

void BoundaryMesh::refineAroundPoints(const std::vector<gp_Pnt>& points, double radius, double localSize) {
    std::vector<gp_Pnt> refinementPoints;
    
    for (const auto& point : points) {
        // Find all elements within radius
        for (const auto& element : m_elements) {
            double distance = element->centroid.Distance(point);
            if (distance <= radius) {
                refinementPoints.push_back(element->centroid);
            }
        }
    }
    
    if (!refinementPoints.empty()) {
        refine(refinementPoints, localSize);
    }
}

void BoundaryMesh::refineInterface(const BoundaryMesh& otherMesh, double interfaceSize) {
    // Find interface elements between this mesh and another mesh
    std::vector<MeshElement*> interfaceElements = findInterfaceElements(otherMesh, interfaceSize);
    
    std::vector<gp_Pnt> refinementPoints;
    for (const auto* element : interfaceElements) {
        refinementPoints.push_back(element->centroid);
    }
    
    if (!refinementPoints.empty()) {
        refine(refinementPoints, interfaceSize);
    }
}

MeshNode* BoundaryMesh::findClosestNode(const gp_Pnt& point) const {
    MeshNode* closest = nullptr;
    double minDistance = std::numeric_limits<double>::max();
    
    for (const auto& node : m_nodes) {
        double distance = node->point.Distance(point);
        if (distance < minDistance) {
            minDistance = distance;
            closest = node.get();
        }
    }
    
    return closest;
}

MeshElement* BoundaryMesh::findElementContaining(const gp_Pnt& point) const {
    // Simplified point-in-triangle test
    // In a full implementation, you would use more efficient spatial data structures
    
    for (const auto& element : m_elements) {
        if (element->nodeIds.size() != 3) continue;
        
        const gp_Pnt& p1 = m_nodes[element->nodeIds[0]]->point;
        const gp_Pnt& p2 = m_nodes[element->nodeIds[1]]->point;
        const gp_Pnt& p3 = m_nodes[element->nodeIds[2]]->point;
        
        // Simple distance-based check - point is "in" triangle if close to centroid
        double distance = point.Distance(element->centroid);
        double avgEdgeLength = (p1.Distance(p2) + p2.Distance(p3) + p3.Distance(p1)) / 3.0;
        
        if (distance < avgEdgeLength * 0.5) {
            return element.get();
        }
    }
    
    return nullptr;
}

std::vector<MeshElement*> BoundaryMesh::getElementsOnFace(int faceId) const {
    std::vector<MeshElement*> result;
    
    for (const auto& element : m_elements) {
        if (element->faceId == faceId) {
            result.push_back(element.get());
        }
    }
    
    return result;
}

std::vector<MeshNode*> BoundaryMesh::getNodesOnFace(int faceId) const {
    std::vector<MeshNode*> result;
    std::set<int> addedNodes;
    
    for (const auto& element : m_elements) {
        if (element->faceId == faceId) {
            for (int nodeId : element->nodeIds) {
                if (addedNodes.find(nodeId) == addedNodes.end()) {
                    result.push_back(m_nodes[nodeId].get());
                    addedNodes.insert(nodeId);
                }
            }
        }
    }
    
    return result;
}

void BoundaryMesh::analyzeMeshQuality() {
    if (m_elements.empty()) {
        m_avgElementQuality = 0.0;
        m_minAngle = 0.0;
        m_maxAngle = 0.0;
        return;
    }
    
    double totalQuality = 0.0;
    m_minAngle = M_PI;
    m_maxAngle = 0.0;
    
    for (const auto& element : m_elements) {
        double quality = calculateElementQuality(*element);
        totalQuality += quality;
        
        if (element->nodeIds.size() == 3) {
            const gp_Pnt& p1 = m_nodes[element->nodeIds[0]]->point;
            const gp_Pnt& p2 = m_nodes[element->nodeIds[1]]->point;
            const gp_Pnt& p3 = m_nodes[element->nodeIds[2]]->point;
            
            double angle1 = calculateTriangleAngle(p1, p2, p3);
            double angle2 = calculateTriangleAngle(p2, p3, p1);
            double angle3 = calculateTriangleAngle(p3, p1, p2);
            
            m_minAngle = std::min({m_minAngle, angle1, angle2, angle3});
            m_maxAngle = std::max({m_maxAngle, angle1, angle2, angle3});
        }
    }
    
    m_avgElementQuality = totalQuality / m_elements.size();
}

std::vector<MeshElement*> BoundaryMesh::getLowQualityElements(double threshold) const {
    std::vector<MeshElement*> result;
    
    for (const auto& element : m_elements) {
        double quality = calculateElementQuality(*element);
        if (quality < threshold) {
            result.push_back(element.get());
        }
    }
    
    return result;
}

double BoundaryMesh::calculateMeshVolume() const {
    // For boundary mesh, calculate surface area instead
    return calculateMeshSurfaceArea();
}

double BoundaryMesh::calculateMeshSurfaceArea() const {
    double totalArea = 0.0;
    
    for (const auto& element : m_elements) {
        totalArea += element->area;
    }
    
    return totalArea;
}

void BoundaryMesh::exportToVTK(const std::string& filename) const {
    if (!VTKExporter::exportMesh(*this, filename)) {
        throw std::runtime_error("Failed to export mesh to VTK file: " + filename);
    }
}

void BoundaryMesh::exportToVTK(const std::string& filename, const std::vector<int>& materialIds, 
                              const std::vector<int>& regionIds, const std::vector<std::string>& /* layerNames */) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // Use VTKExporter for basic structure
    VTKExporter::writeVTKHeader(file, "Semiconductor Device Boundary Mesh with Regions");
    VTKExporter::writeVTKPoints(file, *this);
    VTKExporter::writeVTKCells(file, *this);
    
    // Cell types (5 = triangle)
    file << "CELL_TYPES " << m_elements.size() << std::endl;
    for (size_t i = 0; i < m_elements.size(); i++) {
        file << "5" << std::endl;
    }
    
    // Cell data - this is where we add region information
    file << "CELL_DATA " << m_elements.size() << std::endl;
    
    // Material ID data
    if (!materialIds.empty() && materialIds.size() >= m_elements.size()) {
        file << "SCALARS MaterialID int 1" << std::endl;
        file << "LOOKUP_TABLE default" << std::endl;
        for (size_t i = 0; i < m_elements.size(); i++) {
            file << materialIds[i] << std::endl;
        }
        file << std::endl;
    }
    
    // Region ID data
    if (!regionIds.empty() && regionIds.size() >= m_elements.size()) {
        file << "SCALARS RegionID int 1" << std::endl;
        file << "LOOKUP_TABLE default" << std::endl;
        for (size_t i = 0; i < m_elements.size(); i++) {
            file << regionIds[i] << std::endl;
        }
        file << std::endl;
    }
    
    // Face ID data (existing functionality)
    file << "SCALARS FaceID int 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : m_elements) {
        file << element->faceId << std::endl;
    }
    file << std::endl;
    
    // Element quality data
    file << "SCALARS ElementQuality float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : m_elements) {
        double quality = calculateElementQuality(*element);
        file << quality << std::endl;
    }
    file << std::endl;
    
    // Element area data
    file << "SCALARS ElementArea float 1" << std::endl;
    file << "LOOKUP_TABLE default" << std::endl;
    for (const auto& element : m_elements) {
        file << element->area << std::endl;
    }
    
    file.close();
    std::cout << "Exported mesh with region data to VTK file: " << filename << std::endl;
}

void BoundaryMesh::exportToSTL(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    file << "solid BoundaryMesh" << std::endl;
    
    for (const auto& element : m_elements) {
        if (element->nodeIds.size() != 3) continue;
        
        const gp_Pnt& p1 = m_nodes[element->nodeIds[0]]->point;
        const gp_Pnt& p2 = m_nodes[element->nodeIds[1]]->point;
        const gp_Pnt& p3 = m_nodes[element->nodeIds[2]]->point;
        
        // Calculate normal
        gp_Vec v1(p1, p2);
        gp_Vec v2(p1, p3);
        gp_Vec normal = v1.Crossed(v2);
        normal.Normalize();
        
        file << "facet normal " << normal.X() << " " << normal.Y() << " " << normal.Z() << std::endl;
        file << "outer loop" << std::endl;
        file << "vertex " << p1.X() << " " << p1.Y() << " " << p1.Z() << std::endl;
        file << "vertex " << p2.X() << " " << p2.Y() << " " << p2.Z() << std::endl;
        file << "vertex " << p3.X() << " " << p3.Y() << " " << p3.Z() << std::endl;
        file << "endloop" << std::endl;
        file << "endfacet" << std::endl;
    }
    
    file << "endsolid BoundaryMesh" << std::endl;
    file.close();
    std::cout << "Exported mesh to STL file: " << filename << std::endl;
}

void BoundaryMesh::exportToGMSH(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // GMSH format
    file << "$MeshFormat" << std::endl;
    file << "2.2 0 8" << std::endl;
    file << "$EndMeshFormat" << std::endl;
    
    // Nodes
    file << "$Nodes" << std::endl;
    file << m_nodes.size() << std::endl;
    for (const auto& node : m_nodes) {
        file << (node->id + 1) << " " << node->point.X() << " " 
             << node->point.Y() << " " << node->point.Z() << std::endl;
    }
    file << "$EndNodes" << std::endl;
    
    // Elements
    file << "$Elements" << std::endl;
    file << m_elements.size() << std::endl;
    for (const auto& element : m_elements) {
        file << (element->id + 1) << " 2 2 0 " << (element->faceId + 1) << " "
             << (element->nodeIds[0] + 1) << " " << (element->nodeIds[1] + 1) 
             << " " << (element->nodeIds[2] + 1) << std::endl;
    }
    file << "$EndElements" << std::endl;
    
    file.close();
    std::cout << "Exported mesh to GMSH file: " << filename << std::endl;
}

void BoundaryMesh::exportToOBJ(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // Vertices
    for (const auto& node : m_nodes) {
        file << "v " << node->point.X() << " " << node->point.Y() << " " << node->point.Z() << std::endl;
    }
    
    // Faces
    for (const auto& element : m_elements) {
        if (element->nodeIds.size() == 3) {
            file << "f " << (element->nodeIds[0] + 1) << " " 
                 << (element->nodeIds[1] + 1) << " " << (element->nodeIds[2] + 1) << std::endl;
        }
    }
    
    file.close();
    std::cout << "Exported mesh to OBJ file: " << filename << std::endl;
}

bool BoundaryMesh::importFromVTK(const std::string& filename) {
    // Placeholder implementation
    std::cout << "VTK import not implemented yet: " << filename << std::endl;
    return false;
}

bool BoundaryMesh::importFromSTL(const std::string& filename) {
    // Placeholder implementation
    std::cout << "STL import not implemented yet: " << filename << std::endl;
    return false;
}

void BoundaryMesh::printMeshStatistics() const {
    std::cout << "=== Boundary Mesh Statistics ===" << std::endl;
    std::cout << "Nodes: " << getNodeCount() << std::endl;
    std::cout << "Elements: " << getElementCount() << std::endl;
    std::cout << "Faces: " << getFaceCount() << std::endl;
    std::cout << "Mesh Size: " << m_meshSize << std::endl;
    std::cout << "Min Mesh Size: " << m_minMeshSize << std::endl;
    std::cout << "Max Mesh Size: " << m_maxMeshSize << std::endl;
    std::cout << "Average Element Quality: " << m_avgElementQuality << std::endl;
    std::cout << "Min Angle: " << (m_minAngle * 180.0 / M_PI) << " degrees" << std::endl;
    std::cout << "Max Angle: " << (m_maxAngle * 180.0 / M_PI) << " degrees" << std::endl;
    std::cout << "Surface Area: " << calculateMeshSurfaceArea() << std::endl;
    
    auto bbox = getBoundingBox();
    std::cout << "Bounding Box: [" 
              << bbox.first.X() << ", " << bbox.first.Y() << ", " << bbox.first.Z() << "] to ["
              << bbox.second.X() << ", " << bbox.second.Y() << ", " << bbox.second.Z() << "]" 
              << std::endl;
    std::cout << "================================" << std::endl;
}

std::pair<gp_Pnt, gp_Pnt> BoundaryMesh::getBoundingBox() const {
    if (m_nodes.empty()) {
        return {gp_Pnt(0,0,0), gp_Pnt(0,0,0)};
    }
    
    const gp_Pnt& firstPoint = m_nodes[0]->point;
    double minX = firstPoint.X(), maxX = firstPoint.X();
    double minY = firstPoint.Y(), maxY = firstPoint.Y();
    double minZ = firstPoint.Z(), maxZ = firstPoint.Z();
    
    for (const auto& node : m_nodes) {
        const gp_Pnt& p = node->point;
        minX = std::min(minX, p.X()); maxX = std::max(maxX, p.X());
        minY = std::min(minY, p.Y()); maxY = std::max(maxY, p.Y());
        minZ = std::min(minZ, p.Z()); maxZ = std::max(maxZ, p.Z());
    }
    
    return {gp_Pnt(minX, minY, minZ), gp_Pnt(maxX, maxY, maxZ)};
}

bool BoundaryMesh::validateMesh() const {
    if (m_nodes.empty() || m_elements.empty()) {
        return false;
    }
    
    // Check if all element node indices are valid
    for (const auto& element : m_elements) {
        for (int nodeId : element->nodeIds) {
            if (nodeId < 0 || nodeId >= static_cast<int>(m_nodes.size())) {
                return false;
            }
        }
    }
    
    return checkMeshConnectivity() && checkElementQuality();
}

bool BoundaryMesh::checkMeshConnectivity() const {
    // Simplified connectivity check
    for (const auto& node : m_nodes) {
        if (node->elementIds.empty()) {
            std::cerr << "Warning: Orphaned node found (ID: " << node->id << ")" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool BoundaryMesh::checkElementQuality(double minQuality) const {
    for (const auto& element : m_elements) {
        double quality = calculateElementQuality(*element);
        if (quality < minQuality) {
            std::cerr << "Warning: Low quality element found (ID: " << element->id 
                      << ", Quality: " << quality << ")" << std::endl;
            return false;
        }
    }
    
    return true;
}

void BoundaryMesh::smoothMesh(int iterations) {
    std::cout << "Smoothing mesh with " << iterations << " iterations..." << std::endl;
    
    for (int iter = 0; iter < iterations; iter++) {
        laplacianSmoothing();
    }
    
    // Recalculate element properties after smoothing
    calculateElementProperties();
    analyzeMeshQuality();
}

void BoundaryMesh::laplacianSmoothing() {
    // Store new positions
    std::vector<gp_Pnt> newPositions(m_nodes.size());
    
    for (size_t i = 0; i < m_nodes.size(); i++) {
        const auto& node = m_nodes[i];
        gp_Pnt avgPosition(0, 0, 0);
        int neighborCount = 0;
        
        // Find neighboring nodes through shared elements
        for (int elemId : node->elementIds) {
            const auto& element = m_elements[elemId];
            for (int nodeId : element->nodeIds) {
                if (nodeId != node->id && nodeId < static_cast<int>(m_nodes.size())) {
                    const gp_Pnt& neighborPos = m_nodes[nodeId]->point;
                    avgPosition = gp_Pnt(
                        avgPosition.X() + neighborPos.X(),
                        avgPosition.Y() + neighborPos.Y(),
                        avgPosition.Z() + neighborPos.Z()
                    );
                    neighborCount++;
                }
            }
        }
        
        if (neighborCount > 0) {
            newPositions[i] = gp_Pnt(
                avgPosition.X() / neighborCount,
                avgPosition.Y() / neighborCount,
                avgPosition.Z() / neighborCount
            );
        } else {
            newPositions[i] = node->point;
        }
    }
    
    // Update node positions
    for (size_t i = 0; i < m_nodes.size(); i++) {
        m_nodes[i]->point = newPositions[i];
    }
}

void BoundaryMesh::delaunayRefinement() {
    // Placeholder for Delaunay refinement
    std::cout << "Delaunay refinement not implemented yet" << std::endl;
}

std::vector<MeshElement*> BoundaryMesh::findInterfaceElements(const BoundaryMesh& otherMesh, double tolerance) const {
    std::vector<MeshElement*> interfaceElements;
    
    for (const auto& element : m_elements) {
        // Check if element centroid is close to any element in other mesh
        MeshElement* closestElement = otherMesh.findElementContaining(element->centroid);
        if (closestElement) {
            double distance = element->centroid.Distance(closestElement->centroid);
            if (distance <= tolerance) {
                interfaceElements.push_back(element.get());
            }
        }
    }
    
    return interfaceElements;
}

std::vector<MeshNode*> BoundaryMesh::findInterfaceNodes(const BoundaryMesh& otherMesh, double tolerance) const {
    std::vector<MeshNode*> interfaceNodes;
    
    for (const auto& node : m_nodes) {
        MeshNode* closestNode = otherMesh.findClosestNode(node->point);
        if (closestNode) {
            double distance = node->point.Distance(closestNode->point);
            if (distance <= tolerance) {
                interfaceNodes.push_back(node.get());
            }
        }
    }
    
    return interfaceNodes;
}
