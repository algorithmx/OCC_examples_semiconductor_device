// SpatialIndexOCCT.cpp
#include "SpatialIndexOCCT.h"

SpatialIndexOCCT::SpatialIndexOCCT() {}
SpatialIndexOCCT::~SpatialIndexOCCT() {}

void SpatialIndexOCCT::insert(size_t layer_index, const Bnd_Box& bbox) {
    std::lock_guard lock(mtx_);
    (void)layer_index; (void)bbox;
}

void SpatialIndexOCCT::update(size_t layer_index, const Bnd_Box& old_bbox, const Bnd_Box& new_bbox) {
    std::lock_guard lock(mtx_);
    (void)layer_index; (void)old_bbox; (void)new_bbox;
}

std::vector<size_t> SpatialIndexOCCT::query(const Bnd_Box& bbox) const {
    std::lock_guard lock(mtx_);
    (void)bbox;
    return {};
}

void SpatialIndexOCCT::remove(size_t layer_index) {
    std::lock_guard lock(mtx_);
    (void)layer_index;
}
