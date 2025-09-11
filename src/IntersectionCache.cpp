// IntersectionCache.cpp
#include "IntersectionCache.h"

IntersectionCache::IntersectionCache(size_t max_entries) : max_entries_(max_entries) {}
IntersectionCache::~IntersectionCache() {}

bool IntersectionCache::tryGet(const Key& key, TopoDS_Solid& out) const {
    std::lock_guard lock(mtx_);
    (void)key; (void)out;
    return false;
}

void IntersectionCache::put(const Key& key, const TopoDS_Solid& value) {
    std::lock_guard lock(mtx_);
    (void)key; (void)value;
}

void IntersectionCache::invalidateLayer(size_t layer_index) {
    std::lock_guard lock(mtx_);
    (void)layer_index;
}
