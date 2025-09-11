// SpatialIndexOCCT.h
#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include "OpenCASCADEHeaders.h"

class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;
    virtual void insert(size_t layer_index, const Bnd_Box& bbox) = 0;
    virtual void update(size_t layer_index, const Bnd_Box& old_bbox, const Bnd_Box& new_bbox) = 0;
    virtual std::vector<size_t> query(const Bnd_Box& bbox) const = 0;
    virtual void remove(size_t layer_index) = 0;
};

class SpatialIndexOCCT : public ISpatialIndex {
public:
    SpatialIndexOCCT();
    ~SpatialIndexOCCT() override;
    void insert(size_t layer_index, const Bnd_Box& bbox) override;
    void update(size_t layer_index, const Bnd_Box& old_bbox, const Bnd_Box& new_bbox) override;
    std::vector<size_t> query(const Bnd_Box& bbox) const override;
    void remove(size_t layer_index) override;

private:
    mutable std::mutex mtx_;
    // ... internal storage
};
