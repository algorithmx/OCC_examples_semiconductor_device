# Final Calibrated Design: Intrusive Device Building System

This document fuses the original intrusive builder design (`docs/intrusive_device_building.md`) with the critical review (`docs/design_analysis_and_improvements.md`) and applies the user's chosen runtime defaults.

High-level plan
- Lock in choices you provided and apply them as defaults in this design.
- Produce a concise, actionable design: data shapes, algorithms, APIs, error modes, edge cases, tests, and implementation notes.
- Offer next steps and an implementation plan (stubs or code) you can ask me to create.

Requirements checklist (kept visible)
- Memory optimization (lazy evaluation, shape sharing, cache eviction)
- Transform validation and sanitization
- Scalable intersection detection (use OpenCASCADE built-in spatial indexing)
- Concurrency safety and limited parallelism
- Degenerate-case detection and repair with min-volume policy
- Dependency tracking and incremental updates
- Adaptive precision and intersection caching
- Fluent builder API and structured error reporting
- Unit tests and a small benchmark harness (scaffolded)

Your chosen defaults (applied)
1) Spatial index: OpenCASCADE built-in bounding-box / BVH capabilities (use BRepBndLib / Bnd_Box + OCCT BVH/tree) as the spatial index backend.
2) Geometric tolerance: 1e-7
3) Min volume threshold: 1e-14
4) Concurrency level: 4 threads (default)
5) Intersection cache size: 1000 entries (default)
6) Enable shape sharing: yes (reference-counted/shared handles)

Contract (inputs / outputs / error modes)
- Inputs: vector of ranked layers, each with immutable `original_shape`, rank, region_index and optional transform.
- Outputs: `SemiconductorDevice` assembled from final per-layer solids; `ValidationReport` summarizing issues and metrics.
- Error modes: invalid transform (reject + report); boolean operation failure (retry with adaptive precision/repair, else record failure); degenerate result below volume threshold (mark removed and report)

Core data structures (concrete)
- RankedDeviceLayer (C++ sketch)
  - std::shared_ptr<TopoDS_Shape> original_shape; // shared, immutable canonical geometry
  - int rank;
  - int region_index;
  - gp_Trsf current_trsf; // pose
  - mutable std::optional<TopoDS_Solid> transformed_cache; // lazy
  - mutable std::uint64_t transformed_cache_hash; // quick invalidation key
  - TopoDS_Solid final_shape; // result after precedence resolution; empty if removed
  - double last_volume = 0.0; // last computed volume (0 if none)
  - bool is_modified = false; // whether cuts altered this layer
  - std::vector<int> cut_by_ranks; // history
  - Bnd_Box cached_bbox; // computed lazily via BRepBndLib::Add

- IntrusiveDeviceBuilder (public methods)
  - IntrusiveDeviceBuilder(double tolerance = 1e-7);
  - IntrusiveDeviceBuilder& withTolerance(double t);
  - IntrusiveDeviceBuilder& setMinVolumeThreshold(double v);
  - IntrusiveDeviceBuilder& setSpatialIndexType(/* internal enum */);
  - IntrusiveDeviceBuilder& setMaxThreads(size_t n);
  - IntrusiveDeviceBuilder& withCacheSize(size_t n);
  - IntrusiveDeviceBuilder& enableShapeSharing(bool);
  - void addRankedLayer(std::unique_ptr<DeviceLayer> layer, int rank, int region_index);
  - void updateLayerTransform(size_t layer_index, const gp_Trsf& trsf);
  - void resetLayerToOriginal(size_t layer_index);
  - void recomputeFromOriginals(const std::vector<size_t>& changed_indices);
  - void resolveIntersections();
  - SemiconductorDevice buildDevice(const std::string& name);
  - ValidationReport getLastValidationReport() const;

Supporting subsystems
- TransformValidator
  - bool isValidTransform(const gp_Trsf& trsf, double tol = 1e-7);
  - gp_Trsf sanitizeTransform(const gp_Trsf& trsf);
  - bool causesNumericalInstability(const gp_Trsf& trsf);
  - Implementation notes: check matrix conditioning, scale factors (avoid zero or extreme scales), clamp where needed.

- SpatialIndex (OCCT-backed)
  - Abstraction: ISpatialIndex { insert(layer_index, Bnd_Box), update(...), query(Bnd_Box) -> vector<size_t>, remove(id) }
  - Default implementation: OCASTSpatialIndex that uses BRepBndLib::Add to build Bnd_Box per shape; store boxes in a lightweight BVH/tree (use OpenCASCADE's BVH or an R-tree overlay if helpful). Query returns candidates whose boxes intersect.
  - Rationale: Using OpenCASCADE box computation keeps data consistent and avoids double-precision rescaling surprises.

- DependencyGraph
  - For each layer: sets cuts_applied_by and cuts_applied_to
  - Methods: addDependency(cutter, target), removeDependency(...), getAffectedLayers(changed_layer)
  - Use this to compute minimal recompute set on transform updates.

- IntersectionCache (LRU)
  - Key: pair(layerA, layerB) + hash(trsfA) + hash(trsfB) + optional precision level
  - Value: TopoDS_Solid common/intersection or cut-result and timestamp
  - LRU eviction size default 1000
  - Invalidation: when a layer's transform or original changes, invalidate all cache entries involving that layer

- AdaptivePrecisionManager
  - Track pairwise success/failure history
  - Suggest tolerance adjustments and coordinate scaling strategies for unstable boolean ops

- GeometryValidator & Repair
  - validateCutResult(result, min_volume_threshold=1e-14) -> ValidationResult { is_degenerate, volume }
  - repairDegenerate(shape) uses ShapeFix_Shape and ShapeUpgrade tools; if repair fails, return original or empty per policy

Algorithms (summary)
1. resolveIntersections() — full recompute
  - sort layers by (rank desc, region_index desc)
  - for each layer compute transformed_cache lazily when needed (use GeometryBuilder::transformShape)
  - compute Bnd_Box for transformed shapes via BRepBndLib::Add when first required and insert into SpatialIndex
  - initialize final_shape = transformed_shape for all layers
  - for each pair (higher -> lower) produced by spatial queries: if bbox intersects, optionally compute cheap volume test (BRepGProp::VolumeProperties on Common) or BRepAlgoAPI_Common in fast mode; if positive, perform GeometryBuilder::intrusiveCut(lower, higher)
  - after cut, validate result; if below min_volume_threshold (1e-14) mark final_shape empty and record removal
  - update DependencyGraph and IntersectionCache

2. recomputeFromOriginals(changed_indices) — incremental
  - For each changed index: update transformed_cache (transform original -> transformed) and recompute bbox; update SpatialIndex
  - Query SpatialIndex for neighbors and union into candidate set
  - Expand candidate set via DependencyGraph.getAffectedLayers
  - Sort affected indices by precedence and call a selective resolution that performs cuts only between affected layers (and any higher-ranked layers that cut them), using cached intersection results where valid

Concurrency model
- Reader/writer protection: std::shared_mutex layers_mutex_ for high-level concurrency; SpatialIndex guarded with its own mutex
- Parallelization: process independent target layers in parallel using a task pool with max threads = 4; ensure two tasks don't write the same target layer final_shape concurrently by partitioning by target index
- Boolean operations stay single-threaded per operation but multiple independent cuts may run concurrently

Caching and shape sharing
- original_shape stored in std::shared_ptr<TopoDS_Shape>
- transformed_cache stored per layer as optional; invalidated when transform changes
- IntersectionCache stores results to avoid repeating expensive boolean ops; respects cache size and evicts via LRU

ValidationReport (structure)
- vector<pair<size_t, ValidationIssue>> layer_issues
- vector<pair<pair<size_t,size_t>, string>> intersection_failures
- double total_volume_before, total_volume_after
- size_t layers_modified
- duration computation_time
- map<string, any> metadata (precision history, cache hits/misses)

Error handling strategy
- On invalid transform: throw std::invalid_argument or return error code via builder methods; record in ValidationReport
- On boolean failure: try adaptive tolerance (up to N attempts, e.g., 3), try ShapeFix repair; if still failing record intersection_failures and skip the cut (leave lower layer unchanged) or mark as uncertain per policy (configurable)
- Degenerate result: if volume < 1e-14 mark layer removed and propagate

Edge cases & mitigations
- Identical shapes at different ranks: deterministic tie-breaker by region_index; if still ambiguous, use insertion order
- Encapsulation (one shape fully inside another): boolean cut will result in empty; treat as removed and update dependents
- Extremely thin layers: adaptive precision and coordinate scaling; record in report if repaired or removed
- Repeated tiny transforms: use TransformValidator to merge very small translations into no-op to avoid churn

Testing and validation (scaffold)
- Unit tests (gtest or project existing test framework): transform invariance, single-pair cut between cubes, complete encapsulation, degenerate-thin-layer
- Integration test: build a small MOSFET stack with a couple of intruding layers, verify volumes and that high-rank layers remain intact
- Performance benchmark: initial resolve time and incremental update time with synthetic N layers (N configurable)

Implementation notes & file plan (suggested changes)
- Add header and source stubs under `include/` and `src/`:
  - include/IntrusiveDeviceBuilder.h (class declaration + public API)
  - src/IntrusiveDeviceBuilder.cpp (main logic skeleton)
  - include/SpatialIndexOCCT.h (OCCT-backed index)
  - src/SpatialIndexOCCT.cpp
  - include/TransformValidator.h + src
  - include/IntersectionCache.h + src
  - include/DependencyGraph.h + src
  - include/GeometryValidator.h + src (repair helpers)
- Update `docs/` with this final design (this file) and optionally add `docs/IMPLEMENTATION_STATUS.md` updates
