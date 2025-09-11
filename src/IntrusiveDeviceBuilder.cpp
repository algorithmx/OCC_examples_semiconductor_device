// IntrusiveDeviceBuilder.cpp
#include "IntrusiveDeviceBuilder.h"
#include "TransformValidator.h"
#include "SpatialIndexOCCT.h"
#include "IntersectionCache.h"
#include "DependencyGraph.h"
#include "GeometryValidator.h"

IntrusiveDeviceBuilder::IntrusiveDeviceBuilder(double tolerance)
    : geometric_tolerance_(tolerance), min_volume_threshold_(1e-14) {}

IntrusiveDeviceBuilder& IntrusiveDeviceBuilder::withTolerance(double t) { geometric_tolerance_ = t; return *this; }
IntrusiveDeviceBuilder& IntrusiveDeviceBuilder::setMinVolumeThreshold(double v) { min_volume_threshold_ = v; return *this; }
IntrusiveDeviceBuilder& IntrusiveDeviceBuilder::setMaxThreads(size_t n) { max_threads_ = n; return *this; }
IntrusiveDeviceBuilder& IntrusiveDeviceBuilder::withCacheSize(size_t n) { cache_size_ = n; return *this; }
IntrusiveDeviceBuilder& IntrusiveDeviceBuilder::enableShapeSharing(bool enable) { shape_sharing_enabled_ = enable; return *this; }

void IntrusiveDeviceBuilder::addRankedLayer(std::unique_ptr<DeviceLayer> layer, int rank, int region_index) {
    std::unique_lock lock(layers_mutex_);
    RankedDeviceLayer rdl;
    rdl.rank = rank;
    rdl.region_index = region_index;
    // Keep original solid shared if possible
    rdl.original_shape = std::make_shared<TopoDS_Solid>(layer->getSolid());
    ranked_layers_.push_back(std::move(rdl));
}

void IntrusiveDeviceBuilder::updateLayerTransform(size_t layer_index, const gp_Trsf& trsf) {
    std::unique_lock lock(layers_mutex_);
    if (layer_index >= ranked_layers_.size()) throw std::out_of_range("Layer index out of range");
    auto& l = ranked_layers_[layer_index];
    if (!TransformValidator::isValidTransform(trsf, geometric_tolerance_)) {
        throw std::invalid_argument("Invalid transform");
    }
    l.current_trsf = TransformValidator::sanitizeTransform(trsf);
    l.transformed_cache.reset();
}

void IntrusiveDeviceBuilder::resetLayerToOriginal(size_t layer_index) {
    std::unique_lock lock(layers_mutex_);
    if (layer_index >= ranked_layers_.size()) return;
    auto& l = ranked_layers_[layer_index];
    l.current_trsf = gp_Trsf();
    l.transformed_cache.reset();
}

void IntrusiveDeviceBuilder::recomputeFromOriginals(const std::vector<size_t>& changed_indices) {
    // Stub: mark transformed cache invalid and call full resolve for now
    for (size_t idx : changed_indices) {
        if (idx < ranked_layers_.size()) ranked_layers_[idx].transformed_cache.reset();
    }
    resolveIntersections();
}

void IntrusiveDeviceBuilder::resolveIntersections() {
    // Simplified stub: no actual OCCT ops, just set final_shape = original if possible
    std::shared_lock lock(layers_mutex_);
    for (auto& r : ranked_layers_) {
        if (!r.original_shape) continue;
        // if transformed cache not present, create a trivial cast
        if (!r.transformed_cache.has_value()) {
            // NOTE: real code should apply BRepBuilderAPI_Transform
            TopoDS_Solid s;
            r.transformed_cache = s;
        }
        // assign final shape = transformed
        if (r.transformed_cache.has_value()) r.final_shape = *r.transformed_cache;
    }
}

SemiconductorDevice IntrusiveDeviceBuilder::buildDevice(const std::string& name) {
    SemiconductorDevice dev(name);
    std::shared_lock lock(layers_mutex_);
    for (const auto& r : ranked_layers_) {
        if (r.final_shape.IsNull()) continue;
        // Create a default material based on region index (best-effort)
    MaterialProperties mat = SemiconductorDevice::createStandardSilicon();
    auto layer = std::make_unique<DeviceLayer>(r.final_shape, mat, DeviceRegion::Substrate, "layer");
        dev.addLayer(std::move(layer));
    }
    dev.buildDeviceGeometry();
    return dev;
}

ValidationReport IntrusiveDeviceBuilder::getLastValidationReport() const {
    ValidationReport v;
    v.geometryValid = true;
    v.meshValid = true;
    v.geometryMessage = "No validation performed (stub)";
    v.meshMessage = "No validation performed (stub)";
    return v;
}
