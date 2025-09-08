# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is an OpenCASCADE-based C++ framework for creating and analyzing 3D semiconductor device geometries. It focuses on modeling devices like MOSFETs, diodes, and BJTs with boundary mesh generation for finite element analysis. The project uses OpenCASCADE Community Edition (OCE) for 3D CAD operations and provides a C++17 interface for semiconductor device modeling.

## Common Commands

### Building the Project
```bash
# Build in debug mode (default)
./build.sh

# Build in release mode (optimized)
./build.sh release

# Clean build directory
./build.sh clean

# Install system-wide
./build.sh install
```

### Running Examples
```bash
# Run example applications (after building)
./build.sh examples

# Run specific examples manually
cd build/examples
./basic_shapes_example
./mosfet_example
```

### Manual CMake Build
```bash
# For more control over build process
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Testing and Validation
```bash
# Check OCE installation
pkg-config --cflags --libs oce-foundation

# Verify build outputs
ls build/examples/
ls build/*.a  # Check for libsemiconductor_device.a

# Check generated files after running examples
ls build/examples/*.step build/examples/*.vtk build/examples/*.brep
```

## Architecture Overview

### Core Design Pattern
The framework follows a hierarchical composition pattern:
- **SemiconductorDevice**: Main container orchestrating multiple DeviceLayer objects
- **DeviceLayer**: Individual layers (substrate, oxide, gate, etc.) with associated materials and geometry
- **GeometryBuilder**: Static utility class for creating 3D CAD geometry using OpenCASCADE primitives
- **BoundaryMesh**: Manages triangular boundary mesh generation for finite element analysis

### Key Architectural Components

#### Material System
Materials are defined via `MaterialProperties` struct and `MaterialType` enum. Each DeviceLayer associates a 3D solid with specific material properties (conductivity, permittivity, band gap).

#### Geometry Pipeline
1. Create basic solids using `GeometryBuilder` static methods
2. Wrap solids in `DeviceLayer` objects with material assignments
3. Add layers to `SemiconductorDevice` container
4. Call `buildDeviceGeometry()` to assemble complete device
5. Generate boundary meshes at global or per-layer level

#### Mesh Generation Strategy
- Uses OpenCASCADE's built-in triangulation capabilities
- Supports adaptive refinement around specific points
- Provides both global device meshing and individual layer meshing
- Mesh sizes specified in meters (typically micrometers: 1e-6)

### File Organization
- `include/`: Public headers defining the main API classes
- `src/`: Implementation files for all classes
- `examples/`: Demonstration programs showing usage patterns
- `build/`: Generated build artifacts and executables
- `CMakeLists.txt`: Main build configuration
- `examples/CMakeLists.txt`: Example build targets

## Key Dependencies

### Required System Packages
```bash
# Install OCE (OpenCASCADE Community Edition) development libraries
sudo apt install liboce-foundation-dev liboce-modeling-dev \
                 liboce-ocaf-dev liboce-visualization-dev \
                 build-essential cmake pkg-config
```

### OpenCASCADE Libraries Used
The project links against specific OCE libraries: TKernel, TKMath, TKG2d, TKG3d, TKGeomBase, TKBRep, TKGeomAlgo, TKTopAlgo, TKPrim, TKBO, TKMesh, TKShHealing, TKBool, TKFillet, TKOffset, TKService, TKSTL, TKSTEPBase, TKSTEP, TKIGES, TKBinL, TKXmlL, TKCAF.

## Development Patterns

### Device Creation Workflow
```cpp
// 1. Create device container
SemiconductorDevice device("DeviceName");

// 2. Define materials
MaterialProperties silicon(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon");

// 3. Create 3D geometry
TopoDS_Solid substrate = GeometryBuilder::createBox(gp_Pnt(0,0,0), Dimensions3D(2.0, 2.0, 0.5));

// 4. Create device layer
auto layer = std::make_unique<DeviceLayer>(substrate, silicon, DeviceRegion::Substrate, "Substrate");

// 5. Add to device and build
device.addLayer(std::move(layer));
device.buildDeviceGeometry();

// 6. Generate mesh and export
device.generateGlobalBoundaryMesh(0.1);  // 0.1m mesh size
device.exportGeometry("device.step", "STEP");
device.exportMesh("device.vtk", "VTK");
```

### Boolean Operations Pattern
All boolean operations go through `GeometryBuilder` static methods:
- `GeometryBuilder::unionShapes(shape1, shape2)`
- `GeometryBuilder::intersectShapes(shape1, shape2)` 
- `GeometryBuilder::subtractShapes(shape1, shape2)`

### Mesh Size Considerations
- Use meter units with typical micrometers (1e-6)
- Substrate layers: 0.1e-6 to 1e-6 m mesh size
- Oxide layers: 0.01e-6 to 0.05e-6 m (finer mesh)
- Gate contacts: 0.05e-6 to 0.1e-6 m mesh size

### Export Format Support
- **Geometry**: STEP (.step), IGES (.iges), BREP (.brep), STL (.stl)
- **Mesh**: VTK (.vtk), GMSH (.msh), STL (.stl), OBJ (.obj)
- **Primary formats**: STEP for geometry exchange, VTK for visualization

## Error Handling and Debugging

### Common Build Issues
- **OCE not found**: Install OCE development packages as shown above
- **CMake version**: Requires CMake 3.16+
- **Runtime OpenCASCADE errors**: Usually indicate invalid geometry parameters (negative dimensions, inappropriate mesh sizes)

### Debugging Techniques
- Enable verbose compilation: `cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..`
- Check geometry validity: `device.validateGeometry()` and `device.validateMesh()`
- Use device info: `device.printDeviceInfo()` for layer and mesh statistics

### Typical Parameter Ranges
- **Dimensions**: Micrometers (1e-6 m) to millimeters (1e-3 m)
- **Mesh sizes**: 10 nanometers (0.01e-6 m) to 1 micrometer (1e-6 m)
- **Material conductivity**: 1e-16 S/m (insulators) to 1e7 S/m (metals)

## Code Standards

### Memory Management
- Uses smart pointers (`std::unique_ptr`) for object ownership
- DeviceLayer objects owned by SemiconductorDevice via `addLayer(std::move(layer))`
- OpenCASCADE objects use value semantics (TopoDS_Shape, gp_Pnt, etc.)

### Exception Handling
- OpenCASCADE operations can throw `Standard_DomainError` for invalid inputs
- All example code wrapped in try-catch blocks
- Use `validateGeometry()` and `validateMesh()` before operations

### Coordinate System
- Uses standard 3D Cartesian coordinates (X, Y, Z)
- Units in meters throughout the API
- Origin typically at (0, 0, 0) for substrate base corner
