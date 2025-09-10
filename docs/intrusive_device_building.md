# Intrusive Device Building System Design

## Overview

This document describes the design and implementation strategy for an **intrusive device building system** that enables realistic semiconductor device modeling through hierarchical boolean composition. Unlike the current non-intrusive approach where building blocks don't intersect, this system allows elementary blocks to overlap, with conflicts resolved through a precedence-based ranking system.

## Problem Statement

### Current Limitations
- Existing framework uses non-intrusive composition (blocks may touch but not intersect)
- Real semiconductor devices often require intrusive geometry where layers cut through each other
- Need for hierarchical precedence when multiple layers occupy the same 3D space

### Requirements
1. Each elementary building block has a "rank number" (precedence level)
2. When blocks intersect, higher-ranked block remains intact
3. Lower-ranked block has intersection volume removed
4. When ranks are equal, larger region index takes precedence
5. Must maintain material properties and mesh compatibility
6. Always preserve an immutable original geometry per block to support fast, numerically stable recomputation after any repositioning (transform) without cumulative boolean/precision degradation.

## Technical Architecture

### Core Data Structures

#### RankedDeviceLayer
```cpp
struct RankedDeviceLayer {
    std::unique_ptr<DeviceLayer> layer;    // Original device layer
    int rank;                              // Higher rank = higher precedence
    int region_index;                      // Tiebreaker when ranks equal

    // Geometry state
    TopoDS_Solid original_shape;           // Immutable: canonical geometry at creation time
    gp_Trsf       current_trsf;            // Current pose relative to original (identity by default)
    TopoDS_Solid  transformed_shape;       // original_shape after applying current_trsf (no cuts)
    TopoDS_Solid  final_shape;             // Geometry after precedence resolution (cuts from higher precedence)

    bool is_modified;                      // Track if shape was altered by cuts
    std::vector<int> cut_by_ranks;         // Track which ranks cut this layer
};
```

#### IntrusiveDeviceBuilder
```cpp
class IntrusiveDeviceBuilder {
private:
    std::vector<RankedDeviceLayer> ranked_layers_;
    double geometric_tolerance_;
    bool enable_spatial_optimization_;
    
public:
    // Construction and configuration
    IntrusiveDeviceBuilder(double tolerance = 1e-7);
    void setGeometricTolerance(double tolerance);
    void enableSpatialOptimization(bool enable);
    
    // Layer management
    void addRankedLayer(std::unique_ptr<DeviceLayer> layer, 
                       int rank, int region_index);
    void clearLayers();
    size_t getLayerCount() const;

    // Transform and update
    void updateLayerTransform(size_t layer_index, const gp_Trsf& trsf); // set pose then recompute incrementally
    void resetLayerToOriginal(size_t layer_index);                      // clear transform and cuts
    void recomputeFromOriginals(const std::vector<size_t>& changed_indices); // fast partial recompute
    
    // Core processing
    void resolveIntersections();               // full recompute from originals and current transforms
    SemiconductorDevice buildDevice(const std::string& device_name);
    
    // Validation and diagnostics
    bool validateAllGeometry() const;
    void printResolutionReport() const;
    std::vector<std::pair<int, int>> getIntersectionPairs() const;
    
private:
    // Internal processing methods
    void sortLayersByPrecedence();
    void applyCurrentTransforms();             // original_shape -> transformed_shape
    bool checkIntersection(const TopoDS_Solid& shape1, const TopoDS_Solid& shape2);
    TopoDS_Solid performPrecedenceCut(const TopoDS_Solid& base, 
                                     const TopoDS_Solid& tool);
    void validateAndRepairGeometry(TopoDS_Solid& shape);
    double calculateIntersectionVolume(const TopoDS_Solid& shape1, 
                                      const TopoDS_Solid& shape2);
    
    // Optimization methods
    std::vector<std::pair<size_t, size_t>> findPotentialIntersections();
    bool boundingBoxesIntersect(const TopoDS_Solid& shape1, 
                               const TopoDS_Solid& shape2);
};
```

## Required OpenCASCADE Operations

### Primary Boolean Operations

#### 1. BRepAlgoAPI_Cut (Subtraction)
- **Purpose**: Remove intersection volumes from lower-precedence shapes
- **Usage**: `BRepAlgoAPI_Cut(shape_to_modify, shape_to_subtract)`
- **Critical for**: Core precedence resolution algorithm

#### 2. BRepAlgoAPI_Common (Intersection)
- **Purpose**: Detect and extract overlapping volumes
- **Usage**: `BRepAlgoAPI_Common(shape1, shape2)`
- **Critical for**: Intersection detection and volume calculation

#### 3. BRepAlgoAPI_Fuse (Union)
- **Purpose**: Final assembly of non-conflicting shapes
- **Usage**: `BRepAlgoAPI_Fuse(shape1, shape2)`
- **Critical for**: Device assembly after precedence resolution

### Supporting Operations

#### Geometry Validation
- **BRepCheck_Analyzer**: Validate geometry integrity after operations
- **ShapeFix_Shape**: Automatic geometry repair for invalid results
- **BRepBuilderAPI_MakeShape**: Shape construction and modification

#### Geometric Analysis
- **TopExp_Explorer**: Traverse shape components (faces, edges, vertices)
- **GProp_GProps**: Calculate geometric properties (volume, surface area)
- **BRep_Tool**: Extract geometric information from topological entities

#### Precision Management
- **Precision::Confusion()**: Standard geometric tolerance
- **BRepBuilderAPI_MakeFace**: Control face creation precision
- **BRepLib::Precision()**: Library-wide precision settings

## Implementation Algorithm

### Phase 1: Preparation and Sorting
```cpp
void IntrusiveDeviceBuilder::resolveIntersections() {
    // 1. Sort layers by precedence (rank desc, then region_index desc)
    sortLayersByPrecedence();
    
    // 2. Apply current transforms to produce transformed (uncut) shapes from immutable originals
    applyCurrentTransforms();

    // 3. Initialize final_shape = transformed_shape for all layers (cuts will be applied to these)
    for (auto& ranked_layer : ranked_layers_) {
        ranked_layer.final_shape = ranked_layer.transformed_shape;
        ranked_layer.is_modified = false;
        ranked_layer.cut_by_ranks.clear();
    }
    
    // 4. Proceed to hierarchical resolution
    performHierarchicalResolution();
}
```

### Phase 2: Hierarchical Boolean Resolution
```cpp
void IntrusiveDeviceBuilder::performHierarchicalResolution() {
    for (size_t i = 0; i < ranked_layers_.size(); ++i) {
        auto& current_layer = ranked_layers_[i];  // Higher precedence
        
        for (size_t j = i + 1; j < ranked_layers_.size(); ++j) {
            auto& lower_layer = ranked_layers_[j];  // Lower precedence
            
            // Check for intersection
            if (checkIntersection(current_layer.final_shape, 
                                lower_layer.final_shape)) {
                
                // Perform precedence-based cut
                TopoDS_Solid modified_shape = performPrecedenceCut(
                    lower_layer.final_shape,
                    current_layer.final_shape
                );
                
                // Validate and update
                validateAndRepairGeometry(modified_shape);
                lower_layer.final_shape = modified_shape;
                lower_layer.is_modified = true;
                lower_layer.cut_by_ranks.push_back(current_layer.rank);
            }
        }
    }
}
```

### Phase 3: Device Assembly
```cpp
SemiconductorDevice IntrusiveDeviceBuilder::buildDevice(const std::string& device_name) {
    SemiconductorDevice device(device_name);
    
    for (const auto& ranked_layer : ranked_layers_) {
        // Create new DeviceLayer with modified geometry
        auto modified_layer = std::make_unique<DeviceLayer>(
            ranked_layer.final_shape,
            ranked_layer.layer->getMaterial(),
            ranked_layer.layer->getRegion(),
            ranked_layer.layer->getName()
        );
        
        device.addLayer(std::move(modified_layer));
    }
    
    device.buildDeviceGeometry();
    return device;
}
```

## Repositioning and Recalculation Workflow

To support interactive adjustments (translations, rotations, scaling) without accumulating boolean artifacts, the system follows these principles:

- Keep `original_shape` immutable for every block.
- Store a `current_trsf` per block that represents its pose.
- Build `transformed_shape = BRepBuilderAPI_Transform(original_shape, current_trsf).Shape()`.
- Recompute `final_shape` by applying precedence-based cuts to `transformed_shape` only, never to `original_shape`.
- On any transform change, rebuild only affected pairs using bounding-box pre-screening.

### Typical Update Flow
```cpp
// 1) Update the transform
deviceBuilder.updateLayerTransform(layer_index, trsf);

// 2) Incremental recompute from originals for impacted layers
deviceBuilder.recomputeFromOriginals({layer_index});

// 3) (Optional) Validate
assert(deviceBuilder.validateAllGeometry());
```

### Key Implementation Methods

#### Transform Application
```cpp
void IntrusiveDeviceBuilder::applyCurrentTransforms() {
    for (auto& ranked_layer : ranked_layers_) {
        if (ranked_layer.current_trsf.Form() == gp_Identity) {
            // No transform - direct copy
            ranked_layer.transformed_shape = ranked_layer.original_shape;
        } else {
            // Apply transform to immutable original
            ranked_layer.transformed_shape = GeometryBuilder::transformShape(
                ranked_layer.original_shape, 
                ranked_layer.current_trsf
            );
        }
    }
}
```

#### Incremental Recomputation
```cpp
void IntrusiveDeviceBuilder::recomputeFromOriginals(const std::vector<size_t>& changed_indices) {
    // 1. Update transformed shapes for changed layers
    for (size_t idx : changed_indices) {
        auto& layer = ranked_layers_[idx];
        layer.transformed_shape = GeometryBuilder::transformShape(
            layer.original_shape, layer.current_trsf
        );
        layer.final_shape = layer.transformed_shape;
        layer.is_modified = false;
        layer.cut_by_ranks.clear();
    }
    
    // 2. Identify all layers that need recomputation (those that intersect with changed layers)
    std::set<size_t> layers_to_recompute;
    for (size_t changed_idx : changed_indices) {
        layers_to_recompute.insert(changed_idx);
        
        // Find all layers that might intersect with this changed layer
        for (size_t i = 0; i < ranked_layers_.size(); ++i) {
            if (i != changed_idx && 
                boundingBoxesIntersect(ranked_layers_[changed_idx].transformed_shape,
                                     ranked_layers_[i].transformed_shape)) {
                layers_to_recompute.insert(i);
            }
        }
    }
    
    // 3. Recompute precedence resolution for affected layers only
    std::vector<size_t> sorted_indices(layers_to_recompute.begin(), layers_to_recompute.end());
    std::sort(sorted_indices.begin(), sorted_indices.end(), 
              [this](size_t a, size_t b) {
                  const auto& layer_a = ranked_layers_[a];
                  const auto& layer_b = ranked_layers_[b];
                  if (layer_a.rank != layer_b.rank) return layer_a.rank > layer_b.rank;
                  return layer_a.region_index > layer_b.region_index;
              });
    
    // Apply cuts between affected layers
    performSelectiveResolution(sorted_indices);
}
```

#### Layer Transform Update
```cpp
void IntrusiveDeviceBuilder::updateLayerTransform(size_t layer_index, const gp_Trsf& trsf) {
    if (layer_index >= ranked_layers_.size()) {
        throw std::out_of_range("Layer index out of range");
    }
    
    auto& layer = ranked_layers_[layer_index];
    layer.current_trsf = trsf;
    
    // Immediately update transformed shape
    layer.transformed_shape = GeometryBuilder::transformShape(
        layer.original_shape, layer.current_trsf
    );
}
```

## Enhanced GeometryBuilder Methods

### New Static Methods for Intrusive Operations

```cpp
class GeometryBuilder {
public:
    // Existing methods remain unchanged...
    
    // New intrusive operation methods
    static TopoDS_Solid intrusiveCut(const TopoDS_Solid& base, 
                                    const TopoDS_Solid& tool,
                                    double tolerance = 1e-7);
    
    static bool hasVolumeIntersection(const TopoDS_Solid& shape1, 
                                     const TopoDS_Solid& shape2);
    
    static double calculateIntersectionVolume(const TopoDS_Solid& shape1, 
                                            const TopoDS_Solid& shape2);
    
    static TopoDS_Solid repairGeometry(const TopoDS_Solid& shape);
    
    static std::pair<gp_Pnt, gp_Pnt> getBoundingBox(const TopoDS_Solid& shape);
    
    static bool boundingBoxesIntersect(const TopoDS_Solid& shape1, 
                                      const TopoDS_Solid& shape2,
                                      double tolerance = 1e-7);
    
    // Transform operations
    static TopoDS_Solid transformShape(const TopoDS_Solid& shape, 
                                       const gp_Trsf& transform);
    
    static gp_Trsf combineTransforms(const gp_Trsf& trsf1, 
                                     const gp_Trsf& trsf2);
};
```

## Usage Examples

### Basic Intrusive Device Construction

```cpp
// Create intrusive device builder
IntrusiveDeviceBuilder builder;

// Define materials
MaterialProperties silicon(MaterialType::Silicon, 1.0e-4, 11.7 * 8.854e-12, 1.12, "Silicon");
MaterialProperties oxide(MaterialType::SiliconDioxide, 1.0e-12, 3.9 * 8.854e-12, 9.0, "SiO2");
MaterialProperties polysilicon(MaterialType::PolySilicon, 1.0e3, 11.7 * 8.854e-12, 1.12, "Poly-Si");

// Create substrate (lowest rank - gets cut by everything)
TopoDS_Solid substrate_solid = GeometryBuilder::createBox(
    gp_Pnt(0, 0, 0), Dimensions3D(10e-6, 10e-6, 2e-6)
);
auto substrate_layer = std::make_unique<DeviceLayer>(
    substrate_solid, silicon, DeviceRegion::Substrate, "Substrate"
);
builder.addRankedLayer(std::move(substrate_layer), 1, 0);

// Create gate oxide (medium rank)
TopoDS_Solid oxide_solid = GeometryBuilder::createBox(
    gp_Pnt(2e-6, 0, 1.8e-6), Dimensions3D(6e-6, 10e-6, 0.1e-6)
);
auto oxide_layer = std::make_unique<DeviceLayer>(
    oxide_solid, oxide, DeviceRegion::Oxide, "Gate_Oxide"
);
builder.addRankedLayer(std::move(oxide_layer), 2, 1);

// Create gate contact (highest rank - cuts through everything)
TopoDS_Solid gate_solid = GeometryBuilder::createBox(
    gp_Pnt(3e-6, 0, 1.8e-6), Dimensions3D(4e-6, 10e-6, 0.5e-6)
);
auto gate_layer = std::make_unique<DeviceLayer>(
    gate_solid, polysilicon, DeviceRegion::Gate, "Gate_Contact"
);
builder.addRankedLayer(std::move(gate_layer), 3, 2);

// Resolve intersections and build device
builder.resolveIntersections();
SemiconductorDevice device = builder.buildDevice("MOSFET_Intrusive");

// Generate mesh and export
device.generateGlobalBoundaryMesh(0.1e-6);
device.exportGeometry("mosfet_intrusive.step", "STEP");
device.exportMesh("mosfet_intrusive.vtk", "VTK");
```

### Complex Multi-Layer Device

```cpp
IntrusiveDeviceBuilder builder;

// Substrate (rank 1 - lowest precedence)
builder.addRankedLayer(createSubstrate(), 1, 0);

// Well regions (rank 2)
builder.addRankedLayer(createNWell(), 2, 1);
builder.addRankedLayer(createPWell(), 2, 2);

// Shallow trench isolation (rank 3)
builder.addRankedLayer(createSTI(), 3, 3);

// Gate stack layers (rank 4-6)
builder.addRankedLayer(createGateOxide(), 4, 4);
builder.addRankedLayer(createGatePoly(), 5, 5);
builder.addRankedLayer(createGateCap(), 6, 6);

// Source/Drain implants (rank 7)
builder.addRankedLayer(createSourceDrain(), 7, 7);

// Contacts and interconnect (rank 8-10)
builder.addRankedLayer(createContacts(), 8, 8);
builder.addRankedLayer(createMetal1(), 9, 9);
builder.addRankedLayer(createVias(), 10, 10);

// Process with validation
builder.resolveIntersections();
if (!builder.validateAllGeometry()) {
    std::cerr << "Geometry validation failed!" << std::endl;
    builder.printResolutionReport();
    return;
}

SemiconductorDevice complex_device = builder.buildDevice("Complex_MOSFET");
```

### Transform-Aware Interactive Design

```cpp
IntrusiveDeviceBuilder builder;

// Initial device setup
builder.addRankedLayer(createSubstrate(), 1, 0);     // substrate
builder.addRankedLayer(createGateOxide(), 2, 1);    // gate oxide
builder.addRankedLayer(createGatePoly(), 3, 2);     // gate poly
builder.addRankedLayer(createSourceDrain(), 4, 3);  // source/drain

// Initial resolution
builder.resolveIntersections();
SemiconductorDevice initial_device = builder.buildDevice("Initial_MOSFET");
initial_device.exportGeometry("mosfet_v1.step", "STEP");

// Interactive adjustments - move gate poly by 0.5 micrometers in X
gp_Trsf gate_translation;
gate_translation.SetTranslation(gp_Vec(0.5e-6, 0, 0));
builder.updateLayerTransform(2, gate_translation);  // layer index 2 = gate poly

// Fast incremental recompute (only affected intersections recalculated)
builder.recomputeFromOriginals({2});  // only recompute layer 2 and its intersections

SemiconductorDevice adjusted_device = builder.buildDevice("Adjusted_MOSFET");
adjusted_device.exportGeometry("mosfet_v2.step", "STEP");

// Further adjustment - rotate source/drain region
gp_Trsf source_rotation;
source_rotation.SetRotation(gp_Ax1(gp_Pnt(5e-6, 5e-6, 0), gp_Dir(0, 0, 1)), M_PI/12); // 15 degrees
builder.updateLayerTransform(3, source_rotation);

// Again, fast incremental recompute
builder.recomputeFromOriginals({3});

SemiconductorDevice final_device = builder.buildDevice("Final_MOSFET");
final_device.exportGeometry("mosfet_final.step", "STEP");

// Reset specific layer to original position if needed
builder.resetLayerToOriginal(2);  // reset gate poly to original position
builder.recomputeFromOriginals({2});
```

## Error Handling and Validation

### Geometric Validation Strategy

```cpp
bool IntrusiveDeviceBuilder::validateAllGeometry() const {
    for (const auto& ranked_layer : ranked_layers_) {
        BRepCheck_Analyzer analyzer(ranked_layer.final_shape);
        if (!analyzer.IsValid()) {
            std::cerr << "Invalid geometry in layer: " << ranked_layer.layer->getName() 
                      << " (rank " << ranked_layer.rank << ")" << std::endl;
            return false;
        }
        
        // Check for minimum volume threshold
        GProp_GProps volume_props;
        BRepGProp::VolumeProperties(ranked_layer.final_shape, volume_props);
        if (volume_props.Mass() < geometric_tolerance_) {
            std::cerr << "Layer volume too small after cutting: " 
                      << ranked_layer.layer->getName() << std::endl;
            return false;
        }
    }
    return true;
}
```

### Automatic Geometry Repair

```cpp
TopoDS_Solid GeometryBuilder::repairGeometry(const TopoDS_Solid& shape) {
    ShapeFix_Shape shape_fixer;
    shape_fixer.Init(shape);
    shape_fixer.SetPrecision(Precision::Confusion());
    shape_fixer.SetMinTolerance(Precision::Confusion());
    shape_fixer.SetMaxTolerance(1.0);
    
    if (shape_fixer.Perform()) {
        return TopoDS::Solid(shape_fixer.Shape());
    }
    
    return shape; // Return original if repair fails
}
```

## Benefits of Immutable Originals Approach

### Numerical Stability
- **No Cumulative Precision Loss**: Each recomputation starts from original geometry, preventing precision degradation from repeated boolean operations
- **Consistent Results**: Multiple adjustments yield identical results to single-step transformations
- **Robust Boolean Operations**: Original shapes maintain optimal numerical properties for OpenCASCADE operations

### Performance Advantages
- **Incremental Updates**: Only affected layer pairs need recomputation after transforms
- **Spatial Optimization**: Bounding box pre-screening eliminates unnecessary intersection tests
- **Memory Efficiency**: Immutable originals can be shared/referenced without copying
- **Parallel Processing**: Independent layer transformations can be parallelized

### Design Flexibility
- **Interactive Design**: Real-time geometry adjustments without rebuilding entire device
- **Parameter Sweeps**: Efficient exploration of design space by varying transforms
- **Undo/Redo Support**: Easy restoration to previous states via transform history
- **Design Optimization**: Gradient-based optimization can efficiently evaluate transform derivatives

### Debugging and Validation
- **Reproducible Results**: Deterministic geometry generation independent of adjustment history
- **Easy Comparison**: Original vs. final geometries clearly separated for analysis
- **Selective Validation**: Can validate individual layers before and after precedence resolution
- **Error Isolation**: Transform errors isolated from boolean operation errors

## Performance Considerations

### Spatial Optimization
- **Bounding Box Pre-screening**: Check bounding box intersections before expensive boolean operations
- **Spatial Indexing**: Implement octree or R-tree for large numbers of layers
- **Batch Processing**: Group layers by spatial proximity for parallel processing

### Memory Management
- **Progressive Processing**: Process layers in batches to reduce peak memory usage
- **Shape Caching**: Cache intermediate results for repeated operations
- **Garbage Collection**: Explicitly release OpenCASCADE handles after operations

### Numerical Stability
- **Adaptive Tolerance**: Adjust geometric tolerance based on feature sizes
- **Precision Scaling**: Scale coordinates to avoid numerical precision issues
- **Robust Boolean Operations**: Use `BRepAlgoAPI_BooleanOperation` with proper settings

## Integration with Existing Framework

### Backward Compatibility
- Existing non-intrusive device building remains unchanged
- `IntrusiveDeviceBuilder` operates alongside current `SemiconductorDevice` workflow
- Same export and mesh generation capabilities

### Migration Path
1. **Phase 1**: Implement `IntrusiveDeviceBuilder` as separate class
2. **Phase 2**: Add intrusive building methods to existing `SemiconductorDevice`
3. **Phase 3**: Unify APIs with builder pattern selection

### Configuration Options
```cpp
// In CMakeLists.txt or configuration header
#define ENABLE_INTRUSIVE_BUILDING 1
#define DEFAULT_GEOMETRIC_TOLERANCE 1e-7
#define ENABLE_SPATIAL_OPTIMIZATION 1
#define MAX_INTERSECTION_CACHE_SIZE 1000
```

## Testing Strategy

### Unit Tests
- Individual boolean operation correctness
- Precedence resolution algorithm validation
- Geometric tolerance handling
- Error recovery and repair mechanisms

### Integration Tests
- Complete device building workflows
- Complex multi-layer scenarios
- Performance benchmarks with varying layer counts
- Memory usage profiling

### Validation Tests
- Comparison with analytical solutions for simple geometries
- Mesh quality assessment after intrusive operations
- Material property preservation verification
- Export format compatibility testing

## Future Extensions

### Advanced Features
- **Curved Interface Handling**: Support for non-planar layer interfaces
- **Adaptive Meshing**: Mesh refinement around cut boundaries
- **Process Simulation**: Time-based layer deposition simulation
- **Parallel Processing**: Multi-threaded intersection resolution

### Integration Possibilities
- **CAD Import**: Direct import from process simulation tools
- **Design Rule Checking**: Automatic validation of device rules
- **Parameter Optimization**: Automated device geometry optimization
- **Multi-Physics**: Coupling with electrical/thermal simulation

## Conclusion

This intrusive device building system provides a robust foundation for realistic semiconductor device modeling while maintaining compatibility with the existing OpenCASCADE-based framework. The hierarchical precedence system enables complex device geometries while preserving geometric integrity and material properties throughout the composition process.

The implementation strategy balances computational efficiency with geometric robustness, providing extensive validation and error handling capabilities essential for reliable semiconductor device simulation workflows.
