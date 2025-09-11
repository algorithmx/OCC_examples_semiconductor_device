// GeometryValidator.cpp
#include "GeometryValidator.h"
#include <cmath>

ValidationResult GeometryValidator::validateCutResult(const TopoDS_Solid& result, double min_volume_threshold) {
    ValidationResult r;
    // stub: assume valid and compute zero volume
    r.volume = 0.0;
    r.is_degenerate = (r.volume < min_volume_threshold);
    if (r.is_degenerate) r.message = "Volume below threshold";
    return r;
}

TopoDS_Solid GeometryValidator::repairDegenerate(const TopoDS_Solid& shape) {
    (void)shape;
    return shape;
}
