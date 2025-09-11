// TransformValidator.cpp
#include "TransformValidator.h"
#include <cmath>

bool TransformValidator::isValidTransform(const gp_Trsf& trsf, double tol) {
    // Very lightweight checks: avoid NaNs and extreme scales
    // ...real implementation would inspect matrix
    (void)trsf; (void)tol;
    return true;
}

gp_Trsf TransformValidator::sanitizeTransform(const gp_Trsf& trsf) {
    // No-op for stub
    return trsf;
}

bool TransformValidator::causesNumericalInstability(const gp_Trsf& trsf) {
    (void)trsf;
    return false;
}
