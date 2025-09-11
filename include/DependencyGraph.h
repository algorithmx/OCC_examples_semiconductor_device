// DependencyGraph.h
#pragma once

#include <vector>
#include <set>

class DependencyGraph {
public:
    DependencyGraph();
    void addDependency(size_t cutter, size_t target);
    void removeDependency(size_t cutter, size_t target);
    std::vector<size_t> getAffectedLayers(size_t changed_layer) const;

private:
    struct Node { std::set<size_t> cuts_applied_by; std::set<size_t> cuts_applied_to; };
    std::vector<Node> nodes_;
};
