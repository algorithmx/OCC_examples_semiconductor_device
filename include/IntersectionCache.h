// IntersectionCache.h
#pragma once

#include <unordered_map>
#include <list>
#include <mutex>
#include "OpenCASCADEHeaders.h"

class IntersectionCache {
public:
    IntersectionCache(size_t max_entries = 1000);
    ~IntersectionCache();

    struct Key {
        size_t a, b;
        std::uint64_t ha, hb;
        bool operator==(Key const& o) const noexcept { return a==o.a && b==o.b && ha==o.ha && hb==o.hb; }
    };

    struct Entry {
        Key key;
        TopoDS_Solid result;
    };

    bool tryGet(const Key& key, TopoDS_Solid& out) const;
    void put(const Key& key, const TopoDS_Solid& value);
    void invalidateLayer(size_t layer_index);

private:
    mutable std::mutex mtx_;
    size_t max_entries_;
    // ... LRU structures
};
