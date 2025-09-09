#ifndef BOUNDARY_MESH_H
#define BOUNDARY_MESH_H

#include <vector>
#include <memory>
#include <string>
#include <array>

// OpenCASCADE includes
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <Poly_Triangulation.hxx>
#include <BRepMesh_IncrementalMesh.hxx>

/**
 * @brief Structure representing a mesh node
 */
struct MeshNode {
    gp_Pnt point;
    int id;
    std::vector<int> elementIds;  // Elements that contain this node
    
    MeshNode(const gp_Pnt& p, int nodeId) : point(p), id(nodeId) {}
};

/**
 * @brief Structure representing a triangular mesh element
 */
struct MeshElement {
    std::array<int, 3> nodeIds;   // Indices of the three nodes
    int id;
    int faceId;                   // ID of the face this element belongs to
    gp_Pnt centroid;
    double area;
    
    MeshElement(const std::array<int, 3>& nodes, int elemId, int face = -1) 
        : nodeIds(nodes), id(elemId), faceId(face), area(0.0) {}
};

/**
 * @brief Structure representing a boundary face in the mesh
 */
struct BoundaryFace {
    TopoDS_Face face;
    std::vector<int> elementIds;  // Elements on this face
    std::string name;
    int id;
    
    BoundaryFace(const TopoDS_Face& f, int faceId, const std::string& faceName = "") 
        : face(f), name(faceName), id(faceId) {}
};

/**
 * @brief Class for managing boundary meshes of semiconductor devices
 */
class BoundaryMesh {
private:
    std::vector<std::unique_ptr<MeshNode>> m_nodes;
    std::vector<std::unique_ptr<MeshElement>> m_elements;
    std::vector<std::unique_ptr<BoundaryFace>> m_faces;
    
    TopoDS_Shape m_shape;
    double m_meshSize;
    double m_minMeshSize;
    double m_maxMeshSize;
    
    // Mesh quality parameters
    double m_minAngle;
    double m_maxAngle;
    double m_avgElementQuality;
    
    // Internal mesh generation
    void generateTriangulation();
    void extractMeshData();
    void calculateElementProperties();
    void buildConnectivity();
    
    // Mesh quality assessment
    double calculateTriangleAngle(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3) const;

public:
    explicit BoundaryMesh(const TopoDS_Shape& shape, double meshSize = 0.1);
    ~BoundaryMesh() = default;
    
    // Mesh generation
    void generate();
    void regenerate(double newMeshSize);
    void refine(const std::vector<gp_Pnt>& refinementPoints, double localSize);
    
    // Adaptive mesh refinement
    void adaptiveMeshRefinement(double qualityThreshold = 0.3);
    void refineAroundPoints(const std::vector<gp_Pnt>& points, double radius, double localSize);
    void refineInterface(const BoundaryMesh& otherMesh, double interfaceSize);
    
    // Mesh access
    const std::vector<std::unique_ptr<MeshNode>>& getNodes() const { return m_nodes; }
    const std::vector<std::unique_ptr<MeshElement>>& getElements() const { return m_elements; }
    const std::vector<std::unique_ptr<BoundaryFace>>& getFaces() const { return m_faces; }
    
    size_t getNodeCount() const { return m_nodes.size(); }
    size_t getElementCount() const { return m_elements.size(); }
    size_t getFaceCount() const { return m_faces.size(); }
    
    // Mesh properties
    double getMeshSize() const { return m_meshSize; }
    double getMinMeshSize() const { return m_minMeshSize; }
    double getMaxMeshSize() const { return m_maxMeshSize; }
    double getAverageElementQuality() const { return m_avgElementQuality; }
    
    // Geometric queries
    MeshNode* findClosestNode(const gp_Pnt& point) const;
    MeshElement* findElementContaining(const gp_Pnt& point) const;
    std::vector<MeshElement*> getElementsOnFace(int faceId) const;
    std::vector<MeshNode*> getNodesOnFace(int faceId) const;
    
    // Mesh quality analysis
    void analyzeMeshQuality();
    std::vector<MeshElement*> getLowQualityElements(double threshold = 0.3) const;
    double calculateMeshVolume() const;
    double calculateMeshSurfaceArea() const;
    double calculateElementQuality(const MeshElement& element) const;
    
    // Export functions
    void exportToVTK(const std::string& filename) const;
    void exportToVTK(const std::string& filename, const std::vector<int>& materialIds, 
                    const std::vector<int>& regionIds, const std::vector<std::string>& layerNames) const;
    void exportToSTL(const std::string& filename) const;
    void exportToGMSH(const std::string& filename) const;
    void exportToOBJ(const std::string& filename) const;
    
    // Import functions
    bool importFromVTK(const std::string& filename);
    bool importFromSTL(const std::string& filename);
    
    // Mesh statistics
    void printMeshStatistics() const;
    std::pair<gp_Pnt, gp_Pnt> getBoundingBox() const;
    
    // Mesh validation
    bool validateMesh() const;
    bool checkMeshConnectivity() const;
    bool checkElementQuality(double minQuality = 0.1) const;
    
    // Smoothing operations
    void smoothMesh(int iterations = 5);
    void laplacianSmoothing();
    void delaunayRefinement();
    
    // Interface detection
    std::vector<MeshElement*> findInterfaceElements(const BoundaryMesh& otherMesh, double tolerance = 1e-6) const;
    std::vector<MeshNode*> findInterfaceNodes(const BoundaryMesh& otherMesh, double tolerance = 1e-6) const;
};

#endif // BOUNDARY_MESH_H
