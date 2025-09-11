// IntrusiveDeviceBuilder.h
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <shared_mutex>
#include <mutex>

#include "OpenCASCADEHeaders.h"
#include "SemiconductorDevice.h"

// Alias for the device validation result used by the builder
using ValidationReport = SemiconductorDevice::ValidationResult;

struct RankedDeviceLayer {
    std::shared_ptr<TopoDS_Solid> original_shape; // shared, immutable
    int rank = 0;
    int region_index = 0;
    gp_Trsf current_trsf; // pose
    mutable std::optional<TopoDS_Solid> transformed_cache;
    mutable std::uint64_t transformed_cache_hash = 0;
    TopoDS_Solid final_shape; // empty if removed
    double last_volume = 0.0;
    bool is_modified = false;
    std::vector<int> cut_by_ranks;
    Bnd_Box cached_bbox;
};

class IntrusiveDeviceBuilder {
public:
    IntrusiveDeviceBuilder(double tolerance = 1e-7);

    // configuration
    IntrusiveDeviceBuilder& withTolerance(double t);
    IntrusiveDeviceBuilder& setMinVolumeThreshold(double v);
    IntrusiveDeviceBuilder& setMaxThreads(size_t n);
    IntrusiveDeviceBuilder& withCacheSize(size_t n);
    IntrusiveDeviceBuilder& enableShapeSharing(bool enable);

    // layer management
    void addRankedLayer(std::unique_ptr<DeviceLayer> layer, int rank, int region_index);
    void updateLayerTransform(size_t layer_index, const gp_Trsf& trsf);
    void resetLayerToOriginal(size_t layer_index);
    void recomputeFromOriginals(const std::vector<size_t>& changed_indices);

    // processing
    void resolveIntersections();
    SemiconductorDevice buildDevice(const std::string& name);

    // diagnostics
    ValidationReport getLastValidationReport() const;

private:
    mutable std::shared_mutex layers_mutex_;
    std::vector<RankedDeviceLayer> ranked_layers_;
    double geometric_tolerance_;
    double min_volume_threshold_;
    size_t max_threads_ = 4;
    size_t cache_size_ = 1000;
    bool shape_sharing_enabled_ = true;
};
