// TransformValidator.h
#pragma once

#include "OpenCASCADEHeaders.h"

class TransformValidator {
public:
    static bool isValidTransform(const gp_Trsf& trsf, double tol = 1e-7);
    static gp_Trsf sanitizeTransform(const gp_Trsf& trsf);
    static bool causesNumericalInstability(const gp_Trsf& trsf);
};
