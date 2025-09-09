#ifndef VTKEXPORTER_H
#define VTKEXPORTER_H

#include <string>
#include <vector>
#include <fstream>
#include <memory>

// Forward declarations
class SemiconductorDevice;
class BoundaryMesh;
class DeviceLayer;
enum class MaterialType;
enum class DeviceRegion;

/**
 * @brief Utility class for exporting 3D geometry and mesh data to VTK format
 * 
 * The VTKExporter class provides static methods for exporting semiconductor device
 * geometry and mesh data to VTK files. It supports both simple mesh export and
 * enhanced export with region information including material IDs, region IDs,
 * layer indices, element quality metrics, and areas.
 * 
 * VTK (Visualization Toolkit) files can be visualized using ParaView, VisIt,
 * or other scientific visualization tools.
 */
class VTKExporter {
public:
    /**
     * @brief Export a single boundary mesh to VTK format
     * 
     * @param mesh The boundary mesh to export
     * @param filename Output VTK filename
     * @return true if export was successful, false otherwise
     */
    static bool exportMesh(const BoundaryMesh& mesh, const std::string& filename);

    /**
     * @brief Export a boundary mesh with enhanced region information
     * 
     * Exports mesh with additional cell data attributes including:
     * - MaterialID: Integer ID of the material type
     * - RegionID: Integer ID of the device region
     * - LayerIndex: Index of the device layer
     * - ElementQuality: Quality metric for each triangular element
     * - ElementArea: Area of each triangular element
     * 
     * @param mesh The boundary mesh to export
     * @param layer The device layer containing material and region information
     * @param layerIndex Index of this layer in the device
     * @param filename Output VTK filename
     * @return true if export was successful, false otherwise
     */
    static bool exportMeshWithRegions(const BoundaryMesh& mesh, 
                                     const DeviceLayer& layer,
                                     int layerIndex,
                                     const std::string& filename);

    /**
     * @brief Export multiple device layers as a single merged VTK file
     * 
     * Combines all layer meshes into a single VTK file with region information.
     * Each triangle element includes material ID, region ID, layer index,
     * quality metric, and area as cell data attributes.
     * 
     * @param device The semiconductor device containing all layers
     * @param filename Output VTK filename
     * @return true if export was successful, false otherwise
     */
    static bool exportDeviceWithRegions(const SemiconductorDevice& device, 
                                       const std::string& filename);

    /**
     * @brief Convert MaterialType enum to integer ID
     * 
     * @param material Material type enumeration value
     * @return Integer ID for the material type
     */
    static int materialTypeToID(MaterialType material);

    /**
     * @brief Convert DeviceRegion enum to integer ID
     * 
     * @param region Device region enumeration value
     * @return Integer ID for the device region
     */
    static int deviceRegionToID(DeviceRegion region);

    /**
     * @brief Convert MaterialType enum to human-readable name
     * 
     * @param material Material type enumeration value
     * @return String name for the material type
     */
    static std::string materialTypeToName(MaterialType material);

    /**
     * @brief Convert DeviceRegion enum to human-readable name
     * 
     * @param region Device region enumeration value
     * @return String name for the device region
     */
    static std::string deviceRegionToName(DeviceRegion region);

    /**
     * @brief Write VTK file header
     * 
     * @param file Output file stream
     * @param title Title for the VTK file
     */
    static void writeVTKHeader(std::ofstream& file, const std::string& title);

    /**
     * @brief Write mesh points to VTK file
     * 
     * @param file Output file stream
     * @param mesh Boundary mesh containing point data
     */
    static void writeVTKPoints(std::ofstream& file, const BoundaryMesh& mesh);

    /**
     * @brief Write mesh triangular cells to VTK file
     * 
     * @param file Output file stream
     * @param mesh Boundary mesh containing triangle data
     * @param pointOffset Offset to add to point indices (for merged meshes)
     */
    static void writeVTKCells(std::ofstream& file, const BoundaryMesh& mesh, int pointOffset = 0);

private:

    /**
     * @brief Write cell data arrays to VTK file
     * 
     * @param file Output file stream
     * @param mesh Boundary mesh for quality and area calculations
     * @param layer Device layer for material and region information
     * @param layerIndex Index of the layer in the device
     */
    static void writeVTKCellData(std::ofstream& file, 
                                const BoundaryMesh& mesh,
                                const DeviceLayer& layer,
                                int layerIndex);

    /**
     * @brief Calculate triangular element quality metric
     * 
     * Quality metric is computed as 4*sqrt(3)*area / (perimeter^2)
     * Perfect equilateral triangle has quality = 1.0
     * Degenerate triangle has quality approaching 0.0
     * 
     * @param p1 First vertex of triangle
     * @param p2 Second vertex of triangle
     * @param p3 Third vertex of triangle
     * @return Quality metric between 0.0 and 1.0
     */
    static double calculateTriangleQuality(const std::array<double, 3>& p1,
                                         const std::array<double, 3>& p2,
                                         const std::array<double, 3>& p3);

    /**
     * @brief Calculate triangular element area
     * 
     * @param p1 First vertex of triangle
     * @param p2 Second vertex of triangle
     * @param p3 Third vertex of triangle
     * @return Area of the triangle
     */
    static double calculateTriangleArea(const std::array<double, 3>& p1,
                                      const std::array<double, 3>& p2,
                                      const std::array<double, 3>& p3);
};

#endif // VTKEXPORTER_H
