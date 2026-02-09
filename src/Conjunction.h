#pragma once
#include <vector>
#include <cstddef>

#include <glm/glm.hpp>
#include "Sgp4System.h"

struct ConjunctionHit {
    int    otherIdx = -1;    
    double tcaSec   = 0.0;   
    double missKm   = 0.0;    
    double relSpeedKmS = 0.0; 
};

struct ConjunctionParams {
    double horizonSec   = 2.0 * 3600.0; 
    double stepSec      = 20.0;        
    double thresholdKm  = 25.0;         
    int    maxSatsToCheck = 20000;      
    bool   refine = true;               
};

bool computeConjunctionsSelectedVsAll(
    const Sgp4System& sys,
    size_t targetIdx,
    double startSimSec,
    const ConjunctionParams& p,
    std::vector<ConjunctionHit>& outHits
);
