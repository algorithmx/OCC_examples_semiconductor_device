// GeometryValidator.h
#pragma once

#include "OpenCASCADEHeaders.h"

struct ValidationResult {
    bool is_degenerate = false;
    double volume = 0.0;
    std::string message;
};

class GeometryValidator {
public:
    static ValidationResult validateCutResult(const TopoDS_Solid& result, double min_volume_threshold = 1e-14);
    static TopoDS_Solid repairDegenerate(const TopoDS_Solid& shape);
};
