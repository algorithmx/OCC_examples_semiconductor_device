// DependencyGraph.cpp
#include "DependencyGraph.h"

DependencyGraph::DependencyGraph() {}

void DependencyGraph::addDependency(size_t cutter, size_t target) {
    if (cutter >= nodes_.size() || target >= nodes_.size()) return;
    nodes_[target].cuts_applied_by.insert(cutter);
    nodes_[cutter].cuts_applied_to.insert(target);
}

void DependencyGraph::removeDependency(size_t cutter, size_t target) {
    if (cutter >= nodes_.size() || target >= nodes_.size()) return;
    nodes_[target].cuts_applied_by.erase(cutter);
    nodes_[cutter].cuts_applied_to.erase(target);
}

std::vector<size_t> DependencyGraph::getAffectedLayers(size_t changed_layer) const {
    std::vector<size_t> res;
    if (changed_layer >= nodes_.size()) return res;
    // simple BFS over outgoing edges
    std::set<size_t> seen;
    std::vector<size_t> work{changed_layer};
    while (!work.empty()) {
        size_t n = work.back(); work.pop_back();
        if (seen.insert(n).second) {
            res.push_back(n);
            for (size_t t : nodes_[n].cuts_applied_to) work.push_back(t);
        }
    }
    return res;
}
